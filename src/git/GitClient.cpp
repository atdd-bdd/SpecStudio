#include "GitClient.h"

#include <QProcess>

GitClient::GitClient(const QString& repoPath, QObject* parent)
    : QObject(parent)
    , m_repoPath(repoPath)
{}

QString GitClient::runGit(const QStringList& args, bool* ok)
{
    QProcess proc;
    proc.setWorkingDirectory(m_repoPath);
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

bool GitClient::resolveOurs(const QString& relativeFilePath)
{
    bool ok = false;
    runGit({"checkout", "--ours",   "--", relativeFilePath}, &ok); if (!ok) return false;
    runGit({"add", "--",            relativeFilePath},        &ok);
    return ok;
}

bool GitClient::resolveTheirs(const QString& relativeFilePath)
{
    bool ok = false;
    runGit({"checkout", "--theirs", "--", relativeFilePath}, &ok); if (!ok) return false;
    runGit({"add", "--",            relativeFilePath},        &ok);
    return ok;
}

bool GitClient::abortMerge()
{
    bool ok = false;
    runGit({"merge", "--abort"}, &ok);
    return ok;
}

bool GitClient::finishMerge(const QString& message)
{
    bool ok = false;
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
