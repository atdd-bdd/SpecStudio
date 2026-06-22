#include "SpecConfigEditor.h"
#include "../model/SpecConfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

SpecConfigEditor::SpecConfigEditor(const QString& filePath, QWidget* parent)
    : BaseEditor(filePath, parent)
{
    // ── Outer layout ──────────────────────────────────────────────────────────
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* inner = new QWidget(scroll);
    scroll->setWidget(inner);

    auto* root = new QVBoxLayout(inner);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = new QLabel(tr("SpecTable Project Configuration"), inner);
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Settings used when converting .spectable files to unit tests."), inner);
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    // ── Output group ──────────────────────────────────────────────────────────
    auto* outGroup = new QGroupBox(tr("Output"), inner);
    auto* outForm  = new QFormLayout(outGroup);
    outForm->setRowWrapPolicy(QFormLayout::DontWrapRows);
    outForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_outputDir = new QLineEdit(outGroup);
    m_outputDir->setPlaceholderText(tr("e.g.  generated  or  C:/my/output"));
    auto* browseOut = new QPushButton(tr("Browse…"), outGroup);
    browseOut->setFixedWidth(90);
    auto* outRow = new QHBoxLayout;
    outRow->addWidget(m_outputDir);
    outRow->addWidget(browseOut);
    outForm->addRow(tr("Output directory:"), outRow);

    auto* outHint = new QLabel(
        tr("Relative path is resolved from this config file's location."), outGroup);
    outHint->setStyleSheet("color: gray; font-size: 11px;");
    outForm->addRow(QString(), outHint);

    root->addWidget(outGroup);

    // ── Language / framework group ────────────────────────────────────────────
    auto* langGroup = new QGroupBox(tr("Target Language"), inner);
    auto* langForm  = new QFormLayout(langGroup);
    langForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_language = new QComboBox(langGroup);
    m_language->addItems({ "CSharp", "Java", "Python" });
    langForm->addRow(tr("Language:"), m_language);

    m_framework = new QComboBox(langGroup);
    langForm->addRow(tr("Test framework:"), m_framework);

    m_namespace = new QLineEdit(langGroup);
    m_namespace->setPlaceholderText(tr("e.g.  gherkinexecutor"));
    langForm->addRow(tr("Namespace prefix:"), m_namespace);

    root->addWidget(langGroup);

    // ── Glue group ────────────────────────────────────────────────────────────
    auto* glueGroup = new QGroupBox(tr("Glue File"), inner);
    auto* glueLayout = new QVBoxLayout(glueGroup);

    m_overwriteGlue = new QCheckBox(
        tr("Regenerate glue stubs even if the file already exists"), glueGroup);
    auto* glueHint = new QLabel(
        tr("Leave unchecked to preserve hand-written glue code across builds."),
        glueGroup);
    glueHint->setStyleSheet("color: gray; font-size: 11px;");
    glueLayout->addWidget(m_overwriteGlue);
    glueLayout->addWidget(glueHint);

    root->addWidget(glueGroup);

    // ── Converter path group ──────────────────────────────────────────────────
    auto* convGroup = new QGroupBox(tr("Converter"), inner);
    auto* convForm  = new QFormLayout(convGroup);
    convForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_converterPath  = new QLineEdit(convGroup);
    m_converterPath->setPlaceholderText(tr("Leave blank to auto-detect next to SpecStudio.exe"));
    m_browseConverter = new QPushButton(tr("Browse…"), convGroup);
    m_browseConverter->setFixedWidth(90);
    auto* convRow = new QHBoxLayout;
    convRow->addWidget(m_converterPath);
    convRow->addWidget(m_browseConverter);
    convForm->addRow(tr("SpecTableConverter path:"), convRow);

    root->addWidget(convGroup);

    // ── Status bar ────────────────────────────────────────────────────────────
    m_statusLabel = new QLabel(inner);
    m_statusLabel->setStyleSheet("color: gray;");
    root->addWidget(m_statusLabel);
    root->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_language,  &QComboBox::currentTextChanged,
            this, &SpecConfigEditor::onLanguageChanged);
    connect(m_language,  &QComboBox::currentTextChanged, this, &SpecConfigEditor::markDirty);
    connect(m_framework, &QComboBox::currentTextChanged, this, &SpecConfigEditor::markDirty);
    connect(m_outputDir, &QLineEdit::textChanged, this, &SpecConfigEditor::markDirty);
    connect(m_namespace, &QLineEdit::textChanged, this, &SpecConfigEditor::markDirty);
    connect(m_overwriteGlue, &QCheckBox::toggled, this, &SpecConfigEditor::markDirty);
    connect(m_converterPath, &QLineEdit::textChanged, this, &SpecConfigEditor::markDirty);
    connect(browseOut, &QPushButton::clicked, this, &SpecConfigEditor::onBrowseOutputDir);
    connect(m_browseConverter, &QPushButton::clicked, this, &SpecConfigEditor::onBrowseConverter);

    load(filePath);
}

void SpecConfigEditor::load(const QString& path)
{
    SpecConfig cfg = SpecConfig::load(path);
    populateFromConfig(cfg);
    setDirty(false);
    m_statusLabel->setText(tr("Saved"));
}

bool SpecConfigEditor::save()
{
    SpecConfig cfg = configFromForm();
    if (!cfg.save(filePath())) return false;
    setDirty(false);
    m_statusLabel->setText(tr("Saved"));
    return true;
}

void SpecConfigEditor::onLanguageChanged(const QString& language)
{
    const QStringList fw = SpecConfig::frameworksFor(language);
    const QString prev   = m_framework->currentText();
    m_framework->blockSignals(true);
    m_framework->clear();
    m_framework->addItems(fw);
    int idx = fw.indexOf(prev);
    m_framework->setCurrentIndex(idx >= 0 ? idx : 0);
    m_framework->blockSignals(false);

    // Namespace only applies to CSharp
    m_namespace->setEnabled(language == "CSharp");
}

void SpecConfigEditor::onBrowseOutputDir()
{
    const QString base = QFileInfo(filePath()).dir().absolutePath();
    const QString dir  = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), base);
    if (!dir.isEmpty()) m_outputDir->setText(dir);
}

void SpecConfigEditor::onBrowseConverter()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Locate SpecTableConverter"),
        QString(),
        tr("Executable (*.exe);;All files (*)"));
    if (!path.isEmpty()) m_converterPath->setText(path);
}

void SpecConfigEditor::markDirty()
{
    setDirty(true);
    m_statusLabel->setText(tr("Unsaved changes"));
}

void SpecConfigEditor::populateFromConfig(const SpecConfig& cfg)
{
    // Block signals while populating to avoid marking dirty prematurely
    auto block = [](QObject* o) { o->blockSignals(true); };
    auto unblock = [](QObject* o) { o->blockSignals(false); };

    block(m_language); block(m_framework); block(m_outputDir);
    block(m_namespace); block(m_overwriteGlue); block(m_converterPath);

    m_outputDir->setText(cfg.outputDirectory);

    int langIdx = m_language->findText(cfg.language);
    m_language->setCurrentIndex(langIdx >= 0 ? langIdx : 0);

    // Populate frameworks for this language
    QStringList fw = SpecConfig::frameworksFor(cfg.language);
    m_framework->clear();
    m_framework->addItems(fw);
    int fwIdx = fw.indexOf(cfg.framework);
    m_framework->setCurrentIndex(fwIdx >= 0 ? fwIdx : 0);

    m_namespace->setText(cfg.namespacePrefix);
    m_namespace->setEnabled(cfg.language == "CSharp");
    m_overwriteGlue->setChecked(cfg.overwriteGlue);
    m_converterPath->setText(cfg.converterPath);

    unblock(m_language); unblock(m_framework); unblock(m_outputDir);
    unblock(m_namespace); unblock(m_overwriteGlue); unblock(m_converterPath);
}

SpecConfig SpecConfigEditor::configFromForm() const
{
    SpecConfig cfg;
    cfg.outputDirectory = m_outputDir->text().trimmed();
    cfg.language        = m_language->currentText();
    cfg.framework       = m_framework->currentText();
    cfg.namespacePrefix = m_namespace->text().trimmed();
    cfg.overwriteGlue   = m_overwriteGlue->isChecked();
    cfg.converterPath   = m_converterPath->text().trimmed();
    return cfg;
}
