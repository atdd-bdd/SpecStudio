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
    if (!commitAll(message)) return false;

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
