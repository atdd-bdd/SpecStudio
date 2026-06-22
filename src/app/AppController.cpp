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
#include "../git/GitClient.h"
#include "../editors/BaseEditor.h"
#include "../editors/PlainTextEditor.h"
#include "../editors/SpecTableEditor.h"
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include <QApplication>
#include <QDir>
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
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileDeleteRequested,
            this, &AppController::onDeleteFile);
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

void AppController::onBuildCurrentFile()
{
    auto* ed = m_mainWindow->currentEditor();
    if (!ed) {
        QMessageBox::information(m_mainWindow, tr("No File"),
            tr("Open a file to build."));
        return;
    }

    // Translator program can be stored in settings in future; empty for now
    QString translator; // TODO: read from m_settings once translator setting is added

    m_mainWindow->outputPanel()->clearBuildOutput();
    m_mainWindow->outputPanel()->showBuildTab();

    // Disconnect any previous lambda connections from prior build runs before reconnecting
    disconnect(m_builder, &BuildController::outputReady,  this, nullptr);
    disconnect(m_builder, &BuildController::buildFinished, this, nullptr);

    m_buildAccum.clear();
    connect(m_builder, &BuildController::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::outputReady,
            this, [this](const QString& text) { m_buildAccum += text; });
    connect(m_builder, &BuildController::buildFinished,
            this, [this](bool success) {
                auto diags = BuildOutputParser::parse(m_buildAccum);
                if (!diags.isEmpty())
                    m_mainWindow->outputPanel()->setDiagnostics(diags);
                if (!success)
                    m_mainWindow->outputPanel()->appendBuildOutput(tr("Build failed."));
            });

    m_builder->buildFile(ed->filePath(), translator);
}

void AppController::onBuildProject()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }

    QString translator; // TODO: read from m_settings

    m_mainWindow->outputPanel()->clearBuildOutput();
    m_mainWindow->outputPanel()->showBuildTab();

    disconnect(m_builder, &BuildController::outputReady,  this, nullptr);
    disconnect(m_builder, &BuildController::buildFinished, this, nullptr);

    m_buildAccum.clear();
    connect(m_builder, &BuildController::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::outputReady,
            this, [this](const QString& text) { m_buildAccum += text; });
    connect(m_builder, &BuildController::buildFinished,
            this, [this](bool success) {
                auto diags = BuildOutputParser::parse(m_buildAccum);
                if (!diags.isEmpty())
                    m_mainWindow->outputPanel()->setDiagnostics(diags);
                if (!success)
                    m_mainWindow->outputPanel()->appendBuildOutput(tr("Build failed."));
            });

    for (auto* proj : m_solution->projects())
        m_builder->buildProject(proj, translator);
}

void AppController::onAnalyze()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }

    QList<Diagnostic> all;
    QStringList allTags;
    for (auto* proj : m_solution->projects()) {
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
    }

    m_mainWindow->outputPanel()->setDiagnostics(all);
    m_mainWindow->outputPanel()->showAnalysisTab();

    // Push error squiggles to any open editors (both panes)
    QMap<QString, QList<QPair<int,int>>> marksByFile;
    for (const auto& d : all)
        marksByFile[d.filePath].append({d.line, d.column});
    for (auto it = marksByFile.cbegin(); it != marksByFile.cend(); ++it) {
        if (auto* ed = m_mainWindow->editorForPath(it.key()))
            ed->setErrorMarks(it.value());
    }

    // Push collected tags to all open editors so @ autocomplete works
    for (auto* ed : m_mainWindow->allOpenEditors()) {
        ed->setTagCompletionWords(allTags);
        // Refresh SpecTable symbol completions from the rebuilt index
        if (auto* ste = qobject_cast<SpecTableEditor*>(ed))
            ste->refreshDynamicCompletions();
    }

    // Refresh visualization panels
    if (auto* panel = m_mainWindow->entityTree())
        panel->refresh(m_specTableIndex);
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

    QMessageBox::information(m_mainWindow, tr("Rename Complete"),
        tr("Renamed '%1' to '%2': %3 occurrence(s) in %4 file(s).")
            .arg(oldName, newName).arg(totalReplaced).arg(filesChanged));
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

void AppController::applyFonts()
{
    qApp->setFont(m_settings->uiFont());
    m_mainWindow->outputPanel()->setOutputFont(m_settings->outputFont());
    const QFont edFont = m_settings->editorFont();
    for (auto* ed : m_mainWindow->allOpenEditors())
        if (auto* pte = qobject_cast<PlainTextEditor*>(ed))
            pte->textEdit()->setFont(edFont);
}

void AppController::onOpenFile(const QString& absolutePath)
{
    if (absolutePath.isEmpty()) return;
    m_mainWindow->editorTabs()->openFile(absolutePath);

    // Apply editor font to newly opened editor
    if (auto* pte = qobject_cast<PlainTextEditor*>(
            m_mainWindow->editorForPath(absolutePath)))
        pte->textEdit()->setFont(m_settings->editorFont());

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
