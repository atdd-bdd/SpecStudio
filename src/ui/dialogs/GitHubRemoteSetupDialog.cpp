#include "GitHubRemoteSetupDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

GitHubRemoteSetupDialog::GitHubRemoteSetupDialog(const QString& defaultRepoName,
                                                  const QString& defaultHost,
                                                  QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Set Up GitHub Sharing"));
    setMinimumWidth(420);

    auto* form = new QFormLayout();
    m_hostEdit  = new QLineEdit(defaultHost, this);
    m_nameEdit  = new QLineEdit(defaultRepoName, this);
    m_userEdit  = new QLineEdit(this);
    m_tokenEdit = new QLineEdit(this);
    m_tokenEdit->setEchoMode(QLineEdit::Password);

    form->addRow(tr("Host:"), m_hostEdit);
    form->addRow(tr("Repository name:"), m_nameEdit);
    form->addRow(tr("Username:"), m_userEdit);
    form->addRow(tr("Personal Access Token:"), m_tokenEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString GitHubRemoteSetupDialog::host() const { return m_hostEdit->text().trimmed(); }
QString GitHubRemoteSetupDialog::repoName() const { return m_nameEdit->text().trimmed(); }
QString GitHubRemoteSetupDialog::username() const { return m_userEdit->text().trimmed(); }
QString GitHubRemoteSetupDialog::personalAccessToken() const { return m_tokenEdit->text(); }
