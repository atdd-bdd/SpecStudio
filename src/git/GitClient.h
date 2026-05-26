#pragma once

#include <QObject>
#include <QStringList>

class GitClient : public QObject
{
    Q_OBJECT

public:
    explicit GitClient(const QString& repoPath, QObject* parent = nullptr);

    bool        hasUncommittedChanges();
    bool        commitAll(const QString& message);
    bool        commitAndPush(const QString& message,
                              const QString& remote,
                              const QString& branch);
    bool        fetch(const QString& remote = "origin");
    QString     currentBranch();
    QStringList status();

signals:
    void outputReady(const QString& text);
    void errorOccurred(const QString& text);

private:
    QString runGit(const QStringList& args, bool* ok = nullptr);

    QString m_repoPath;
};
