#include "SettingsDialog.h"
#include "../../app/AppSettings.h"
#include "../../model/Project.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(AppSettings* settings,
                                Project*     currentProject,
                                QWidget*     parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_project(currentProject)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(520, 400);

    auto* tabs = new QTabWidget(this);
    buildEditorTab(tabs);
    buildGitTab(tabs);
    buildFeaturexTab(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveValues(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadValues();
}

void SettingsDialog::buildEditorTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* layout = new QVBoxLayout(widget);

    m_editorTable = new QTableWidget(0, 2, widget);
    m_editorTable->setHorizontalHeaderLabels({tr("Extension"), tr("Editor Program")});
    m_editorTable->horizontalHeader()->setStretchLastSection(true);
    m_editorTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Populate known extensions
    const QStringList knownExts = {".feature", ".featurex", ".txt", ".md", ".csv", ".xls", ".xlsx"};
    for (const QString& ext : knownExts) {
        int row = m_editorTable->rowCount();
        m_editorTable->insertRow(row);
        m_editorTable->setItem(row, 0, new QTableWidgetItem(ext));
        m_editorTable->setItem(row, 1, new QTableWidgetItem(
            m_settings ? m_settings->editorForExtension(ext.mid(1)) : QString()));
    }

    auto* browseBtn = new QPushButton(tr("Browse..."), widget);
    connect(browseBtn, &QPushButton::clicked, widget, [this] {
        int row = m_editorTable->currentRow();
        if (row < 0) return;
        QString prog = QFileDialog::getOpenFileName(this, tr("Select Editor Program"));
        if (!prog.isEmpty())
            m_editorTable->item(row, 1)->setText(prog);
    });

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(browseBtn);

    layout->addWidget(new QLabel(tr("Map file extensions to external editors (blank = built-in):"), widget));
    layout->addWidget(m_editorTable);
    layout->addLayout(btnRow);

    tabs->addTab(widget, tr("Editors"));
}

void SettingsDialog::buildGitTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* form   = new QFormLayout(widget);

    m_urlEdit    = new QLineEdit(widget);
    m_branchEdit = new QLineEdit(widget);
    m_userEdit   = new QLineEdit(widget);
    m_passEdit   = new QLineEdit(widget);
    m_passEdit->setEchoMode(QLineEdit::Password);

    form->addRow(tr("Remote URL:"), m_urlEdit);
    form->addRow(tr("Branch:"),     m_branchEdit);
    form->addRow(tr("User name:"),  m_userEdit);
    form->addRow(tr("Password:"),   m_passEdit);

    if (!m_project)
        widget->setEnabled(false);

    tabs->addTab(widget, tr("Git"));
}

void SettingsDialog::buildFeaturexTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* form   = new QFormLayout(widget);

    m_implicitImport = new QCheckBox(tr("Implicitly import Data from other folders"), widget);
    m_uniqueScenario = new QCheckBox(tr("Require unique Scenario names across project"), widget);
    m_uniqueStep     = new QCheckBox(tr("Require unique Step names across project"), widget);

    m_stepScope = new QComboBox(widget);
    m_stepScope->addItem(tr("Same file"),   static_cast<int>(StepScope::File));
    m_stepScope->addItem(tr("Same folder"), static_cast<int>(StepScope::Folder));
    m_stepScope->addItem(tr("Entire project"), static_cast<int>(StepScope::Project));

    form->addRow(m_implicitImport);
    form->addRow(m_uniqueScenario);
    form->addRow(m_uniqueStep);
    form->addRow(tr("Step autocomplete scope:"), m_stepScope);

    if (!m_project)
        widget->setEnabled(false);

    tabs->addTab(widget, tr("FeatureX"));
}

void SettingsDialog::loadValues()
{
    if (!m_settings || !m_project) return;

    const QString& root = m_project->rootPath();
    m_urlEdit->setText(m_settings->gitRemoteUrl(root));
    m_branchEdit->setText(m_settings->gitBranch(root));
    m_userEdit->setText(m_settings->gitUser(root));
    m_passEdit->setText(m_settings->gitPassword(root));

    m_implicitImport->setChecked(m_settings->implicitFolderImport(root));
    m_uniqueScenario->setChecked(m_settings->uniqueScenarioNames(root));
    m_uniqueStep->setChecked(m_settings->uniqueStepNames(root));

    StepScope scope = m_settings->stepSuggestionScope(root);
    for (int i = 0; i < m_stepScope->count(); ++i) {
        if (m_stepScope->itemData(i).toInt() == static_cast<int>(scope)) {
            m_stepScope->setCurrentIndex(i);
            break;
        }
    }
}

void SettingsDialog::saveValues()
{
    if (!m_settings) return;

    // Editor associations
    for (int row = 0; row < m_editorTable->rowCount(); ++row) {
        QString ext  = m_editorTable->item(row, 0)->text().mid(1); // strip leading '.'
        QString prog = m_editorTable->item(row, 1)->text().trimmed();
        m_settings->setEditorForExtension(ext, prog);
    }

    if (!m_project) return;
    const QString& root = m_project->rootPath();

    m_settings->setGitRemoteUrl(root, m_urlEdit->text().trimmed());
    m_settings->setGitBranch(root,    m_branchEdit->text().trimmed());
    m_settings->setGitUser(root,      m_userEdit->text().trimmed());
    m_settings->setGitPassword(root,  m_passEdit->text());

    m_settings->setImplicitFolderImport(root, m_implicitImport->isChecked());
    m_settings->setUniqueScenarioNames(root,  m_uniqueScenario->isChecked());
    m_settings->setUniqueStepNames(root,      m_uniqueStep->isChecked());

    StepScope scope = static_cast<StepScope>(
        m_stepScope->currentData().toInt());
    m_settings->setStepSuggestionScope(root, scope);
}
