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
    const QString askpassPath = toolpath::siblingTool("SpecStudioAskPass");
    env.insert("GIT_ASKPASS", askpassPath);
    env.insert("SPECSTUDIO_GIT_USERNAME", username);
    env.insert("SPECSTUDIO_GIT_PASSWORD", password);

    // Disable any machine-wide credential helper (e.g. Git Credential Manager,
    // which Git for Windows installs by default) for just this invocation, so
    // it can't intercept the prompt before our askpass helper runs and pop up
    // its own UI or use its own cached credentials.
    env.insert("GIT_CONFIG_COUNT", "1");
    env.insert("GIT_CONFIG_KEY_0", "credential.helper");
    env.insert("GIT_CONFIG_VALUE_0", "");

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
    if (ok) *ok = success;
    return out;
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

bool GitClient::fetch(const QString& remote)
{
    bool ok = false;
    runGit({"fetch", remote}, &ok);
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

QString GitClient::conflictDiff(const QString& relativeFilePath)
{
    return runGit({"diff", "--", relativeFilePath});
}
