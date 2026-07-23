#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

// File > Clone an Existing Solution... — asks for the remote URL and local
// folder to clone into. Username/PAT are optional (only needed for private
// repos); when given, they're applied to the one-shot `git clone` process via
// GitClient::applyCredentialEnv rather than embedded in the URL.
class CloneSolutionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CloneSolutionDialog(QWidget* parent = nullptr);

    QString remoteUrl() const;
    QString localDirectory() const;
    QString username() const;
    QString personalAccessToken() const;

private:
    QLineEdit*   m_urlEdit    = nullptr;
    QLineEdit*   m_dirEdit    = nullptr;
    QPushButton* m_browseBtn  = nullptr;
    QLineEdit*   m_userEdit   = nullptr;
    QLineEdit*   m_tokenEdit  = nullptr;
};
