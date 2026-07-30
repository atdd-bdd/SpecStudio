#pragma once

#include <QObject>
#include <QStringList>

class QProcess;

class GitClient : public QObject
{
    Q_OBJECT

public:
    explicit GitClient(const QString& repoPath, QObject* parent = nullptr);

    // Credentials used to authenticate over HTTPS via the SpecStudioAskPass
    // GIT_ASKPASS helper (see runGit()) — never written to the remote URL or
    // .git/config. Safe to call again whenever settings change; picked up by
    // the next git invocation.
    void        setCredentials(const QString& username, const QString& password);

    bool        addRemote(const QString& name, const QString& url);
    bool        setRemoteUrl(const QString& name, const QString& url);
    bool        hasRemote(const QString& name = "origin");
    QString     remoteUrl(const QString& name = "origin");

    bool        hasUncommittedChanges();
    bool        commitAll(const QString& message);
    bool        commitAndPush(const QString& message,
                              const QString& remote,
                              const QString& branch);
    bool        pull(const QString& remote = "origin", const QString& branch = {});
    bool        pullRebase(const QString& remote = "origin", const QString& branch = {});
    bool        isRebaseInProgress() const;
    QString     diff(const QString& relativeFilePath = {});
    bool        hasUncommittedChanges(const QString& relativeFilePath);

    // One past version of a file, for choosing what to compare against.
    struct FileVersion {
        QString commit;   // full sha
        QString date;     // ISO, as git formatted it
        QString subject;  // commit message, first line
    };
    // Commits that touched this file, newest first. Follows renames.
    QList<FileVersion> fileVersions(const QString& relativeFilePath, int limit = 50);
    // The file as it is now, against how it was at `commit`.
    QString     diffAgainst(const QString& commit, const QString& relativeFilePath = {});
    // The file's whole content as of that commit.
    QString     fileAtVersion(const QString& commit, const QString& relativeFilePath);
    // Shows what's different for a currently-conflicted path — a plain
    // `git diff` on an unmerged file during an in-progress rebase/merge
    // renders git's own combined view of both sides' changes.
    QString     conflictDiff(const QString& relativeFilePath);
    QStringList conflictedFiles();
    bool        resolveOurs(const QString& relativeFilePath);
    bool        resolveTheirs(const QString& relativeFilePath);
    bool        abortMerge();
    bool        finishMerge(const QString& message);
    QString     currentBranch();
    QStringList status();
    void        setRepoPath(const QString& path) { m_repoPath = path; }
    QString     repoPath() const { return m_repoPath; }

    // Shared helper for callers that need to run a raw `git` QProcess before a
    // repo (and therefore a GitClient) exists yet — e.g. the initial `git
    // clone`. Applies the same GIT_ASKPASS setup that runGit() applies
    // internally for every GitClient-mediated call. The machine's own credential
    // helper is left in place and answers first.
    static void applyCredentialEnv(QProcess& proc, const QString& username, const QString& password);

signals:
    void outputReady(const QString& text);
    void errorOccurred(const QString& text);

private:
    QString runGit(const QStringList& args, bool* ok = nullptr);

    // Turn a rejected credential into advice the user can act on, rather than
    // leaving them with git's "Authentication failed".
    void explainAuthFailure(const QString& output);

    QString m_repoPath;
    QString m_username;
    QString m_password;
};
