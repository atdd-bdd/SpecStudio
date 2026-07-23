#include "RepositorySettingsDialog.h"
#include "../../app/AppSettings.h"
#include "../../model/Solution.h"
#include "../../model/Project.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QStackedWidget>
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

    layout->addWidget(new QLabel(tr("How should git repositories work for this solution?"), this));

    m_separateRadio = new QRadioButton(tr("Separate — each project has its own repository"), this);
    m_combinedRadio = new QRadioButton(tr("Combined — every project shares one repository"), this);
    auto* group = new QButtonGroup(this);
    group->addButton(m_separateRadio);
    group->addButton(m_combinedRadio);
    layout->addWidget(m_separateRadio);
    layout->addWidget(m_combinedRadio);

    m_stack = new QStackedWidget(this);

    // ---- Separate page: pick a project, edit its own remote ----
    auto* sepPage = new QWidget(m_stack);
    auto* sepForm = new QFormLayout(sepPage);
    m_projectPicker = new QComboBox(sepPage);
    if (m_solution)
        for (Project* p : m_solution->projects())
            m_projectPicker->addItem(p->name(), QVariant::fromValue(static_cast<void*>(p)));
    m_separateUrl    = new QLineEdit(sepPage);
    m_separateBranch = new QLineEdit(sepPage);
    m_separateUser   = new QLineEdit(sepPage);
    m_separatePass   = new QLineEdit(sepPage);
    m_separatePass->setEchoMode(QLineEdit::Password);
    sepForm->addRow(tr("Project:"),   m_projectPicker);
    sepForm->addRow(tr("Remote URL:"), m_separateUrl);
    sepForm->addRow(tr("Branch:"),     m_separateBranch);
    sepForm->addRow(tr("User name:"),  m_separateUser);
    sepForm->addRow(tr("Password:"),   m_separatePass);
    m_stack->addWidget(sepPage);

    // ---- Combined page: one shared remote for the whole solution ----
    auto* combPage = new QWidget(m_stack);
    auto* combForm = new QFormLayout(combPage);
    m_combinedUrl    = new QLineEdit(combPage);
    m_combinedBranch = new QLineEdit(combPage);
    m_combinedUser   = new QLineEdit(combPage);
    m_combinedPass   = new QLineEdit(combPage);
    m_combinedPass->setEchoMode(QLineEdit::Password);
    combForm->addRow(tr("Remote URL:"), m_combinedUrl);
    combForm->addRow(tr("Branch:"),     m_combinedBranch);
    combForm->addRow(tr("User name:"),  m_combinedUser);
    combForm->addRow(tr("Password:"),   m_combinedPass);
    m_stack->addWidget(combPage);

    layout->addWidget(m_stack);

    connect(m_separateRadio, &QRadioButton::toggled, this, [this](bool on) {
        if (on) m_stack->setCurrentIndex(0);
    });
    connect(m_combinedRadio, &QRadioButton::toggled, this, [this](bool on) {
        if (on) m_stack->setCurrentIndex(1);
    });
    connect(m_projectPicker, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        saveProjectFields(m_pickedProject);
        m_pickedProject = idx >= 0
            ? static_cast<Project*>(m_projectPicker->itemData(idx).value<void*>())
            : nullptr;
        loadProjectFields(m_pickedProject);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveValues(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (!m_solution) setEnabled(false);
}

void RepositorySettingsDialog::loadProjectFields(Project* proj)
{
    if (!proj || !m_settings) {
        m_separateUrl->clear();
        m_separateBranch->clear();
        m_separateUser->clear();
        m_separatePass->clear();
        return;
    }
    const QString root = proj->rootPath();
    m_separateUrl->setText(m_settings->gitRemoteUrl(root));
    m_separateBranch->setText(m_settings->gitBranch(root));
    m_separateUser->setText(m_settings->gitUser(root));
    m_separatePass->setText(m_settings->gitPassword(root));
}

void RepositorySettingsDialog::saveProjectFields(Project* proj)
{
    if (!proj || !m_settings) return;
    const QString root = proj->rootPath();
    m_settings->setGitRemoteUrl(root, m_separateUrl->text().trimmed());
    m_settings->setGitBranch(root,    m_separateBranch->text().trimmed());
    m_settings->setGitUser(root,      m_separateUser->text().trimmed());
    m_settings->setGitPassword(root,  m_separatePass->text());
}

void RepositorySettingsDialog::loadValues()
{
    if (!m_solution) return;

    const bool combined = m_solution->repoScope() == Solution::RepoScope::Combined;
    m_combinedRadio->setChecked(combined);
    m_separateRadio->setChecked(!combined);
    m_stack->setCurrentIndex(combined ? 1 : 0);

    if (m_settings) {
        const QString root = m_solution->rootPath();
        m_combinedUrl->setText(m_settings->solutionGitRemoteUrl(root));
        m_combinedBranch->setText(m_settings->solutionGitBranch(root));
        m_combinedUser->setText(m_settings->solutionGitUser(root));
        m_combinedPass->setText(m_settings->solutionGitPassword(root));
    }

    if (m_projectPicker->count() > 0) {
        m_pickedProject = static_cast<Project*>(m_projectPicker->itemData(0).value<void*>());
        loadProjectFields(m_pickedProject);
    }
}

void RepositorySettingsDialog::saveValues()
{
    if (!m_solution) return;

    m_solution->setRepoScope(m_combinedRadio->isChecked()
        ? Solution::RepoScope::Combined : Solution::RepoScope::Separate);

    if (m_settings) {
        const QString root = m_solution->rootPath();
        m_settings->setSolutionGitRemoteUrl(root, m_combinedUrl->text().trimmed());
        m_settings->setSolutionGitBranch(root,    m_combinedBranch->text().trimmed());
        m_settings->setSolutionGitUser(root,      m_combinedUser->text().trimmed());
        m_settings->setSolutionGitPassword(root,  m_combinedPass->text());
    }

    saveProjectFields(m_pickedProject);
}
