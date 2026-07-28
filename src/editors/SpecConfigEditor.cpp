#include "SpecConfigEditor.h"
#include "../model/SpecConfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPlainTextEdit>
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
    // Every target SpecTableConverter can emit. Cpp, JavaScript, Go and Swift
    // were already supported by the converter but missing from this list.
    m_language->addItems({ "CSharp", "Java", "Rust", "Python",
                           "Cpp", "JavaScript", "TypeScript", "Go", "Swift" });
    langForm->addRow(tr("Language:"), m_language);

    m_framework = new QComboBox(langGroup);
    langForm->addRow(tr("Test framework:"), m_framework);

    m_namespace = new QLineEdit(langGroup);
    m_namespace->setPlaceholderText(tr("(blank = no prefix)"));
    langForm->addRow(tr("Namespace prefix:"), m_namespace);

    root->addWidget(langGroup);

    // ── Output options ────────────────────────────────────────────────────────
    auto* outOptGroup  = new QGroupBox(tr("Output Options"), inner);
    auto* outOptLayout = new QVBoxLayout(outOptGroup);

    m_copySpectable = new QCheckBox(
        tr("Copy .spectable source file to output directory"), outOptGroup);
    m_copySpectable->setChecked(true);
    auto* copyHint = new QLabel(
        tr("When checked, the .spectable file is copied alongside the generated code."),
        outOptGroup);
    copyHint->setWordWrap(true);
    copyHint->setStyleSheet("color: gray; font-size: 11px;");
    outOptLayout->addWidget(m_copySpectable);
    outOptLayout->addWidget(copyHint);

    root->addWidget(outOptGroup);

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

    // ── Test scaffolding group ─────────────────────────────────────────────────
    auto* testGroup  = new QGroupBox(tr("Test Scaffolding"), inner);
    auto* testLayout = new QVBoxLayout(testGroup);

    m_failEveryTest = new QCheckBox(
        tr("Fail every generated step until implemented"), testGroup);
    auto* testHint = new QLabel(
        tr("When checked, every generated glue stub ends with a failure, so a fresh "
           "scaffold is all-red until each step is actually implemented. Unchecked, "
           "a stub prints its arguments and returns — an unimplemented step then "
           "reports success."),
        testGroup);
    testHint->setWordWrap(true);
    testHint->setStyleSheet("color: gray; font-size: 11px;");
    testLayout->addWidget(m_failEveryTest);
    testLayout->addWidget(testHint);

    root->addWidget(testGroup);

    // ── Production classes group ──────────────────────────────────────────────
    auto* prodGroup  = new QGroupBox(tr("Production Classes"), inner);
    auto* prodLayout = new QVBoxLayout(prodGroup);

    m_createProdClasses = new QCheckBox(
        tr("Create production classes if they do not exist"), prodGroup);
    auto* prodHint = new QLabel(
        tr("When checked, generates a production class for each DataType (ValidValues → class, "
           "Enumeration → enum) in the specified folder, only if the file is not already present."),
        prodGroup);
    prodHint->setWordWrap(true);
    prodHint->setStyleSheet("color: gray; font-size: 11px;");
    prodLayout->addWidget(m_createProdClasses);
    prodLayout->addWidget(prodHint);

    m_prodClassesDetails = new QWidget(prodGroup);
    auto* prodForm = new QFormLayout(m_prodClassesDetails);
    prodForm->setContentsMargins(0, 8, 0, 0);
    prodForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_prodClassesDir = new QLineEdit(m_prodClassesDetails);
    m_prodClassesDir->setPlaceholderText(tr("e.g.  C:/my/project/src/main/java/com/example"));
    m_browseProdClassesDir = new QPushButton(tr("Browse…"), m_prodClassesDetails);
    m_browseProdClassesDir->setFixedWidth(90);
    auto* prodDirRow = new QHBoxLayout;
    prodDirRow->addWidget(m_prodClassesDir);
    prodDirRow->addWidget(m_browseProdClassesDir);
    prodForm->addRow(tr("Folder for production classes:"), prodDirRow);

    m_prodClassesPackage = new QLineEdit(m_prodClassesDetails);
    m_prodClassesPackage->setPlaceholderText(tr("e.g.  com.example.domain"));
    prodForm->addRow(tr("Package for production classes:"), m_prodClassesPackage);

    prodLayout->addWidget(m_prodClassesDetails);
    m_prodClassesDetails->setVisible(false);
    root->addWidget(prodGroup);

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

    // ── Imports group ─────────────────────────────────────────────────────────
    auto* impGroup  = new QGroupBox(tr("Extra Imports / Using Statements"), inner);
    auto* impLayout = new QVBoxLayout(impGroup);

    auto* impHint = new QLabel(
        tr("One statement per line. Added verbatim to every generated source file after "
           "the automatic imports.\n"
           "Java example:  import com.example.Money;\n"
           "C# example:    using Example.Domain;"), impGroup);
    impHint->setWordWrap(true);
    impHint->setStyleSheet("color: gray; font-size: 11px;");
    impLayout->addWidget(impHint);

    m_imports = new QPlainTextEdit(impGroup);
    m_imports->setPlaceholderText(tr("import com.example.Money;\nimport com.example.Validator;"));
    m_imports->setFixedHeight(120);
    impLayout->addWidget(m_imports);

    root->addWidget(impGroup);

    // ── Tag filter group ──────────────────────────────────────────────────────
    auto* tfGroup  = new QGroupBox(tr("Generator Tag Filter"), inner);
    auto* tfLayout = new QVBoxLayout(tfGroup);

    auto* tfHint = new QLabel(
        tr("Boolean expression of $tags. Only matching blocks are generated.\n"
           "Leave blank to generate everything.\n"
           "Examples:  smoke\n"
           "           smoke AND NOT wip\n"
           "           (smoke OR regression) AND NOT draft"), tfGroup);
    tfHint->setWordWrap(true);
    tfHint->setStyleSheet("color: gray; font-size: 11px;");
    tfLayout->addWidget(tfHint);

    m_tagFilter = new QLineEdit(tfGroup);
    m_tagFilter->setPlaceholderText(tr("e.g.  smoke AND NOT wip"));
    tfLayout->addWidget(m_tagFilter);

    root->addWidget(tfGroup);

    // ── External Spectables group ─────────────────────────────────────────────
    auto* extGroup  = new QGroupBox(tr("External Spectables (Cross-Project Types)"), inner);
    auto* extLayout = new QVBoxLayout(extGroup);

    auto* extHint = new QLabel(
        tr("External .spectable files from other projects whose DataTypes, Entities, and "
           "AttributeSets are visible in this project.\n"
           "Each entry can specify a production code directory and code import statements "
           "(wildcards are allowed, e.g. import com.example.types.*)."),
        extGroup);
    extHint->setWordWrap(true);
    extHint->setStyleSheet("color: gray; font-size: 11px;");
    extLayout->addWidget(extHint);

    m_extSpecList = new QListWidget(extGroup);
    m_extSpecList->setFixedHeight(100);
    extLayout->addWidget(m_extSpecList);

    auto* extBtnRow = new QHBoxLayout;
    m_extSpecAdd    = new QPushButton(tr("Add…"), extGroup);
    m_extSpecRemove = new QPushButton(tr("Remove"), extGroup);
    m_extSpecRemove->setEnabled(false);
    extBtnRow->addWidget(m_extSpecAdd);
    extBtnRow->addWidget(m_extSpecRemove);
    extBtnRow->addStretch();
    extLayout->addLayout(extBtnRow);

    // Detail pane (hidden until an item is selected)
    m_extSpecDetail = new QWidget(extGroup);
    auto* extDetailForm = new QFormLayout(m_extSpecDetail);
    extDetailForm->setContentsMargins(0, 8, 0, 0);
    extDetailForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_extSpecFile = new QLineEdit(m_extSpecDetail);
    m_extSpecFile->setPlaceholderText(tr("Path to the external .spectable file"));
    m_browseExtSpecFile = new QPushButton(tr("Browse…"), m_extSpecDetail);
    m_browseExtSpecFile->setFixedWidth(90);
    auto* extFileRow = new QHBoxLayout;
    extFileRow->addWidget(m_extSpecFile);
    extFileRow->addWidget(m_browseExtSpecFile);
    extDetailForm->addRow(tr("File:"), extFileRow);

    m_extSpecProdDir = new QLineEdit(m_extSpecDetail);
    m_extSpecProdDir->setPlaceholderText(tr("Production code directory for types in this file (optional)"));
    m_browseExtSpecProdDir = new QPushButton(tr("Browse…"), m_extSpecDetail);
    m_browseExtSpecProdDir->setFixedWidth(90);
    auto* extProdRow = new QHBoxLayout;
    extProdRow->addWidget(m_extSpecProdDir);
    extProdRow->addWidget(m_browseExtSpecProdDir);
    extDetailForm->addRow(tr("Production dir:"), extProdRow);

    m_extSpecImports = new QPlainTextEdit(m_extSpecDetail);
    m_extSpecImports->setPlaceholderText(
        tr("import com.example.types.*;\nimport com.example.domain.Money;"));
    m_extSpecImports->setFixedHeight(80);
    extDetailForm->addRow(tr("Code imports:"), m_extSpecImports);

    extLayout->addWidget(m_extSpecDetail);
    m_extSpecDetail->setVisible(false);
    root->addWidget(extGroup);

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
    connect(m_overwriteGlue,  &QCheckBox::toggled, this, &SpecConfigEditor::markDirty);
    connect(m_copySpectable,  &QCheckBox::toggled, this, &SpecConfigEditor::markDirty);
    connect(m_converterPath, &QLineEdit::textChanged, this, &SpecConfigEditor::markDirty);
    connect(m_imports,    &QPlainTextEdit::textChanged, this, &SpecConfigEditor::markDirty);
    connect(m_tagFilter,  &QLineEdit::textChanged,      this, &SpecConfigEditor::markDirty);
    connect(browseOut, &QPushButton::clicked, this, &SpecConfigEditor::onBrowseOutputDir);
    connect(m_browseConverter, &QPushButton::clicked, this, &SpecConfigEditor::onBrowseConverter);
    connect(m_browseProdClassesDir, &QPushButton::clicked,
            this, &SpecConfigEditor::onBrowseProdClassesDir);
    connect(m_createProdClasses, &QCheckBox::toggled,
            this, &SpecConfigEditor::onCreateProdClassesToggled);
    connect(m_createProdClasses, &QCheckBox::toggled, this, &SpecConfigEditor::markDirty);
    connect(m_prodClassesDir,    &QLineEdit::textChanged, this, &SpecConfigEditor::markDirty);
    connect(m_prodClassesPackage, &QLineEdit::textChanged, this, &SpecConfigEditor::markDirty);

    connect(m_extSpecList, &QListWidget::currentRowChanged,
            this, &SpecConfigEditor::onExtSpecSelectionChanged);
    connect(m_extSpecAdd,    &QPushButton::clicked, this, &SpecConfigEditor::onExtSpecAdd);
    connect(m_extSpecRemove, &QPushButton::clicked, this, &SpecConfigEditor::onExtSpecRemove);
    connect(m_browseExtSpecFile,    &QPushButton::clicked, this, &SpecConfigEditor::onBrowseExtSpecFile);
    connect(m_browseExtSpecProdDir, &QPushButton::clicked, this, &SpecConfigEditor::onBrowseExtSpecProdDir);
    connect(m_extSpecFile,    &QLineEdit::textChanged,      this, [this](const QString& text) {
        if (!m_extSpecSyncing && m_extSpecCurrentRow >= 0
            && m_extSpecCurrentRow < m_extSpecList->count()) {
            const QString display = QFileInfo(text).fileName();
            m_extSpecList->item(m_extSpecCurrentRow)->setText(display.isEmpty() ? text : display);
            m_extSpecList->item(m_extSpecCurrentRow)->setToolTip(text);
        }
        if (!m_extSpecSyncing) markDirty();
    });
    connect(m_extSpecProdDir, &QLineEdit::textChanged,      this, [this]() { if (!m_extSpecSyncing) markDirty(); });
    connect(m_extSpecImports, &QPlainTextEdit::textChanged, this, [this]() { if (!m_extSpecSyncing) markDirty(); });

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

    // Namespace only applies to CSharp/Java; not used by Rust
    m_namespace->setEnabled(language == "CSharp" || language == "Java");
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

    m_extSpecSyncing = true;
    block(m_language); block(m_framework); block(m_outputDir);
    block(m_namespace); block(m_overwriteGlue); block(m_copySpectable);
    block(m_converterPath); block(m_imports); block(m_tagFilter);
    block(m_createProdClasses); block(m_prodClassesDir); block(m_prodClassesPackage);

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
    m_namespace->setEnabled(cfg.language == "CSharp" || cfg.language == "Java");
    m_overwriteGlue->setChecked(cfg.overwriteGlue);
    m_failEveryTest->setChecked(cfg.failEveryTest);
    m_copySpectable->setChecked(cfg.copySpectable);
    m_converterPath->setText(cfg.converterPath);
    m_imports->setPlainText(cfg.imports.join("\n"));
    m_tagFilter->setText(cfg.tagFilter);

    m_createProdClasses->setChecked(cfg.createProductionClasses);
    m_prodClassesDir->setText(cfg.productionClassesDir);
    m_prodClassesPackage->setText(cfg.productionClassesPackage);
    m_prodClassesDetails->setVisible(cfg.createProductionClasses);

    unblock(m_language); unblock(m_framework); unblock(m_outputDir);
    unblock(m_namespace); unblock(m_overwriteGlue); unblock(m_copySpectable);
    unblock(m_converterPath); unblock(m_imports); unblock(m_tagFilter);
    unblock(m_createProdClasses); unblock(m_prodClassesDir); unblock(m_prodClassesPackage);

    // Populate external spectables list
    m_extSpectables = cfg.externalSpectables;
    m_extSpecCurrentRow = -1;
    m_extSpecList->clear();
    for (const ExternalSpectable& es : m_extSpectables) {
        auto* item = new QListWidgetItem(QFileInfo(es.file).fileName());
        item->setToolTip(es.file);
        m_extSpecList->addItem(item);
    }
    m_extSpecDetail->setVisible(false);
    m_extSpecSyncing = false;
}

SpecConfig SpecConfigEditor::configFromForm() const
{
    SpecConfig cfg;
    cfg.outputDirectory = m_outputDir->text().trimmed();
    cfg.language        = m_language->currentText();
    cfg.framework       = m_framework->currentText();
    cfg.namespacePrefix = m_namespace->text().trimmed();
    cfg.overwriteGlue   = m_overwriteGlue->isChecked();
    cfg.failEveryTest   = m_failEveryTest->isChecked();
    cfg.copySpectable   = m_copySpectable->isChecked();
    cfg.converterPath   = m_converterPath->text().trimmed();
    const QString impText = m_imports->toPlainText();
    for (const QString& line : impText.split('\n'))
        if (!line.trimmed().isEmpty()) cfg.imports << line.trimmed();
    cfg.tagFilter = m_tagFilter->text().trimmed();
    cfg.createProductionClasses  = m_createProdClasses->isChecked();
    cfg.productionClassesDir     = m_prodClassesDir->text().trimmed();
    cfg.productionClassesPackage = m_prodClassesPackage->text().trimmed();

    // External spectables: use in-memory list with current detail pane state merged in
    cfg.externalSpectables = m_extSpectables;
    if (m_extSpecCurrentRow >= 0 && m_extSpecCurrentRow < cfg.externalSpectables.size()) {
        ExternalSpectable& es = cfg.externalSpectables[m_extSpecCurrentRow];
        es.file         = m_extSpecFile->text().trimmed();
        es.productionDir = m_extSpecProdDir->text().trimmed();
        es.codeImports.clear();
        for (const QString& ln : m_extSpecImports->toPlainText().split('\n'))
            if (!ln.trimmed().isEmpty()) es.codeImports << ln.trimmed();
    }
    return cfg;
}

void SpecConfigEditor::onBrowseProdClassesDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Production Classes Folder"), QString());
    if (!dir.isEmpty()) m_prodClassesDir->setText(dir);
}

void SpecConfigEditor::onCreateProdClassesToggled(bool checked)
{
    m_prodClassesDetails->setVisible(checked);
}

void SpecConfigEditor::saveCurrentExtSpecRow()
{
    if (m_extSpecCurrentRow < 0 || m_extSpecCurrentRow >= m_extSpectables.size()) return;
    ExternalSpectable& es = m_extSpectables[m_extSpecCurrentRow];
    es.file          = m_extSpecFile->text().trimmed();
    es.productionDir = m_extSpecProdDir->text().trimmed();
    es.codeImports.clear();
    for (const QString& ln : m_extSpecImports->toPlainText().split('\n'))
        if (!ln.trimmed().isEmpty()) es.codeImports << ln.trimmed();
    // Sync list item display name
    if (m_extSpecCurrentRow < m_extSpecList->count()) {
        const QString display = QFileInfo(es.file).fileName();
        m_extSpecList->item(m_extSpecCurrentRow)->setText(display.isEmpty() ? es.file : display);
        m_extSpecList->item(m_extSpecCurrentRow)->setToolTip(es.file);
    }
}

void SpecConfigEditor::onExtSpecSelectionChanged()
{
    if (m_extSpecSyncing) return;

    // Save the row we're leaving
    saveCurrentExtSpecRow();

    const int newRow = m_extSpecList->currentRow();
    m_extSpecCurrentRow = newRow;

    m_extSpecSyncing = true;
    if (newRow >= 0 && newRow < m_extSpectables.size()) {
        const ExternalSpectable& es = m_extSpectables[newRow];
        m_extSpecFile->setText(es.file);
        m_extSpecProdDir->setText(es.productionDir);
        m_extSpecImports->setPlainText(es.codeImports.join('\n'));
        m_extSpecDetail->setVisible(true);
        m_extSpecRemove->setEnabled(true);
    } else {
        m_extSpecDetail->setVisible(false);
        m_extSpecRemove->setEnabled(false);
    }
    m_extSpecSyncing = false;
}

void SpecConfigEditor::onExtSpecAdd()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select External Spectable"),
        QString(),
        tr("SpecTable files (*.spectable);;All files (*)"));
    if (file.isEmpty()) return;

    // Save current row before switching
    saveCurrentExtSpecRow();

    ExternalSpectable es;
    es.file = file;
    m_extSpectables.append(es);

    m_extSpecSyncing = true;
    auto* item = new QListWidgetItem(QFileInfo(file).fileName());
    item->setToolTip(file);
    m_extSpecList->addItem(item);
    m_extSpecCurrentRow = m_extSpectables.size() - 1;
    m_extSpecList->setCurrentRow(m_extSpecCurrentRow);
    m_extSpecFile->setText(file);
    m_extSpecProdDir->clear();
    m_extSpecImports->clear();
    m_extSpecDetail->setVisible(true);
    m_extSpecRemove->setEnabled(true);
    m_extSpecSyncing = false;

    markDirty();
}

void SpecConfigEditor::onExtSpecRemove()
{
    const int row = m_extSpecList->currentRow();
    if (row < 0 || row >= m_extSpectables.size()) return;

    m_extSpecSyncing = true;
    m_extSpectables.removeAt(row);
    delete m_extSpecList->takeItem(row);
    m_extSpecCurrentRow = -1;
    m_extSpecDetail->setVisible(false);
    m_extSpecRemove->setEnabled(false);
    m_extSpecSyncing = false;

    if (!m_extSpectables.isEmpty()) {
        const int selectRow = qMin(row, m_extSpectables.size() - 1);
        m_extSpecList->setCurrentRow(selectRow);
    }

    markDirty();
}

void SpecConfigEditor::onBrowseExtSpecFile()
{
    const QString current = m_extSpecFile->text();
    const QString startDir = current.isEmpty()
        ? QFileInfo(filePath()).absolutePath() : QFileInfo(current).absolutePath();
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select External Spectable"),
        startDir,
        tr("SpecTable files (*.spectable);;All files (*)"));
    if (!file.isEmpty())
        m_extSpecFile->setText(file);
}

void SpecConfigEditor::onBrowseExtSpecProdDir()
{
    const QString current = m_extSpecProdDir->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Production Code Directory"),
        current.isEmpty() ? QString() : current);
    if (!dir.isEmpty())
        m_extSpecProdDir->setText(dir);
}
