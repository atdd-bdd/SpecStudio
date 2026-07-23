#pragma once

#include <QDialog>

class QLineEdit;

// The one-shot "set up GitHub sharing" dialog — shown exactly once per
// solution: either at creation time (New Solution/New Project, GitHub chosen)
// or later via the Git menu's "Share with Git..." action on a solution that
// started out as Shared Files.
class GitHubRemoteSetupDialog : public QDialog
{
    Q_OBJECT

public:
    GitHubRemoteSetupDialog(const QString& defaultRepoName,
                             const QString& defaultHost,
                             QWidget* parent = nullptr);

    QString host() const;
    QString repoName() const;
    QString username() const;
    QString personalAccessToken() const;

private:
    QLineEdit* m_hostEdit  = nullptr;
    QLineEdit* m_nameEdit  = nullptr;
    QLineEdit* m_userEdit  = nullptr;
    QLineEdit* m_tokenEdit = nullptr;
};
