#include "AppController.h"
#include "MainWindow.h"
#include "../model/Solution.h"
#include "../model/Project.h"
#include "../model/SolutionSerializer.h"
#include "../ui/SolutionExplorer.h"
#include "../ui/SolutionTreeModel.h"
#include "../ui/EditorTabWidget.h"
#include "../ui/StatusBarManager.h"
#include "../ui/OutputPanel.h"
#include "../ui/AttributeInspectorPanel.h"
#include "../ui/EntityTreePanel.h"
#include "../ui/dialogs/NewSolutionDialog.h"
#include "../ui/dialogs/NewProjectDialog.h"
#include "../ui/dialogs/NewFileDialog.h"
#include "../ui/dialogs/ConflictResolutionDialog.h"
#include "../ui/dialogs/GitPushDialog.h"
#include "../ui/dialogs/SettingsDialog.h"
#include "AppSettings.h"
#include "../analyzer/ProjectIndex.h"
#include "../analyzer/FeatureXAnalyzer.h"
#include "../analyzer/SpecTableIndex.h"
#include "../analyzer/SpecTableAnalyzer.h"
#include "../model/ProjectFile.h"
#include "../model/FileType.h"
#include "../build/BuildController.h"
#include "../build/BuildOutputParser.h"
#include "../model/SpecConfig.h"
#include "../git/GitClient.h"
#include "../editors/BaseEditor.h"
#include "../editors/PlainTextEditor.h"
#include "../editors/SpecTableEditor.h"
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMap>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QTreeView>

AppController::AppController(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    m_settings       = new AppSettings();
    m_index          = new ProjectIndex();
    m_analyzer       = new FeatureXAnalyzer(m_settings, m_index);
    m_specTableIndex = new SpecTableIndex();
    m_specAnalyzer   = new SpecTableAnalyzer(m_specTableIndex);
    m_builder        = new BuildController(this);
    m_treeModel = new SolutionTreeModel(this);
    mainWindow->solutionExplorer()->setModel(m_treeModel);

    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileDoubleClicked,
            this, &AppController::onOpenFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::newFileRequested,
            this, &AppController::onNewFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileRenameRequested,
            this, &AppController::onRenameFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileMoveRequested,
            this, &AppController::onMoveFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileDeleteRequested,
            this, &AppController::onDeleteFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::projectRenameRequested,
            this, &AppController::onRenameProject);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::projectMoveRequested,
            this, &AppController::onMoveProject);
    connect(mainWindow->editorTabs(), &EditorTabWidget::fileOpenRequested,
            this, &AppController::onOpenFile);

    if (auto* ep = mainWindow->entityTree())
        connect(ep, &EntityTreePanel::navigateToRequested,
                this, &AppController::navigateToLine);

    applyFonts();

    connect(mainWindow->outputPanel(), &OutputPanel::diagnosticActivated,
            this, [this](const QString& filePath, int line) {
                navigateToLine(filePath, line);
            });
}

AppController::~AppController()
{
    delete m_analyzer;
    delete m_index;
    delete m_settings;
    delete m_solution;
}

void AppController::setSolution(Solution* solution)
{
    delete m_solution;
    m_solution = solution;
    m_treeModel->setSolution(solution);

    if (solution) {
        m_mainWindow->statusBarMgr()->setSolutionName(solution->name());
        m_mainWindow->solutionExplorer()->treeView()->expandAll();
        // Record in recent solutions list
        QString sspecPath = solution->rootPath() + QDir::separator() + solution->name() + ".sspec";
        m_settings->addRecentSolution(sspecPath);
    } else {
        m_mainWindow->statusBarMgr()->clearAll();
    }
    emit solutionLoaded(solution);
}

void AppController::loadSolution(const QString& sspecPath)
{
    QString error;
    Solution* sol = SolutionSerializer::load(sspecPath, error);
    if (!sol) {
        QMessageBox::critical(m_mainWindow, tr("Open Failed"), error);
        return;
    }
    setSolution(sol);
}

void AppController::onNewSolution()
{
    NewSolutionDialog dlg(m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    QString name   = dlg.solutionName();
    QString folder = dlg.rootFolder();

    // Create the root folder if it doesn't exist
    QDir dir(folder);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::critical(m_mainWindow, tr("Error"),
            tr("Cannot create folder: %1").arg(folder));
        return;
    }

    auto* solution = new Solution(name, folder);

    QString error;
    if (!SolutionSerializer::save(solution, error)) {
        QMessageBox::critical(m_mainWindow, tr("Save Failed"), error);
        delete solution;
        return;
    }

    setSolution(solution);
}

void AppController::onNewProject()
{
    NewProjectDialog dlg(m_solution, m_settings->defaultProjectLocation(), m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString projName = dlg.projectName();
    QString projDir;

    if (dlg.isStandalone()) {
        // ── Standalone: project IS the root; solution lives in the same folder ──
        projDir = dlg.standaloneLocation();
        QDir dir(projDir);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::critical(m_mainWindow, tr("Error"),
                tr("Cannot create project folder: %1").arg(projDir));
            return;
        }
        auto* newSol = new Solution(projName, projDir);
        setSolution(newSol);
        // projDir == m_solution->rootPath(), relativePath will be "."

    } else if (!dlg.addToCurrentSolution()) {
        // ── New solution + project as subfolder ───────────────────────────────
        const QString solFolder = dlg.newSolutionFolder();
        QDir solDir(solFolder);
        if (!solDir.exists() && !solDir.mkpath(".")) {
            QMessageBox::critical(m_mainWindow, tr("Error"),
                tr("Cannot create solution folder: %1").arg(solFolder));
            return;
        }
        auto* newSol = new Solution(dlg.newSolutionName(), solFolder);
        setSolution(newSol);

        projDir = m_solution->rootPath() + QDir::separator() + projName;
        QDir pdir(projDir);
        if (!pdir.exists() && !pdir.mkpath(".")) {
            QMessageBox::critical(m_mainWindow, tr("Error"),
                tr("Cannot create project folder: %1").arg(projDir));
            return;
        }
    } else {
        // ── Add to current solution ───────────────────────────────────────────
        projDir = m_solution->rootPath() + QDir::separator() + projName;
        QDir dir(projDir);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::critical(m_mainWindow, tr("Error"),
                tr("Cannot create project folder: %1").arg(projDir));
            return;
        }
    }

    // Run git init in the project directory
    {
        QProcess proc;
        proc.setWorkingDirectory(projDir);
        proc.start("git", {"init"});
        const bool ok = proc.waitForFinished(10000) && proc.exitCode() == 0;
        if (!ok) {
            QMessageBox::warning(m_mainWindow, tr("Git Init Failed"),
                tr("Could not run 'git init' in '%1'.\n"
                   "Make sure git is installed and on your PATH.\n\n"
                   "The project was created but has no git repository.")
                .arg(projDir));
        } else {
            m_mainWindow->outputPanel()->appendBuildOutput(
                tr("Initialized git repository in %1").arg(projDir));
        }
    }

    // Write a default .specconfig so the project is ready to build
    {
        SpecConfig cfg;
        const QString cfgPath = projDir + QDir::separator() + ".specconfig";
        if (!QFile::exists(cfgPath))
            cfg.save(cfgPath);
    }

    auto* project = new Project(projName, projDir);
    project->scanFiles();
    m_solution->addProject(project);

    QString error;
    if (!SolutionSerializer::save(m_solution, error))
        QMessageBox::warning(m_mainWindow, tr("Save Warning"), error);

    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();
}

void AppController::onNewFile(const QString& projectRootHint)
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open or create a project first."));
        return;
    }

    // Pick the project: use hint if provided, else use the first project
    Project* proj = nullptr;
    if (!projectRootHint.isEmpty()) {
        for (auto* p : m_solution->projects())
            if (p->rootPath() == projectRootHint) { proj = p; break; }
    }
    if (!proj) proj = m_solution->projects().first();

    NewFileDialog dlg(m_settings, m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    QString fileName = dlg.fileName();
    QString filePath = proj->rootPath() + QDir::separator() + fileName;

    if (QFile::exists(filePath)) {
        QMessageBox::warning(m_mainWindow, tr("File Exists"),
            tr("'%1' already exists in this project.").arg(fileName));
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(m_mainWindow, tr("Error"),
            tr("Cannot create '%1': %2").arg(filePath, f.errorString()));
        return;
    }
    f.close();

    proj->scanFiles();
    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();

    onOpenFile(filePath);
}

void AppController::onOpenSolution()
{
    QString path = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Open Solution"),
        QString(),
        tr("SpecStudio Solutions (*.sspec);;All Files (*)"));

    if (path.isEmpty()) return;
    loadSolution(path);
}

void AppController::onSave()
{
    auto* tabs = m_mainWindow->editorTabs();
    auto* ed   = tabs->currentEditor();
    if (!ed) return;

    if (tabs->saveCurrentFile()) {
        // Rebuild spec table index so context menu reflects the saved content immediately
        if (fileTypeFromPath(ed->filePath()) == FileType::SpecTable) {
            QStringList stFiles;
            if (m_solution)
                for (auto* proj : m_solution->projects())
                    for (auto* file : proj->files())
                        if (file->type() == FileType::SpecTable)
                            stFiles.append(file->absolutePath());
            if (!stFiles.contains(ed->filePath()))
                stFiles.append(ed->filePath());
            m_specTableIndex->rebuildProject(stFiles);
            if (auto* ste = qobject_cast<SpecTableEditor*>(
                    m_mainWindow->editorForPath(ed->filePath())))
                ste->refreshDynamicCompletions();
        }

        if (m_solution) {
            Project* proj = m_solution->projectForFile(ed->filePath());
            if (proj) {
                connect(proj->git(), &GitClient::outputReady,
                        m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                        Qt::UniqueConnection);
                proj->git()->commitAll(tr("Auto-save"));
            }
        }
    }
}

void AppController::onSaveAll()
{
    m_mainWindow->editorTabs()->saveAllFiles();
    // Auto-commit each project that has a dirty file
    if (m_solution) {
        for (auto* proj : m_solution->projects()) {
            connect(proj->git(), &GitClient::outputReady,
                    m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                    Qt::UniqueConnection);
            proj->git()->commitAll(tr("Auto-save all"));
        }
    }
}

void AppController::onPrint()
{
    auto* ed = qobject_cast<PlainTextEditor*>(m_mainWindow->currentEditor());
    if (!ed) {
        QMessageBox::information(m_mainWindow, tr("No File"), tr("Open a file to print."));
        return;
    }
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, m_mainWindow);
    dialog.setWindowTitle(tr("Print"));
    if (dialog.exec() == QDialog::Accepted)
        ed->textEdit()->print(&printer);
}

void AppController::onSettings()
{
    Project* proj = nullptr;
    if (m_solution && !m_solution->projects().isEmpty())
        proj = m_solution->projects().first();

    SettingsDialog dlg(m_settings, proj, m_mainWindow);
    dlg.exec();
    applyFonts();
    applyAutoReload();
}

void AppController::onCommitAndPush()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project before committing."));
        return;
    }

    GitPushDialog dlg(m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    QString reason = dlg.changeReason();
    m_mainWindow->outputPanel()->showBuildTab();
    m_mainWindow->outputPanel()->appendBuildOutput(tr("--- Commit and Push ---"));

    // For simplicity commit+push all projects; per-project settings come in Phase 8
    for (auto* proj : m_solution->projects()) {
        connect(proj->git(), &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);
        connect(proj->git(), &GitClient::errorOccurred,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);

        QString remote = "origin";
        QString branch = m_settings->gitBranch(proj->rootPath());
        if (branch.isEmpty()) branch = "main";

        bool ok = proj->git()->commitAndPush(reason, remote, branch);
        if (!ok)
            m_mainWindow->outputPanel()->appendBuildOutput(
                tr("[%1] Push failed — check Output panel.").arg(proj->name()));
    }
}

void AppController::onFetch()
{
    if (!m_solution || m_solution->projects().isEmpty()) return;

    m_mainWindow->outputPanel()->showBuildTab();
    m_mainWindow->outputPanel()->appendBuildOutput(tr("--- Fetch ---"));

    for (auto* proj : m_solution->projects()) {
        connect(proj->git(), &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);
        proj->git()->fetch();
    }
}

void AppController::onDiffCurrentFile()
{
    auto* ed = m_mainWindow->currentEditor();
    if (!ed || !m_solution) {
        QMessageBox::information(m_mainWindow, tr("No File"), tr("Open a file first."));
        return;
    }

    const QString filePath = ed->filePath();
    Project* ownerProject  = nullptr;
    QString  relPath;

    for (auto* proj : m_solution->projects()) {
        const QString root = proj->rootPath();
        if (filePath.startsWith(root)) {
            ownerProject = proj;
            relPath = QDir(root).relativeFilePath(filePath);
            break;
        }
    }

    if (!ownerProject) {
        QMessageBox::information(m_mainWindow, tr("Not in Project"),
            tr("The current file is not part of any project."));
        return;
    }

    const QString diffText = ownerProject->git()->diff(relPath);
    const QString title    = QFileInfo(filePath).fileName();

    if (diffText.trimmed().isEmpty()) {
        m_mainWindow->outputPanel()->showDiff(
            tr("(no changes vs HEAD)"), title);
    } else {
        m_mainWindow->outputPanel()->showDiff(diffText, title);
    }
}

void AppController::onPull()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project before pulling."));
        return;
    }

    m_mainWindow->outputPanel()->showBuildTab();
    m_mainWindow->outputPanel()->appendBuildOutput(tr("--- Pull ---"));

    for (auto* proj : m_solution->projects()) {
        connect(proj->git(), &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);
        connect(proj->git(), &GitClient::errorOccurred,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);

        const QString branch = m_settings->gitBranch(proj->rootPath());
        bool ok = proj->git()->pull("origin", branch);

        if (!ok) {
            const QStringList conflicts = proj->git()->conflictedFiles();
            if (!conflicts.isEmpty()) {
                auto* dlg = new ConflictResolutionDialog(
                    proj->name(), proj->git(), proj->rootPath(), conflicts, m_mainWindow);
                connect(dlg, &ConflictResolutionDialog::openFileRequested,
                        this, &AppController::onOpenFile);
                dlg->exec();
                dlg->deleteLater();
                // Reload any modified files
                proj->scanFiles();
                m_treeModel->refresh();
            } else {
                m_mainWindow->outputPanel()->appendBuildOutput(
                    tr("[%1] Pull failed — check output.").arg(proj->name()));
            }
        }
    }
}

// Auto-detect SpecTableConverter.exe: check same dir as SpecStudio, then dev build locations
static QString autoDetectConverter()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    // Production: converter deployed next to SpecStudio.exe
    const QString prod = appDir + "/SpecTableConverter.exe";
    if (QFile::exists(prod)) return prod;
    // Development: Visual Studio build layout  build/src/Debug -> build/converter/Debug
    const QString devDebug   = appDir + "/../../converter/Debug/SpecTableConverter.exe";
    const QString devRelease = appDir + "/../../converter/Release/SpecTableConverter.exe";
    if (QFile::exists(devDebug))   return QFileInfo(devDebug).absoluteFilePath();
    if (QFile::exists(devRelease)) return QFileInfo(devRelease).absoluteFilePath();
    return {};
}

// Find the nearest .specconfig file walking from spectableDir up to projectRoot
static QString findSpecConfig(const QString& spectableDir, const QString& projectRoot)
{
    QDir dir(spectableDir);
    const QString projAbs = QFileInfo(projectRoot).absoluteFilePath();
    while (true) {
        const QStringList found = dir.entryList({ "*.specconfig" }, QDir::Files);
        if (!found.isEmpty())
            return dir.absoluteFilePath(found.first());
        if (dir.absolutePath() == projAbs) break;
        if (!dir.cdUp()) break;
    }
    return {};
}

// Resolve the output directory from a SpecConfig (relative = beside the config file)
static QString resolveOutputDir(const SpecConfig& cfg, const QString& configFilePath,
                                 const QString& spectableFilePath)
{
    const QString base = configFilePath.isEmpty()
        ? QFileInfo(spectableFilePath).dir().absolutePath()
        : QFileInfo(configFilePath).dir().absolutePath();

    const QString out = cfg.outputDirectory.trimmed();
    if (out.isEmpty()) return base + "/generated";
    if (QFileInfo(out).isAbsolute()) return out;
    return QDir(base).absoluteFilePath(out);
}

// Returns the project for the currently selected Solution Explorer node,
// or the project that owns the current editor's file, or nullptr (→ all projects).
Project* AppController::activeProject() const
{
    if (!m_solution) return nullptr;
    // Check Solution Explorer selection first
    Project* p = m_mainWindow->solutionExplorer()->selectedProject(m_treeModel);
    if (p) return p;
    // Fall back to the current editor's file
    auto* ed = m_mainWindow->editorTabs()->currentEditor();
    if (ed) return m_solution->projectForFile(ed->filePath());
    return nullptr;
}

// Set up build signal connections (call after disconnect)
void AppController::setupBuildConnections()
{
    m_buildAccum.clear();
    connect(m_builder, &BuildController::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::outputReady,
            this, [this](const QString& text) { m_buildAccum += text; });
    connect(m_builder, &BuildController::buildFinished,
            this, [this](bool success) {
                const auto diags = BuildOutputParser::parse(m_buildAccum);
                if (!diags.isEmpty()) {
                    m_mainWindow->outputPanel()->setDiagnostics(diags);
                    m_mainWindow->outputPanel()->showAnalysisTab();
                }
                if (!m_buildLogPath.isEmpty()) {
                    QFile logFile(m_buildLogPath);
                    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text))
                        QTextStream(&logFile) << m_buildAccum;
                }
                m_mainWindow->outputPanel()->appendBuildOutput(
                    success ? tr("Done.") : tr("Build failed."));
            });
}

void AppController::onBuildCurrentFile()
{
    auto* ed = m_mainWindow->currentEditor();
    if (!ed) {
        QMessageBox::information(m_mainWindow, tr("No File"),
            tr("Open a file to build."));
        return;
    }

    for (auto* e : m_mainWindow->allOpenEditors()) if (e->isDirty()) e->save();

    if (fileTypeFromPath(ed->filePath()) != FileType::SpecTable) {
        QMessageBox::information(m_mainWindow, tr("Not Supported"),
            tr("Build is only supported for .spectable files."));
        return;
    }

    // Locate config — prefer user-selected active config for this project
    const QString projRoot = m_solution
        ? (m_solution->projectForFile(ed->filePath())
           ? m_solution->projectForFile(ed->filePath())->rootPath() : QString())
        : QString();
    QString cfgPath = projRoot.isEmpty() ? QString()
                                         : m_settings->activeBuildConfig(projRoot);
    if (cfgPath.isEmpty() || !QFile::exists(cfgPath))
        cfgPath = findSpecConfig(QFileInfo(ed->filePath()).dir().absolutePath(),
                                 projRoot.isEmpty()
                                 ? QFileInfo(ed->filePath()).dir().absolutePath()
                                 : projRoot);
    const SpecConfig cfg   = cfgPath.isEmpty() ? SpecConfig{} : SpecConfig::load(cfgPath);

    // Resolve converter and output dir
    const QString converter = cfg.converterPath.isEmpty() ? autoDetectConverter()
                                                           : cfg.converterPath;
    const QString outDir    = resolveOutputDir(cfg, cfgPath, ed->filePath());

    m_mainWindow->outputPanel()->clearBuildOutput();
    m_mainWindow->outputPanel()->showBuildTab();
    m_mainWindow->outputPanel()->appendBuildOutput(
        tr("--- Converting %1 ---").arg(QFileInfo(ed->filePath()).fileName()));
    if (cfgPath.isEmpty())
        m_mainWindow->outputPanel()->appendBuildOutput(
            tr("No .specconfig found — using defaults"));
    m_mainWindow->outputPanel()->appendBuildOutput(
        tr("Output: %1").arg(outDir));

    disconnect(m_builder, &BuildController::outputReady,  this, nullptr);
    disconnect(m_builder, &BuildController::buildFinished, this, nullptr);
    m_buildLogPath = outDir + "/build.log";
    m_mainWindow->outputPanel()->appendBuildOutput(tr("Log: %1").arg(m_buildLogPath));
    setupBuildConnections();

    QStringList args = { ed->filePath(), outDir };
    if (!cfg.language.isEmpty())        args << "--language"  << cfg.language;
    if (!cfg.framework.isEmpty())       args << "--framework" << cfg.framework;
    if (!cfg.namespacePrefix.isEmpty()) args << "--namespace" << cfg.namespacePrefix;
    if (cfg.overwriteGlue)              args << "--overwrite-glue";
    if (!cfg.copySpectable)             args << "--no-copy-spectable";
    if (!projRoot.isEmpty())            args << "--source-root" << projRoot;
    for (const QString& imp : cfg.imports)
        args << "--import" << imp;
    if (!cfg.tagFilter.isEmpty())       args << "--tag-filter" << cfg.tagFilter;
    if (cfg.createProductionClasses && !cfg.productionClassesDir.isEmpty())
        args << "--prod-dir" << cfg.productionClassesDir;
    if (!cfg.productionClassesPackage.isEmpty())
        args << "--prod-package" << cfg.productionClassesPackage;
    // Pass other .spectable files from the same project as context
    if (m_solution) {
        auto* ownerProj = m_solution->projectForFile(ed->filePath());
        if (ownerProj)
            for (auto* pf : ownerProj->files())
                if (pf->type() == FileType::SpecTable && pf->absolutePath() != ed->filePath())
                    args << "--context" << pf->absolutePath();
    }
    m_buildAccum += "FILE:" + ed->filePath() + "\n";
    m_builder->run(converter, args);
}

void AppController::doBuildProjects(const QList<Project*>& targets)
{
    m_mainWindow->outputPanel()->clearBuildOutput();
    m_mainWindow->outputPanel()->showBuildTab();

    disconnect(m_builder, &BuildController::outputReady,  this, nullptr);
    disconnect(m_builder, &BuildController::buildFinished, this, nullptr);

    m_buildLogPath = targets.isEmpty() ? QString() : targets.first()->rootPath() + "/build.log";
    setupBuildConnections();

    for (auto* proj : targets) {
        // Prefer the user-selected config; fall back to nearest .specconfig in tree
        QString cfgPath = m_settings->activeBuildConfig(proj->rootPath());
        if (cfgPath.isEmpty() || !QFile::exists(cfgPath))
            cfgPath = findSpecConfig(proj->rootPath(), proj->rootPath());
        const SpecConfig cfg  = cfgPath.isEmpty() ? SpecConfig{} : SpecConfig::load(cfgPath);
        const QString converter = cfg.converterPath.isEmpty() ? autoDetectConverter()
                                                               : cfg.converterPath;
        if (!cfgPath.isEmpty())
            m_mainWindow->outputPanel()->appendBuildOutput(
                tr("Configuration: %1").arg(QFileInfo(cfgPath).fileName()));

        for (auto* pf : proj->files()) {
            if (pf->type() == FileType::SpecTable) {
                const QString outDir = resolveOutputDir(cfg, cfgPath, pf->absolutePath());
                m_mainWindow->outputPanel()->appendBuildOutput(
                    tr("--- Converting %1 ---").arg(pf->fileName()));
                QStringList args = { pf->absolutePath(), outDir };
                if (!cfg.language.isEmpty())        args << "--language"  << cfg.language;
                if (!cfg.framework.isEmpty())       args << "--framework" << cfg.framework;
                if (!cfg.namespacePrefix.isEmpty()) args << "--namespace" << cfg.namespacePrefix;
                if (cfg.overwriteGlue)              args << "--overwrite-glue";
                if (!cfg.copySpectable)             args << "--no-copy-spectable";
                args << "--source-root" << proj->rootPath();
                for (const QString& imp : cfg.imports)
                    args << "--import" << imp;
                if (!cfg.tagFilter.isEmpty()) args << "--tag-filter" << cfg.tagFilter;
                if (cfg.createProductionClasses && !cfg.productionClassesDir.isEmpty())
                    args << "--prod-dir" << cfg.productionClassesDir;
                if (!cfg.productionClassesPackage.isEmpty())
                    args << "--prod-package" << cfg.productionClassesPackage;
                for (auto* other : proj->files())
                    if (other->type() == FileType::SpecTable && other->absolutePath() != pf->absolutePath())
                        args << "--context" << other->absolutePath();
                m_buildAccum += "FILE:" + pf->absolutePath() + "\n";
                m_builder->run(converter, args);
            }
        }
    }
}

void AppController::onBuildProject()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }
    Project* active = activeProject();
    if (!active) {
        QMessageBox::information(m_mainWindow, tr("No Active Project"),
            tr("Select a project in Solution Explorer or open a file in a project.\n"
               "Use Build > Solution to build all projects."));
        return;
    }
    for (auto* e : m_mainWindow->allOpenEditors()) if (e->isDirty()) e->save();
    doBuildProjects({ active });
}

void AppController::onSetActiveBuildConfig(const QString& configAbsPath)
{
    Project* proj = activeProject();
    if (!proj) return;
    m_settings->setActiveBuildConfig(proj->rootPath(), configAbsPath);
    m_mainWindow->outputPanel()->appendBuildOutput(
        tr("Build configuration set to: %1").arg(QFileInfo(configAbsPath).fileName()));
}

void AppController::onBuildSolution()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Solution"),
            tr("Open a solution first."));
        return;
    }
    for (auto* e : m_mainWindow->allOpenEditors()) if (e->isDirty()) e->save();
    doBuildProjects(m_solution->projects());
}

void AppController::doAnalyze(const QList<Project*>& targets)
{
    QList<Diagnostic>  all;
    QList<CoverageEntry> coverageEntries;
    QStringList allTags;

    for (auto* proj : targets) {
        // FeatureX analysis
        m_index->rebuild(proj);
        all.append(m_analyzer->analyzeProject(proj));
        for (const QString& t : m_index->tags())
            if (!allTags.contains(t))
                allTags.append(t);

        // SpecTable analysis
        QStringList specTableFiles;
        for (auto* file : proj->files())
            if (file->type() == FileType::SpecTable)
                specTableFiles.append(file->absolutePath());
        if (!specTableFiles.isEmpty()) {
            m_specTableIndex->rebuildProject(specTableFiles);
            for (const QString& f : specTableFiles)
                all.append(m_specAnalyzer->analyzeFile(f));
        }

        // Coverage data — per .spectable file
        for (const QString& fp : specTableFiles) {
            const SpecTableSymbols fileSym = m_specTableIndex->buildFor(fp);

            CoverageEntry entry;
            entry.filePath = fp;

            if (!fileSym.specifications.isEmpty())
                entry.specName = fileSym.specifications.begin().key();

            for (auto it = fileSym.scenarios.cbegin(); it != fileSym.scenarios.cend(); ++it)
                if (it.value().filePath == fp) ++entry.scenarios;
            for (auto it = fileSym.businessRules.cbegin(); it != fileSym.businessRules.cend(); ++it)
                if (it.value().filePath == fp) ++entry.businessRules;
            for (auto it = fileSym.calculations.cbegin(); it != fileSym.calculations.cend(); ++it)
                if (it.value().filePath == fp) ++entry.calculations;
            for (auto it = fileSym.attributes.cbegin(); it != fileSym.attributes.cend(); ++it)
                if (it.value().filePath == fp) ++entry.attrSets;
            for (auto it = fileSym.entities.cbegin(); it != fileSym.entities.cend(); ++it)
                if (it.value().filePath == fp) ++entry.attrSets;

            if (!entry.specName.isEmpty()) {
                const QString cfgPath = findSpecConfig(
                    QFileInfo(fp).dir().absolutePath(), proj->rootPath());
                const SpecConfig cfg = cfgPath.isEmpty() ? SpecConfig{} : SpecConfig::load(cfgPath);
                const QString outDir = resolveOutputDir(cfg, cfgPath, fp);
                const QString ext = (cfg.language.compare("Java", Qt::CaseInsensitive) == 0)
                    ? ".java" : ".cs";
                QString className = entry.specName;
                className.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
                className.remove(QRegularExpression("^_+|_+$"));
                entry.testsGenerated = QFile::exists(QDir(outDir).filePath(className + "_Tests" + ext));
            }

            coverageEntries << entry;
        }
    }

    m_mainWindow->outputPanel()->setCoverageData(coverageEntries);
    m_mainWindow->outputPanel()->setDiagnostics(all);
    m_mainWindow->outputPanel()->showAnalysisTab();

    QMap<QString, QList<QPair<int,int>>> marksByFile;
    for (const auto& d : all)
        marksByFile[d.filePath].append({d.line, d.column});
    for (auto it = marksByFile.cbegin(); it != marksByFile.cend(); ++it) {
        if (auto* ed = m_mainWindow->editorForPath(it.key()))
            ed->setErrorMarks(it.value());
    }

    for (auto* ed : m_mainWindow->allOpenEditors()) {
        ed->setTagCompletionWords(allTags);
        if (auto* ste = qobject_cast<SpecTableEditor*>(ed))
            ste->refreshDynamicCompletions();
    }

    if (auto* panel = m_mainWindow->entityTree())
        panel->refresh(m_specTableIndex);
}

void AppController::onAnalyze()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }
    Project* active = activeProject();
    if (!active) {
        QMessageBox::information(m_mainWindow, tr("No Active Project"),
            tr("Select a project in Solution Explorer or open a file in a project.\n"
               "Use Analyze > Solution to analyze all projects."));
        return;
    }
    for (auto* e : m_mainWindow->allOpenEditors()) if (e->isDirty()) e->save();
    doAnalyze({ active });
}

void AppController::onAnalyzeSolution()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Solution"),
            tr("Open a solution first."));
        return;
    }
    for (auto* e : m_mainWindow->allOpenEditors()) if (e->isDirty()) e->save();
    doAnalyze(m_solution->projects());
}

void AppController::onRenameFile(const QString& absolutePath)
{
    QFileInfo fi(absolutePath);
    bool ok;
    const QString newName = QInputDialog::getText(
        m_mainWindow, tr("Rename File"),
        tr("New name:"), QLineEdit::Normal, fi.fileName(), &ok);
    if (!ok || newName.isEmpty() || newName == fi.fileName()) return;

    const QString newPath = fi.absolutePath() + "/" + newName;
    if (QFile::exists(newPath)) {
        QMessageBox::warning(m_mainWindow, tr("Rename Failed"),
            tr("A file named '%1' already exists.").arg(newName));
        return;
    }

    m_mainWindow->editorTabs()->closeFile(absolutePath);

    if (!QFile::rename(absolutePath, newPath)) {
        QMessageBox::critical(m_mainWindow, tr("Rename Failed"),
            tr("Could not rename '%1'.").arg(fi.fileName()));
        return;
    }

    if (m_solution) {
        for (auto* proj : m_solution->projects()) {
            proj->scanFiles();
            connect(proj->git(), &GitClient::outputReady,
                    m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                    Qt::UniqueConnection);
            proj->git()->commitAll(tr("Rename %1 to %2").arg(fi.fileName(), newName));
        }
        m_treeModel->refresh();
        m_mainWindow->solutionExplorer()->treeView()->expandAll();
    }
}

void AppController::onMoveFile(const QString& absolutePath)
{
    if (!m_solution) return;

    const QString fileName = QFileInfo(absolutePath).fileName();
    Project* srcProj = m_solution->projectForFile(absolutePath);
    if (!srcProj) return;

    const QString destDir = QFileDialog::getExistingDirectory(
        m_mainWindow,
        tr("Move '%1' To Folder").arg(fileName),
        srcProj->rootPath());
    if (destDir.isEmpty()) return;

    const QString newPath = destDir + "/" + fileName;
    if (newPath == absolutePath) return;

    if (QFile::exists(newPath)) {
        QMessageBox::warning(m_mainWindow, tr("Move Failed"),
            tr("'%1' already exists in that folder.").arg(fileName));
        return;
    }

    m_mainWindow->editorTabs()->closeFile(absolutePath);

    if (!QFile::rename(absolutePath, newPath)) {
        QMessageBox::critical(m_mainWindow, tr("Move Failed"),
            tr("Could not move '%1'.").arg(fileName));
        return;
    }

    for (auto* proj : m_solution->projects()) {
        proj->scanFiles();
        connect(proj->git(), &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);
        proj->git()->commitAll(tr("Move %1").arg(fileName));
    }
    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();
    onOpenFile(newPath);
}

void AppController::onRenameProject(const QString& projectRootPath)
{
    if (!m_solution) return;

    Project* proj = nullptr;
    for (auto* p : m_solution->projects())
        if (p->rootPath() == projectRootPath) { proj = p; break; }
    if (!proj) return;

    bool ok;
    const QString newName = QInputDialog::getText(
        m_mainWindow, tr("Rename Project"),
        tr("New project name:"), QLineEdit::Normal, proj->name(), &ok);
    if (!ok || newName.trimmed().isEmpty() || newName == proj->name()) return;

    proj->setName(newName.trimmed());

    QString error;
    if (!SolutionSerializer::save(m_solution, error))
        QMessageBox::warning(m_mainWindow, tr("Save Warning"), error);

    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();
    m_mainWindow->statusBarMgr()->setSolutionName(m_solution->name());
}

void AppController::onMoveProject(const QString& projectRootPath)
{
    if (!m_solution) return;

    Project* proj = nullptr;
    for (auto* p : m_solution->projects())
        if (p->rootPath() == projectRootPath) { proj = p; break; }
    if (!proj) return;

    const QString folderName = QFileInfo(projectRootPath).fileName();
    const QString newParent = QFileDialog::getExistingDirectory(
        m_mainWindow,
        tr("Move Project '%1' To Folder").arg(proj->name()),
        QFileInfo(projectRootPath).absolutePath());
    if (newParent.isEmpty()) return;

    const QString newRoot = newParent + "/" + folderName;
    if (QDir::cleanPath(newRoot) == QDir::cleanPath(projectRootPath)) return;

    if (QDir(newRoot).exists()) {
        QMessageBox::warning(m_mainWindow, tr("Move Failed"),
            tr("A folder named '%1' already exists in that location.").arg(folderName));
        return;
    }

    // Close all open tabs for files in this project
    for (auto* pf : proj->files())
        m_mainWindow->editorTabs()->closeFile(pf->absolutePath());

    if (!QDir().rename(projectRootPath, newRoot)) {
        QMessageBox::critical(m_mainWindow, tr("Move Failed"),
            tr("Could not move project folder '%1'.").arg(folderName));
        return;
    }

    proj->setRootPath(newRoot);
    proj->scanFiles();

    QString error;
    if (!SolutionSerializer::save(m_solution, error))
        QMessageBox::warning(m_mainWindow, tr("Save Warning"), error);

    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();
}

void AppController::onDeleteFile(const QString& absolutePath)
{
    const QString name = QFileInfo(absolutePath).fileName();
    auto btn = QMessageBox::question(m_mainWindow, tr("Delete File"),
        tr("Permanently delete '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    m_mainWindow->editorTabs()->closeFile(absolutePath);

    if (!QFile::remove(absolutePath)) {
        QMessageBox::critical(m_mainWindow, tr("Delete Failed"),
            tr("Could not delete '%1'.").arg(name));
        return;
    }

    if (m_solution) {
        for (auto* proj : m_solution->projects()) {
            proj->scanFiles();
            connect(proj->git(), &GitClient::outputReady,
                    m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                    Qt::UniqueConnection);
            proj->git()->commitAll(tr("Delete %1").arg(name));
        }
        m_treeModel->refresh();
        m_mainWindow->solutionExplorer()->treeView()->expandAll();
    }
}

void AppController::onRefreshSolution()
{
    if (!m_solution) return;
    for (auto* proj : m_solution->projects())
        proj->scanFiles();
    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();
}

void AppController::onFindAllUsages()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }

    // Default to selected text or word under cursor in current editor
    QString defaultTerm;
    if (auto* ed = qobject_cast<PlainTextEditor*>(m_mainWindow->currentEditor())) {
        defaultTerm = ed->textEdit()->textCursor().selectedText();
        if (defaultTerm.isEmpty()) {
            QTextCursor tc = ed->textEdit()->textCursor();
            tc.select(QTextCursor::WordUnderCursor);
            defaultTerm = tc.selectedText().trimmed();
        }
    }
    const bool currentIsSpecTable =
        qobject_cast<SpecTableEditor*>(m_mainWindow->currentEditor()) != nullptr;

    bool ok;
    const QString term = QInputDialog::getText(
        m_mainWindow, tr("Find All Usages"),
        tr("Search for:"), QLineEdit::Normal, defaultTerm, &ok);
    if (!ok || term.isEmpty()) return;

    // For SpecTable files use symbol-aware whole-word search
    if (currentIsSpecTable) {
        findReferencesForSymbol(term);
        return;
    }

    QList<Diagnostic> results;
    for (auto* proj : m_solution->projects()) {
        for (auto* file : proj->files()) {
            QFile f(file->absolutePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            int lineNum = 1;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (line.contains(term, Qt::CaseInsensitive)) {
                    Diagnostic d;
                    d.filePath = file->absolutePath();
                    d.line     = lineNum;
                    d.message  = line.trimmed();
                    d.severity = Diagnostic::Severity::Info;
                    results.append(d);
                }
                ++lineNum;
            }
        }
    }

    m_mainWindow->outputPanel()->setFindResults(results, term);
    m_mainWindow->outputPanel()->showFindResultsTab();
}

void AppController::onFindAll(const QString& term, bool caseSensitive, bool useRegex)
{
    if (term.isEmpty()) return;

    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QRegularExpression re;
    if (useRegex) {
        re = QRegularExpression(term,
            caseSensitive ? QRegularExpression::NoPatternOption
                          : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) return;
    }

    auto lineMatches = [&](const QString& line) {
        return useRegex ? re.match(line).hasMatch()
                        : line.contains(term, cs);
    };

    QList<Diagnostic> results;
    QSet<QString> searchedPaths;

    // 1. Search open editors (in-memory content — catches unsaved edits)
    for (auto* ed : m_mainWindow->allOpenEditors()) {
        auto* pte = qobject_cast<PlainTextEditor*>(ed);
        if (!pte) continue;
        searchedPaths.insert(ed->filePath());
        QTextDocument* doc = pte->textEdit()->document();
        for (QTextBlock blk = doc->begin(); blk != doc->end(); blk = blk.next()) {
            if (lineMatches(blk.text())) {
                Diagnostic d;
                d.filePath = ed->filePath();
                d.line     = blk.blockNumber() + 1;
                d.message  = blk.text().trimmed();
                d.severity = Diagnostic::Severity::Info;
                results.append(d);
            }
        }
    }

    // 2. If nothing found in open editors (or no editors open), also search project files
    if (results.isEmpty() && m_solution) {
        for (auto* proj : m_solution->projects()) {
            for (auto* file : proj->files()) {
                if (searchedPaths.contains(file->absolutePath())) continue;
                QFile f(file->absolutePath());
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                QTextStream in(&f);
                int lineNum = 1;
                while (!in.atEnd()) {
                    const QString line = in.readLine();
                    if (lineMatches(line)) {
                        Diagnostic d;
                        d.filePath = file->absolutePath();
                        d.line     = lineNum;
                        d.message  = line.trimmed();
                        d.severity = Diagnostic::Severity::Info;
                        results.append(d);
                    }
                    ++lineNum;
                }
            }
        }
    }

    m_mainWindow->outputPanel()->setFindResults(results, term);
    m_mainWindow->outputPanel()->showFindResultsTab();
}

void AppController::onRenameStep()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }

    QString defaultOld;
    if (auto* ed = qobject_cast<PlainTextEditor*>(m_mainWindow->currentEditor()))
        defaultOld = ed->textEdit()->textCursor().selectedText();

    bool ok;
    const QString oldText = QInputDialog::getText(
        m_mainWindow, tr("Rename Step"),
        tr("Find text:"), QLineEdit::Normal, defaultOld, &ok);
    if (!ok || oldText.isEmpty()) return;

    const QString newText = QInputDialog::getText(
        m_mainWindow, tr("Rename Step"),
        tr("Replace with:"), QLineEdit::Normal, oldText, &ok);
    if (!ok) return;

    int filesChanged = 0;
    int totalReplaced = 0;

    for (auto* proj : m_solution->projects()) {
        bool projModified = false;
        for (auto* file : proj->files()) {
            QFile f(file->absolutePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            QString content = in.readAll();
            f.close();

            const int count = content.count(oldText, Qt::CaseSensitive);
            if (count == 0) continue;

            content.replace(oldText, newText, Qt::CaseSensitive);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) continue;
            QTextStream out(&f);
            out << content;
            f.close();

            totalReplaced += count;
            ++filesChanged;
            projModified = true;

            // Reload the file if it's open in an editor
            if (auto* ed = m_mainWindow->editorTabs()->editorForPath(file->absolutePath()))
                ed->load(file->absolutePath());
        }

        if (projModified) {
            connect(proj->git(), &GitClient::outputReady,
                    m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                    Qt::UniqueConnection);
            proj->git()->commitAll(tr("Rename: %1 → %2").arg(oldText, newText));
        }
    }

    QMessageBox::information(m_mainWindow, tr("Rename Complete"),
        tr("Replaced %1 occurrence(s) in %2 file(s).")
            .arg(totalReplaced).arg(filesChanged));
}

void AppController::renameSpecTableSymbol(const QString& oldName)
{
    if (!m_solution || m_solution->projects().isEmpty()) return;

    const SymbolLocation loc = m_specTableIndex->projectSymbols().locationFor(oldName);
    if (loc.filePath.isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("Unknown Symbol"),
            tr("'%1' is not a known SpecTable symbol.").arg(oldName));
        return;
    }

    bool ok;
    const QString newName = QInputDialog::getText(
        m_mainWindow, tr("Rename Symbol"),
        tr("Rename '%1' to:").arg(oldName), QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.isEmpty() || newName == oldName) return;

    static QRegularExpression reIdent(R"(^[A-Za-z_]\w*$)");
    if (!reIdent.match(newName).hasMatch()) {
        QMessageBox::warning(m_mainWindow, tr("Invalid Name"),
            tr("'%1' is not a valid symbol name.").arg(newName));
        return;
    }

    const QRegularExpression re(
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(oldName)));

    int filesChanged = 0, totalReplaced = 0;
    for (auto* proj : m_solution->projects()) {
        bool projModified = false;
        for (auto* file : proj->files()) {
            if (file->type() != FileType::SpecTable) continue;
            QFile f(file->absolutePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            QString content = in.readAll();
            f.close();

            const int count = content.count(re);
            if (count == 0) continue;

            content.replace(re, newName);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) continue;
            QTextStream out(&f);
            out << content;
            f.close();

            totalReplaced += count;
            ++filesChanged;
            projModified = true;

            if (auto* pte = qobject_cast<PlainTextEditor*>(
                    m_mainWindow->editorForPath(file->absolutePath())))
                pte->suppressNextExternalChange();
        }
        if (projModified) {
            connect(proj->git(), &GitClient::outputReady,
                    m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                    Qt::UniqueConnection);
            proj->git()->commitAll(tr("Rename: %1 → %2").arg(oldName, newName));
        }
    }

    // Update glue files — replace OldNameString/OldNameTyped with NewName variants
    // and the bare symbol name where it appears as a class identifier
    int glueFilesChanged = 0;
    {
        // Build replacements: order matters — longer strings first
        struct Rep { QRegularExpression re; QString replacement; };
        const QList<Rep> reps = {
            { QRegularExpression(QStringLiteral("\\b%1String\\b").arg(QRegularExpression::escape(oldName))),
              newName + "String" },
            { QRegularExpression(QStringLiteral("\\b%1Typed\\b").arg(QRegularExpression::escape(oldName))),
              newName + "Typed" },
            { QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(oldName))),
              newName },
        };

        for (auto* proj : m_solution->projects()) {
            QDirIterator it(proj->rootPath(),
                            { "*_glue.cs", "*_glue.java" },
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString gluePath = it.next();
                QFile gf(gluePath);
                if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                QString content = QTextStream(&gf).readAll();
                gf.close();

                bool changed = false;
                for (const Rep& rep : reps) {
                    if (content.contains(rep.re)) {
                        content.replace(rep.re, rep.replacement);
                        changed = true;
                    }
                }
                if (!changed) continue;

                if (!gf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) continue;
                QTextStream(&gf) << content;
                gf.close();
                ++glueFilesChanged;

                if (auto* pte = qobject_cast<PlainTextEditor*>(
                        m_mainWindow->editorForPath(gluePath)))
                    pte->suppressNextExternalChange();
            }
        }
    }

    // Rebuild index and refresh completions
    if (filesChanged > 0) {
        QStringList stFiles;
        for (auto* proj : m_solution->projects())
            for (auto* file : proj->files())
                if (file->type() == FileType::SpecTable)
                    stFiles.append(file->absolutePath());
        m_specTableIndex->rebuildProject(stFiles);
        for (auto* ed : m_mainWindow->allOpenEditors())
            if (auto* ste = qobject_cast<SpecTableEditor*>(ed))
                ste->refreshDynamicCompletions();
    }

    QString msg = tr("Renamed '%1' to '%2': %3 occurrence(s) in %4 .spectable file(s).")
                    .arg(oldName, newName).arg(totalReplaced).arg(filesChanged);
    if (glueFilesChanged > 0)
        msg += tr("\nAlso updated %1 glue file(s).").arg(glueFilesChanged);
    QMessageBox::information(m_mainWindow, tr("Rename Complete"), msg);
}

void AppController::onSymbolAtCursor(const QString& name)
{
    auto* panel = m_mainWindow->attributeInspector();
    if (!panel) return;
    if (name.isEmpty())
        panel->clear();
    else
        panel->showSymbol(name, m_specTableIndex);
}

void AppController::navigateToLine(const QString& filePath, int line)
{
    onOpenFile(filePath);
    auto* ed = m_mainWindow->editorTabs()->editorForPath(filePath);
    if (auto* pte = qobject_cast<PlainTextEditor*>(ed)) {
        QTextDocument* doc = pte->textEdit()->document();
        QTextBlock block = doc->findBlockByLineNumber(qMax(0, line - 1));
        QTextCursor cursor = block.isValid() ? QTextCursor(block) : QTextCursor(doc);
        pte->textEdit()->setTextCursor(cursor);
        pte->textEdit()->ensureCursorVisible();
    }
}

void AppController::findReferencesForSymbol(const QString& symbolName)
{
    if (!m_solution || m_solution->projects().isEmpty()) return;

    const QRegularExpression re(
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(symbolName)));

    QList<Diagnostic> results;
    for (auto* proj : m_solution->projects()) {
        for (auto* file : proj->files()) {
            if (file->type() != FileType::SpecTable) continue;
            QFile f(file->absolutePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            int lineNum = 1;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (re.match(line).hasMatch()) {
                    Diagnostic d;
                    d.filePath = file->absolutePath();
                    d.line     = lineNum;
                    d.message  = line.trimmed();
                    d.severity = Diagnostic::Severity::Info;
                    results.append(d);
                }
                ++lineNum;
            }
        }
    }

    m_mainWindow->outputPanel()->setFindResults(results, symbolName);
    m_mainWindow->outputPanel()->showFindResultsTab();
}

void AppController::findStepUsages(const QString& keyword, const QString& stepText)
{
    if (!m_solution || m_solution->projects().isEmpty()) return;

    // Match lines where the step keyword (or And/But) is followed by the same text,
    // optionally trailed by : AttrSetName. Search is case-insensitive for keyword.
    const QString escapedText = QRegularExpression::escape(stepText);
    const QRegularExpression re(
        QStringLiteral(R"(^\s*(?:Given|When|Then|And|But)\s+%1\s*(?::.*)?$)").arg(escapedText),
        QRegularExpression::CaseInsensitiveOption);

    const QString termLabel = keyword + " " + stepText;
    QList<Diagnostic> results;

    for (auto* proj : m_solution->projects()) {
        for (auto* file : proj->files()) {
            if (file->type() != FileType::SpecTable) continue;
            QFile f(file->absolutePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream in(&f);
            int lineNum = 1;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (re.match(line).hasMatch()) {
                    Diagnostic d;
                    d.filePath = file->absolutePath();
                    d.line     = lineNum;
                    d.message  = line.trimmed();
                    d.severity = Diagnostic::Severity::Info;
                    results.append(d);
                }
                ++lineNum;
            }
        }
    }

    m_mainWindow->outputPanel()->setFindResults(results, termLabel);
    m_mainWindow->outputPanel()->showFindResultsTab();
}

void AppController::applyFonts()
{
    qApp->setFont(m_settings->uiFont());
    m_mainWindow->outputPanel()->setOutputFont(m_settings->outputFont());
    const QFont edFont = m_settings->editorFont();
    for (auto* ed : m_mainWindow->allOpenEditors())
        if (auto* pte = qobject_cast<PlainTextEditor*>(ed))
            pte->textEdit()->setFont(edFont);
}

void AppController::applyAutoReload()
{
    const bool autoReload = m_settings->autoReloadFiles();
    for (auto* ed : m_mainWindow->allOpenEditors())
        if (auto* pte = qobject_cast<PlainTextEditor*>(ed))
            pte->setAutoReload(autoReload);
}

void AppController::onOpenFile(const QString& absolutePath)
{
    if (absolutePath.isEmpty()) return;
    m_mainWindow->editorTabs()->openFile(absolutePath);

    // Apply editor font and auto-reload setting to newly opened editor
    if (auto* pte = qobject_cast<PlainTextEditor*>(
            m_mainWindow->editorForPath(absolutePath))) {
        pte->textEdit()->setFont(m_settings->editorFont());
        pte->setAutoReload(m_settings->autoReloadFiles());
    }

    // Give SpecTableEditor access to the project index for context menu features
    if (auto* ste = qobject_cast<SpecTableEditor*>(
            m_mainWindow->editorForPath(absolutePath)))
    {
        // Rebuild the spec table index now so symbols are available before the
        // user runs Analyze. Include the opened file even if the project hasn't
        // scanned it yet (e.g. freshly created file).
        QStringList specTableFiles;
        if (m_solution) {
            for (auto* proj : m_solution->projects())
                for (auto* file : proj->files())
                    if (file->type() == FileType::SpecTable)
                        specTableFiles.append(file->absolutePath());
        }
        if (!specTableFiles.contains(absolutePath))
            specTableFiles.append(absolutePath);
        m_specTableIndex->rebuildProject(specTableFiles);

        ste->setIndex(m_specTableIndex);

        connect(ste, &SpecTableEditor::goToDefinitionRequested,
                this, &AppController::navigateToLine, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::findReferencesRequested,
                this, &AppController::findReferencesForSymbol, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::findStepUsagesRequested,
                this, &AppController::findStepUsages, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::renameSymbolRequested,
                this, &AppController::renameSpecTableSymbol, Qt::UniqueConnection);

        connect(ste, &SpecTableEditor::symbolAtCursor,
                this, &AppController::onSymbolAtCursor, Qt::UniqueConnection);
    }
}

void AppController::openRecentSolution(const QString& sspecPath)
{
    loadSolution(sspecPath);
}
