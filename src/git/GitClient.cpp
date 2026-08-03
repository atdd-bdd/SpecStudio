#include "GitClient.h"
#include "../ToolPath.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>

GitClient::GitClient(const QString& repoPath, QObject* parent)
    : QObject(parent)
    , m_repoPath(repoPath)
{}

void GitClient::setCredentials(const QString& username, const QString& password)
{
    m_username = username;
    m_password = password;
}

void GitClient::applyCredentialEnv(QProcess& proc, const QString& username, const QString& password)
{
    if (username.isEmpty() && password.isEmpty())
        return;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Point git at our own askpass helper (a sibling executable of
    // SpecStudio) so it never needs an interactive terminal.
    const QString askpassPath = toolpath::siblingTool("AlignThreeAskPass");
    env.insert("GIT_ASKPASS", askpassPath);
    env.insert("SPECSTUDIO_GIT_USERNAME", username);
    env.insert("SPECSTUDIO_GIT_PASSWORD", password);

    // The machine's own credential helper is deliberately left alone.
    //
    // This used to force credential.helper to empty for the duration of the
    // call, so that Git Credential Manager could not answer before the askpass
    // helper did. The effect was that a working, properly stored credential was
    // suppressed in favour of whatever SpecStudio had saved -- and when that
    // saved value was a GitHub account password rather than a token, every
    // operation failed with "Password authentication is not supported for Git
    // operations", while the same push from a terminal succeeded. That is a bad
    // trade: the platform helper keeps its secret in the OS credential store,
    // can refresh an expired token, and is what the user has already set up.
    //
    // Precedence is now the sensible way round. git consults the credential
    // helper first and only calls GIT_ASKPASS if the helper has nothing, so a
    // credential saved in SpecStudio still works as a fallback.
    proc.setProcessEnvironment(env);
}

QString GitClient::runGit(const QStringList& args, bool* ok)
{
    QProcess proc;
    proc.setWorkingDirectory(m_repoPath);
    applyCredentialEnv(proc, m_username, m_password);
    proc.start("git", args);

    if (!proc.waitForFinished(30000)) {
        if (ok) *ok = false;
        emit errorOccurred(tr("git timed out"));
        return {};
    }

    QString out    = QString::fromUtf8(proc.readAllStandardOutput());
    QString errOut = QString::fromUtf8(proc.readAllStandardError());

    if (!out.isEmpty())    emit outputReady(out);
    if (!errOut.isEmpty()) emit outputReady(errOut);

    bool success = (proc.exitCode() == 0);
    if (!success)
        explainAuthFailure(errOut + out);
    if (ok) *ok = success;
    return out;
}

// git's own message for a rejected credential says what happened but not what to
// do about it, and the most common cause on a fresh machine -- no credential
// helper configured, so nothing can answer the prompt -- is invisible in it.
// Say the useful part instead of leaving the user with "Authentication failed".
void GitClient::explainAuthFailure(const QString& output)
{
    static const QStringList markers = {
        QStringLiteral("Authentication failed"),
        QStringLiteral("could not read Username"),
        QStringLiteral("could not read Password"),
        QStringLiteral("Invalid username or token"),
        QStringLiteral("Password authentication is not supported"),
        QStringLiteral("terminal prompts disabled"),
    };
    bool looksLikeAuth = false;
    for (const QString& m : markers)
        if (output.contains(m, Qt::CaseInsensitive)) { looksLikeAuth = true; break; }
    if (!looksLikeAuth)
        return;

    // Ask git what helper is configured. Deliberately a bare QProcess with no
    // credential environment: this must report the machine's real configuration,
    // not one temporarily overridden for a single call.
    QProcess cfg;
    cfg.setWorkingDirectory(m_repoPath);
    cfg.start("git", {"config", "--get", "credential.helper"});
    cfg.waitForFinished(5000);
    const QString helper = QString::fromUtf8(cfg.readAllStandardOutput()).trimmed();

    QString advice;
    if (helper.isEmpty()) {
        advice = tr(
            "No git credential helper is configured, so nothing can supply your "
            "sign-in details.\n");
#if defined(Q_OS_WIN)
        advice += tr(
            "Git for Windows includes Git Credential Manager. Enable it with:\n"
            "    git config --global credential.helper manager\n"
            "then try again -- a browser window will open for you to sign in once.");
#elif defined(Q_OS_MACOS)
        advice += tr(
            "Install Git Credential Manager (brew install --cask git-credential-manager),\n"
            "or use the Keychain with:\n"
            "    git config --global credential.helper osxkeychain");
#else
        advice += tr(
            "Install Git Credential Manager, or on a desktop with a keyring:\n"
            "    git config --global credential.helper libsecret");
#endif
    } else if (output.contains(QStringLiteral("Password authentication is not supported"),
                               Qt::CaseInsensitive)
            || output.contains(QStringLiteral("Invalid username or token"), Qt::CaseInsensitive)) {
        advice = tr(
            "GitHub stopped accepting account passwords for git in August 2021, so a "
            "password saved here can never work.\n"
            "The '%1' helper is configured; clear the saved entry and let it sign you "
            "in again, or replace the password with a personal access token.").arg(helper);
    } else {
        advice = tr(
            "The '%1' credential helper is configured but its stored sign-in was "
            "rejected -- it may have expired or been revoked.\n"
            "Clear the saved entry for this host and try again.").arg(helper);
    }

    emit outputReady(QStringLiteral("\n") + advice + QStringLiteral("\n"));
}

bool GitClient::addRemote(const QString& name, const QString& url)
{
    bool ok = false;
    runGit({"remote", "add", name, url}, &ok);
    return ok;
}

bool GitClient::setRemoteUrl(const QString& name, const QString& url)
{
    bool ok = false;
    runGit({"remote", "set-url", name, url}, &ok);
    return ok;
}

bool GitClient::hasRemote(const QString& name)
{
    const QString out = runGit({"remote"});
    return out.split('\n', Qt::SkipEmptyParts).contains(name);
}

QString GitClient::remoteUrl(const QString& name)
{
    bool ok = false;
    const QString out = runGit({"config", "--get", QStringLiteral("remote.%1.url").arg(name)}, &ok);
    return ok ? out.trimmed() : QString();
}

bool GitClient::hasUncommittedChanges()
{
    // Stage everything first so we see the true diff
    runGit({"add", "-A"});
    // --cached compares index to HEAD; exit 1 means there are staged changes
    bool ok = false;
    runGit({"diff", "--cached", "--quiet"}, &ok);
    return !ok;  // exit 0 = nothing staged, exit 1 = changes present
}

// Commit only what is under the given paths, leaving everything else alone --
// staged or not.
//
// Needed because a solution's root can be somebody's existing code repository:
// open a Gradle or Maven project, put the specifications in a subfolder, and
// `commitAll`'s `git add -A` would sweep every uncommitted source change into a
// commit labelled "Auto-save" the first time a .spectable was saved.
//
// The pathspec is given to `commit` as well as to `add`, which is what makes the
// guarantee hold: `git commit -- <paths>` commits those paths from the working
// tree and leaves anything else that happened to be staged still staged.
bool GitClient::commitPaths(const QStringList& relativePaths, const QString& message)
{
    if (relativePaths.isEmpty()) return false;

    bool ok = false;
    runGit(QStringList{ "add", "-A", "--" } + relativePaths, &ok);
    if (!ok) return false;

    runGit(QStringList{ "commit", "-m", message, "--" } + relativePaths, &ok);
    return ok;
}

bool GitClient::commitAll(const QString& message)
{
    bool ok = false;
    runGit({"add", "-A"}, &ok);
    if (!ok) return false;
    runGit({"commit", "-m", message}, &ok);
    return ok;
}

bool GitClient::commitAndPush(const QString& message,
                               const QString& remote,
                               const QString& branch)
{
    // Only commit if there is something new to commit (auto-save may have
    // already committed the working-tree changes).
    if (hasUncommittedChanges()) {
        if (!commitAll(message)) return false;
    } else {
        emit outputReady(tr("Nothing new to commit — pushing existing commits.\n"));
    }

    bool ok = false;
    runGit({"push", remote, branch}, &ok);
    return ok;
}

bool GitClient::pull(const QString& remote, const QString& branch)
{
    bool ok = false;
    QStringList args = {"pull", remote};
    if (!branch.isEmpty()) args << branch;
    runGit(args, &ok);
    return ok;
}

bool GitClient::pullRebase(const QString& remote, const QString& branch)
{
    bool ok = false;
    QStringList args = {"pull", "--rebase", remote};
    if (!branch.isEmpty()) args << branch;
    runGit(args, &ok);
    return ok;
}

bool GitClient::isRebaseInProgress() const
{
    return QDir(m_repoPath + "/.git/rebase-merge").exists() ||
           QDir(m_repoPath + "/.git/rebase-apply").exists();
}

QStringList GitClient::conflictedFiles()
{
    // UU prefix in --porcelain means unmerged (both sides modified)
    const QString out = runGit({"status", "--porcelain"});
    QStringList result;
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("UU ") || line.startsWith("AA ") ||
            line.startsWith("DD ") || line.startsWith("AU ") ||
            line.startsWith("UA ")) {
            result << line.mid(3).trimmed();
        }
    }
    return result;
}

// Note: git's --ours/--theirs meaning is SWAPPED during a rebase vs. a merge
// (mid-rebase, --ours is the upstream commit being replayed onto, --theirs is
// the caller's own commit being reapplied) — but "Use Mine"/"Use Theirs" in
// the UI must always mean "my local edit"/"the incoming edit" from the user's
// point of view, regardless of which git operation is actually underway.
bool GitClient::resolveOurs(const QString& relativeFilePath)
{
    const QString flag = isRebaseInProgress() ? "--theirs" : "--ours";
    bool ok = false;
    runGit({"checkout", flag, "--", relativeFilePath}, &ok); if (!ok) return false;
    runGit({"add", "--", relativeFilePath}, &ok);
    return ok;
}

bool GitClient::resolveTheirs(const QString& relativeFilePath)
{
    const QString flag = isRebaseInProgress() ? "--ours" : "--theirs";
    bool ok = false;
    runGit({"checkout", flag, "--", relativeFilePath}, &ok); if (!ok) return false;
    runGit({"add", "--", relativeFilePath}, &ok);
    return ok;
}

bool GitClient::abortMerge()
{
    bool ok = false;
    if (isRebaseInProgress())
        runGit({"rebase", "--abort"}, &ok);
    else
        runGit({"merge", "--abort"}, &ok);
    return ok;
}

bool GitClient::finishMerge(const QString& message)
{
    bool ok = false;
    if (isRebaseInProgress())
        runGit({"rebase", "--continue"}, &ok);
    else
        runGit({"commit", "--no-edit", "-m", message.isEmpty() ? "Merge" : message}, &ok);
    return ok;
}

QString GitClient::currentBranch()
{
    QString out = runGit({"rev-parse", "--abbrev-ref", "HEAD"});
    return out.trimmed();
}

QStringList GitClient::status()
{
    QString out = runGit({"status", "--porcelain"});
    return out.split('\n', Qt::SkipEmptyParts);
}

QString GitClient::diff(const QString& relativeFilePath)
{
    QStringList args = {"diff", "HEAD"};
    if (!relativeFilePath.isEmpty())
        args.append(relativeFilePath);
    return runGit(args);
}

// The commits that actually touched this file, newest first.
//
// The file's own history rather than the repository's, because the newest commit
// overall is very often about something else entirely -- and "the previous
// version of this file" is what someone comparing versions means. --follow keeps
// the history across renames, which matters here since renaming a file is one of
// the things SpecStudio commits for you.
QList<GitClient::FileVersion> GitClient::fileVersions(const QString& relativeFilePath, int limit)
{
    QStringList args = { "log", QStringLiteral("--max-count=%1").arg(limit),
                         "--follow", "--date=iso",
                         // Unit separator between fields: safe in a commit subject,
                         // unlike any punctuation someone might actually type.
                         "--format=%H\x1f%ad\x1f%s" };
    if (!relativeFilePath.isEmpty())
        args << "--" << relativeFilePath;

    QList<FileVersion> out;
    const QString text = runGit(args);
    for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
        const QStringList parts = line.split(QChar(0x1f));
        if (parts.size() < 3) continue;
        out.append({ parts[0].trimmed(), parts[1].trimmed(), parts.mid(2).join(QChar(0x1f)).trimmed() });
    }
    return out;
}

// This file as it is now against how it was at `commit`.
QString GitClient::diffAgainst(const QString& commit, const QString& relativeFilePath)
{
    QStringList args = { "diff", commit };
    if (!relativeFilePath.isEmpty())
        args << "--" << relativeFilePath;
    return runGit(args);
}

// The file's full content as of `commit`. `git show <sha>:<path>` rather than
// reverse-applying a patch: the patch holds only the changed hunks, and what a
// revert needs is the whole file.
QString GitClient::fileAtVersion(const QString& commit, const QString& relativeFilePath)
{
    bool ok = false;
    const QString out = runGit({ "show", commit + QLatin1Char(':') + relativeFilePath }, &ok);
    return ok ? out : QString();
}

// Whether the file differs from HEAD right now, so the caller can say that the
// patch it is showing is not the whole story.
bool GitClient::hasUncommittedChanges(const QString& relativeFilePath)
{
    QStringList args = {"diff", "--quiet", "HEAD"};
    if (!relativeFilePath.isEmpty())
        args << "--" << relativeFilePath;
    bool ok = false;
    runGit(args, &ok);
    return !ok;     // exit 1 means it differs
}

QString GitClient::conflictDiff(const QString& relativeFilePath)
{
    return runGit({"diff", "--", relativeFilePath});
}
