#include "NewProjectDialog.h"
#include "../../model/Solution.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QVBoxLayout>

NewProjectDialog::NewProjectDialog(Solution*      currentSolution,
                                   const QString& defaultLocation,
                                   QWidget*       parent)
    : QDialog(parent)
    , m_defaultLocation(defaultLocation)
    , m_currentSolution(currentSolution)
{
    setWindowTitle(tr("New Project"));
    setMinimumWidth(500);

    // ── Project name ──────────────────────────────────────────────────────────
    m_projectNameEdit = new QLineEdit(this);
    auto* topForm = new QFormLayout();
    topForm->addRow(tr("Project name:"), m_projectNameEdit);

    // ── Top-level choice ──────────────────────────────────────────────────────
    m_standaloneBtn    = new QRadioButton(tr("Standalone project (no solution)"), this);
    m_partOfSolutionBtn = new QRadioButton(tr("Part of a solution"), this);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_standaloneBtn);
    modeGroup->addButton(m_partOfSolutionBtn);

    // ── Standalone sub-panel ──────────────────────────────────────────────────
    m_standaloneGroup = new QGroupBox(this);
    m_standaloneGroup->setFlat(true);
    m_standaloneFolderEdit = new QLineEdit(m_standaloneGroup);
    m_standaloneBrowseBtn  = new QPushButton(tr("Browse..."), m_standaloneGroup);

    auto* saFolderRow = new QHBoxLayout();
    saFolderRow->addWidget(m_standaloneFolderEdit);
    saFolderRow->addWidget(m_standaloneBrowseBtn);
    auto* saForm = new QFormLayout(m_standaloneGroup);
    saForm->addRow(tr("Location:"), saFolderRow);

    // ── Solution sub-panel ────────────────────────────────────────────────────
    m_solutionGroup = new QGroupBox(this);
    m_solutionGroup->setFlat(true);
    auto* solLayout = new QVBoxLayout(m_solutionGroup);

    if (currentSolution) {
        m_addToCurrentBtn = new QRadioButton(tr("Add to current solution"), m_solutionGroup);
        m_currentSolLabel = new QLabel(
            QStringLiteral("  %1  [%2]").arg(currentSolution->name(), currentSolution->rootPath()),
            m_solutionGroup);

        auto* subGroup = new QButtonGroup(this);

        m_newSolutionBtn = new QRadioButton(tr("Create new solution"), m_solutionGroup);
        subGroup->addButton(m_addToCurrentBtn);
        subGroup->addButton(m_newSolutionBtn);
        m_addToCurrentBtn->setChecked(true);

        solLayout->addWidget(m_addToCurrentBtn);
        solLayout->addWidget(m_currentSolLabel);
        solLayout->addWidget(m_newSolutionBtn);
    } else {
        m_newSolutionBtn = new QRadioButton(tr("Create new solution"), m_solutionGroup);
        m_newSolutionBtn->setChecked(true);
        m_newSolutionBtn->setVisible(false); // only option, no need to show radio
        solLayout->addWidget(m_newSolutionBtn);
    }

    m_solNameEdit   = new QLineEdit(m_solutionGroup);
    m_solFolderEdit = new QLineEdit(m_solutionGroup);
    m_solBrowseBtn  = new QPushButton(tr("Browse..."), m_solutionGroup);

    auto* solFolderRow = new QHBoxLayout();
    solFolderRow->addWidget(m_solFolderEdit);
    solFolderRow->addWidget(m_solBrowseBtn);

    auto* newSolForm = new QFormLayout();
    newSolForm->addRow(tr("Solution name:"), m_solNameEdit);
    newSolForm->addRow(tr("Location:"),      solFolderRow);
    solLayout->addLayout(newSolForm);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topForm);
    mainLayout->addWidget(m_standaloneBtn);
    mainLayout->addWidget(m_standaloneGroup);
    mainLayout->addWidget(m_partOfSolutionBtn);
    mainLayout->addWidget(m_solutionGroup);
    mainLayout->addWidget(buttons);

    // ── Connections ───────────────────────────────────────────────────────────

    // Project name → auto-fill downstream fields
    connect(m_projectNameEdit, &QLineEdit::textChanged, this, [this](const QString&) {
        syncStandaloneFolder();
        if (!m_solNameEdited) {
            m_solNameEdit->setText(m_projectNameEdit->text().trimmed());
            syncSolutionFolder();
        }
        updateOk();
    });

    // Standalone folder
    connect(m_standaloneFolderEdit, &QLineEdit::textChanged,  this, [this] { updateOk(); });
    connect(m_standaloneFolderEdit, &QLineEdit::textEdited,   this, [this] { m_standaloneFolderEdited = true; });
    connect(m_standaloneBrowseBtn,  &QPushButton::clicked,    this, [this] {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Project Location"), m_standaloneFolderEdit->text());
        if (!dir.isEmpty()) {
            const QString name = m_projectNameEdit->text().trimmed();
            m_standaloneFolderEdit->setText(name.isEmpty() ? dir : dir + "/" + name);
            m_standaloneFolderEdited = true;
        }
    });

    // Solution name → auto-fill solution folder
    connect(m_solNameEdit, &QLineEdit::textChanged, this, [this] { syncSolutionFolder(); updateOk(); });
    connect(m_solNameEdit, &QLineEdit::textEdited,  this, [this] { m_solNameEdited = true; });
    connect(m_solFolderEdit, &QLineEdit::textChanged, this, [this] { updateOk(); });
    connect(m_solFolderEdit, &QLineEdit::textEdited,  this, [this] { m_solFolderEdited = true; });
    connect(m_solBrowseBtn,  &QPushButton::clicked,   this, [this] {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Solution Location"), m_solFolderEdit->text());
        if (!dir.isEmpty()) {
            const QString name = m_solNameEdit->text().trimmed();
            m_solFolderEdit->setText(name.isEmpty() ? dir : dir + "/" + name);
            m_solFolderEdited = true;
        }
    });

    // Top-level mode toggle
    connect(m_standaloneBtn,    &QRadioButton::toggled, this, [this] { updatePanels(); updateOk(); });
    connect(m_partOfSolutionBtn, &QRadioButton::toggled, this, [this] { updatePanels(); updateOk(); });

    // Sub-choice within "Part of solution"
    if (m_addToCurrentBtn)
        connect(m_addToCurrentBtn, &QRadioButton::toggled, this, [this] { updatePanels(); updateOk(); });
    if (m_newSolutionBtn)
        connect(m_newSolutionBtn, &QRadioButton::toggled, this, [this] { updatePanels(); updateOk(); });

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Default selection
    if (currentSolution)
        m_partOfSolutionBtn->setChecked(true);
    else
        m_standaloneBtn->setChecked(true);

    updatePanels();
    updateOk();
    m_projectNameEdit->setFocus();
}

void NewProjectDialog::syncStandaloneFolder()
{
    if (m_standaloneFolderEdited) return;
    const QString name = m_projectNameEdit->text().trimmed();
    QString safe = name;
    safe.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    m_standaloneFolderEdit->setText(
        safe.isEmpty() ? m_defaultLocation : m_defaultLocation + "/" + safe);
}

void NewProjectDialog::syncSolutionFolder()
{
    if (m_solFolderEdited) return;
    const QString name = m_solNameEdit->text().trimmed();
    QString safe = name;
    safe.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    m_solFolderEdit->setText(
        safe.isEmpty() ? m_defaultLocation : m_defaultLocation + "/" + safe);
}

void NewProjectDialog::updatePanels()
{
    const bool standalone = m_standaloneBtn->isChecked();
    m_standaloneGroup->setVisible(standalone);
    m_solutionGroup->setVisible(!standalone);

    if (!standalone && m_addToCurrentBtn && m_newSolutionBtn) {
        const bool creating = m_newSolutionBtn->isChecked();
        m_solNameEdit->setEnabled(creating);
        m_solFolderEdit->setEnabled(creating);
        m_solBrowseBtn->setEnabled(creating);
    }
}

void NewProjectDialog::updateOk()
{
    const bool projOk = !m_projectNameEdit->text().trimmed().isEmpty();
    bool locOk = true;

    if (m_standaloneBtn->isChecked()) {
        locOk = !m_standaloneFolderEdit->text().trimmed().isEmpty();
    } else {
        if (!m_addToCurrentBtn || m_newSolutionBtn->isChecked()) {
            locOk = !m_solNameEdit->text().trimmed().isEmpty() &&
                    !m_solFolderEdit->text().trimmed().isEmpty();
        }
    }
    m_okBtn->setEnabled(projOk && locOk);
}

QString NewProjectDialog::projectName()          const { return m_projectNameEdit->text().trimmed(); }
bool    NewProjectDialog::isStandalone()         const { return m_standaloneBtn->isChecked(); }
QString NewProjectDialog::standaloneLocation()   const { return m_standaloneFolderEdit->text().trimmed(); }
bool    NewProjectDialog::addToCurrentSolution() const { return m_currentSolution && m_addToCurrentBtn && m_addToCurrentBtn->isChecked(); }
QString NewProjectDialog::newSolutionName()      const { return m_solNameEdit->text().trimmed(); }
QString NewProjectDialog::newSolutionFolder()    const { return m_solFolderEdit->text().trimmed(); }
