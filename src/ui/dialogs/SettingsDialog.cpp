#include "SettingsDialog.h"
#include "../../app/AppSettings.h"
#include "../../app/ThemeManager.h"
#include "../../model/Project.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
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
    buildGeneralTab(tabs);
    buildEditorTab(tabs);
    buildGitTab(tabs);
    buildFeaturexTab(tabs);
    buildAppearanceTab(tabs);
    buildFontsTab(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveValues(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadValues();
}

void SettingsDialog::buildGeneralTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* form   = new QFormLayout(widget);

    m_defaultLocEdit = new QLineEdit(widget);
    if (m_settings)
        m_defaultLocEdit->setText(m_settings->defaultProjectLocation());

    auto* browseBtn = new QPushButton(tr("Browse..."), widget);
    connect(browseBtn, &QPushButton::clicked, widget, [this] {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Default Project Location"), m_defaultLocEdit->text());
        if (!dir.isEmpty()) m_defaultLocEdit->setText(dir);
    });

    auto* row = new QHBoxLayout();
    row->addWidget(m_defaultLocEdit);
    row->addWidget(browseBtn);

    form->addRow(tr("Default project location:"), row);

    m_autoReloadCheck = new QCheckBox(tr("Automatically reload files changed outside the editor"), widget);
    if (m_settings)
        m_autoReloadCheck->setChecked(m_settings->autoReloadFiles());
    form->addRow(m_autoReloadCheck);

    tabs->addTab(widget, tr("General"));
}

void SettingsDialog::buildEditorTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* layout = new QVBoxLayout(widget);

    m_editorTable = new QTableWidget(0, 2, widget);
    m_editorTable->setHorizontalHeaderLabels({tr("Extension"), tr("Editor Program")});
    m_editorTable->horizontalHeader()->setStretchLastSection(true);
    m_editorTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Populate known extensions; blank entries are pre-filled with the OS default
    const QStringList knownExts = AppSettings::knownExtensions();
    for (const QString& ext : knownExts) {
        const QString bareExt = ext.mid(1); // strip '.'
        QString prog = m_settings ? m_settings->editorForExtension(bareExt) : QString();
        if (prog.isEmpty())
            prog = AppSettings::osDefaultEditor(bareExt);

        int row = m_editorTable->rowCount();
        m_editorTable->insertRow(row);
        m_editorTable->setItem(row, 0, new QTableWidgetItem(ext));
        m_editorTable->setItem(row, 1, new QTableWidgetItem(prog));
    }

    auto* browseBtn   = new QPushButton(tr("Browse..."),        widget);
    auto* osDefaultBtn = new QPushButton(tr("Use OS Defaults"), widget);
    auto* clearBtn    = new QPushButton(tr("Clear"),            widget);

    connect(browseBtn, &QPushButton::clicked, widget, [this] {
        int row = m_editorTable->currentRow();
        if (row < 0) return;
        QString prog = QFileDialog::getOpenFileName(this, tr("Select Editor Program"));
        if (!prog.isEmpty())
            m_editorTable->item(row, 1)->setText(prog);
    });

    // Fill ALL rows with OS defaults (overwriting whatever is there)
    connect(osDefaultBtn, &QPushButton::clicked, widget, [this] {
        for (int row = 0; row < m_editorTable->rowCount(); ++row) {
            auto* extItem  = m_editorTable->item(row, 0);
            auto* progItem = m_editorTable->item(row, 1);
            if (!extItem || !progItem) continue;
            const QString def = AppSettings::osDefaultEditor(extItem->text().mid(1));
            if (!def.isEmpty())
                progItem->setText(def);
        }
    });

    // Clear the selected row's program (revert to built-in)
    connect(clearBtn, &QPushButton::clicked, widget, [this] {
        int row = m_editorTable->currentRow();
        if (row >= 0 && m_editorTable->item(row, 1))
            m_editorTable->item(row, 1)->setText(QString());
    });

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(osDefaultBtn);
    btnRow->addStretch();
    btnRow->addWidget(clearBtn);
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

void SettingsDialog::buildAppearanceTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* layout = new QVBoxLayout(widget);

    m_darkTheme = new QCheckBox(tr("Dark theme"), widget);
    if (m_settings)
        m_darkTheme->setChecked(m_settings->darkTheme());

    // Apply immediately on toggle so the user sees it live
    connect(m_darkTheme, &QCheckBox::toggled, this, [this](bool dark) {
        ThemeManager::apply(qApp, dark);
    });

    layout->addWidget(m_darkTheme);
    layout->addStretch();
    tabs->addTab(widget, tr("Appearance"));
}

void SettingsDialog::buildFontsTab(QTabWidget* tabs)
{
    auto* widget = new QWidget(tabs);
    auto* form   = new QFormLayout(widget);

    auto makeRow = [&](const QString& label,
                       QFontComboBox*& combo, QSpinBox*& spin, QLabel*& preview,
                       const QFont& current)
    {
        combo = new QFontComboBox(widget);
        combo->setCurrentFont(current);
        combo->setMinimumWidth(200);

        spin = new QSpinBox(widget);
        spin->setRange(6, 72);
        spin->setValue(current.pointSize() > 0 ? current.pointSize() : 10);
        spin->setSuffix(" pt");
        spin->setFixedWidth(70);

        preview = new QLabel(tr("AaBbCc 123"), widget);
        preview->setFont(current);
        preview->setMinimumWidth(120);

        auto* row = new QHBoxLayout();
        row->addWidget(combo);
        row->addWidget(spin);
        row->addWidget(preview);
        row->addStretch();
        form->addRow(label, row);

        auto updatePreview = [combo, spin, preview] {
            QFont f = combo->currentFont();
            f.setPointSize(spin->value());
            preview->setFont(f);
        };
        connect(combo, &QFontComboBox::currentFontChanged, widget, updatePreview);
        connect(spin,  qOverload<int>(&QSpinBox::valueChanged), widget, updatePreview);
    };

    const QFont edFont  = m_settings ? m_settings->editorFont() : QFont("Courier New", 12);
    const QFont outFont = m_settings ? m_settings->outputFont() : QFont("Courier New", 10);
    const QFont uiFont  = m_settings ? m_settings->uiFont()     : QApplication::font();

    makeRow(tr("Editor:"), m_editorFontCombo, m_editorFontSize, m_editorPreview, edFont);
    makeRow(tr("Output:"), m_outputFontCombo, m_outputFontSize, m_outputPreview, outFont);
    makeRow(tr("Menus / UI:"), m_uiFontCombo, m_uiFontSize, m_uiPreview, uiFont);

    tabs->addTab(widget, tr("Fonts"));
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

    if (m_defaultLocEdit)
        m_settings->setDefaultProjectLocation(m_defaultLocEdit->text().trimmed());

    if (m_autoReloadCheck)
        m_settings->setAutoReloadFiles(m_autoReloadCheck->isChecked());

    if (m_darkTheme)
        m_settings->setDarkTheme(m_darkTheme->isChecked());

    if (m_editorFontCombo && m_editorFontSize) {
        QFont f = m_editorFontCombo->currentFont();
        f.setPointSize(m_editorFontSize->value());
        m_settings->setEditorFont(f);
    }
    if (m_outputFontCombo && m_outputFontSize) {
        QFont f = m_outputFontCombo->currentFont();
        f.setPointSize(m_outputFontSize->value());
        m_settings->setOutputFont(f);
    }
    if (m_uiFontCombo && m_uiFontSize) {
        QFont f = m_uiFontCombo->currentFont();
        f.setPointSize(m_uiFontSize->value());
        m_settings->setUiFont(f);
    }

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
