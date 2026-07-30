#include "RepositorySettingsDialog.h"
#include "../../app/AppSettings.h"
#include "../../model/Solution.h"
#include "../../git/SecretStore.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

RepositorySettingsDialog::RepositorySettingsDialog(AppSettings* settings, Solution* solution, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_solution(solution)
{
    setWindowTitle(tr("Repository Settings"));
    setMinimumWidth(420);
    buildUi();
    loadValues();
}

void RepositorySettingsDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    m_url    = new QLineEdit(this);
    m_branch = new QLineEdit(this);
    m_user   = new QLineEdit(this);
    // A token, not a password. The field used to be labelled "Password", which
    // invited exactly the value that cannot work: GitHub stopped accepting
    // account passwords for git in August 2021, and one saved here failed every
    // operation with a message that named neither the cause nor the fix.
    m_token  = new QLineEdit(this);
    m_token->setEchoMode(QLineEdit::Password);
    m_token->setPlaceholderText(tr("usually not needed"));
    form->addRow(tr("Remote URL:"), m_url);
    form->addRow(tr("Branch:"),     m_branch);
    form->addRow(tr("User name:"),  m_user);
    form->addRow(tr("Access token:"), m_token);
    layout->addLayout(form);

    // One line. The full story -- when a token is needed, which host to get it
    // from, what scope, and the SSO trap -- is in Git Setup.md, which ships.
    auto* note = new QLabel(
        tr("Usually left empty: your git credential helper signs you in. "
           "Stored in %1. See \"Git Setup.md\".").arg(SecretStore::backendName()),
        this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveValues(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (!m_solution) setEnabled(false);
}

void RepositorySettingsDialog::loadValues()
{
    if (!m_solution || !m_settings) return;
    const QString root = m_solution->rootPath();
    m_url->setText(m_settings->solutionGitRemoteUrl(root));
    m_branch->setText(m_settings->solutionGitBranch(root));
    m_user->setText(m_settings->solutionGitUser(root));
    m_token->setText(m_settings->solutionGitPassword(root));
}

void RepositorySettingsDialog::saveValues()
{
    if (!m_solution || !m_settings) return;
    const QString root = m_solution->rootPath();
    m_settings->setSolutionGitRemoteUrl(root, m_url->text().trimmed());
    m_settings->setSolutionGitBranch(root,    m_branch->text().trimmed());
    m_settings->setSolutionGitUser(root,      m_user->text().trimmed());
    m_settings->setSolutionGitPassword(root,  m_token->text().trimmed());
}
