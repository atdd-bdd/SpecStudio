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
#include "../ui/dialogs/NewSolutionDialog.h"
#include "../ui/dialogs/NewProjectDialog.h"
#include "../ui/dialogs/NewFileDialog.h"
#include "../ui/dialogs/GitPushDialog.h"
#include "../ui/dialogs/SettingsDialog.h"
#include "AppSettings.h"
#include "../analyzer/ProjectIndex.h"
#include "../analyzer/FeatureXAnalyzer.h"
#include "../build/BuildController.h"
#include "../build/BuildOutputParser.h"
#include "../git/GitClient.h"
#include "../editors/BaseEditor.h"
#include "../editors/PlainTextEditor.h"
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QTreeView>

AppController::AppController(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    m_settings  = new AppSettings();
    m_index     = new ProjectIndex();
    m_analyzer  = new FeatureXAnalyzer(m_settings, m_index);
    m_builder   = new BuildController(this);
    m_treeModel = new SolutionTreeModel(this);
    mainWindow->solutionExplorer()->setModel(m_treeModel);

    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileDoubleClicked,
            this, &AppController::onOpenFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::newFileRequested,
            this, &AppController::onNewFile);
    connect(mainWindow->editorTabs(), &EditorTabWidget::fileOpenRequested,
            this, &AppController::onOpenFile);

    connect(mainWindow->outputPanel(), &OutputPanel::diagnosticActivated,
            this, [this](const QString& filePath, int line) {
                onOpenFile(filePath);
                // Navigate to line in the opened editor
                auto* ed = m_mainWindow->editorTabs()->editorForPath(filePath);
                if (auto* pte = qobject_cast<PlainTextEditor*>(ed)) {
                    QTextCursor cursor = pte->textEdit()->document()->findBlockByLineNumber(
                        qMax(0, line - 1)).position() >= 0
                        ? QTextCursor(pte->textEdit()->document()->findBlockByLineNumber(
                            qMax(0, line - 1)))
                        : QTextCursor(pte->textEdit()->document());
                    pte->textEdit()->setTextCursor(cursor);
                    pte->textEdit()->ensureCursorVisible();
                }
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
    if (!m_solution) {
        QMessageBox::information(m_mainWindow, tr("No Solution"),
            tr("Open or create a solution first."));
        return;
    }

    NewProjectDialog dlg(m_solution, m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    QString name    = dlg.projectName();
    QString projDir = m_solution->rootPath() + QDir::separator() + name;

    QDir dir(projDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::critical(m_mainWindow, tr("Error"),
            tr("Cannot create project folder: %1").arg(projDir));
        return;
    }

    // Run git init via a temporary GitClient
    {
        GitClient initGit(projDir);
        bool ok = false;
        connect(&initGit, &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput);
        connect(&initGit, &GitClient::errorOccurred,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput);

        // runGit is private, so use commitAll with no files to trigger git init indirectly.
        // Instead, expose a dedicated init via QProcess here.
        QProcess proc;
        proc.setWorkingDirectory(projDir);
        proc.start("git", {"init"});
        ok = proc.waitForFinished(10000) && proc.exitCode() == 0;

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

    auto* project = new Project(name, projDir);
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
    auto* ed = qobject_cast<PlainTextEditor*>(m_mainWindow->editorTabs()->currentEditor());
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

void AppController::onBuildCurrentFile()
{
    auto* ed = m_mainWindow->editorTabs()->currentEditor();
    if (!ed) {
        QMessageBox::information(m_mainWindow, tr("No File"),
            tr("Open a file to build."));
        return;
    }

    // Translator program can be stored in settings in future; empty for now
    QString translator; // TODO: read from m_settings once translator setting is added

    m_mainWindow->outputPanel()->clearBuildOutput();
    m_mainWindow->outputPanel()->showBuildTab();

    QString accum;
    connect(m_builder, &BuildController::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::outputReady,
            this, [&accum](const QString& text) { accum += text; },
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::buildFinished,
            this, [this, &accum](bool success) {
                auto diags = BuildOutputParser::parse(accum);
                if (!diags.isEmpty())
                    m_mainWindow->outputPanel()->setDiagnostics(diags);
                if (!success)
                    m_mainWindow->outputPanel()->appendBuildOutput(tr("Build failed."));
            }, Qt::UniqueConnection);

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

    QString accum;
    connect(m_builder, &BuildController::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::outputReady,
            this, [&accum](const QString& text) { accum += text; },
            Qt::UniqueConnection);
    connect(m_builder, &BuildController::buildFinished,
            this, [this, &accum](bool success) {
                auto diags = BuildOutputParser::parse(accum);
                if (!diags.isEmpty())
                    m_mainWindow->outputPanel()->setDiagnostics(diags);
                if (!success)
                    m_mainWindow->outputPanel()->appendBuildOutput(tr("Build failed."));
            }, Qt::UniqueConnection);

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
    for (auto* proj : m_solution->projects()) {
        m_index->rebuild(proj);
        auto diags = m_analyzer->analyzeProject(proj);
        all.append(diags);
    }

    m_mainWindow->outputPanel()->setDiagnostics(all);
    m_mainWindow->outputPanel()->showAnalysisTab();
}

void AppController::onOpenFile(const QString& absolutePath)
{
    if (absolutePath.isEmpty()) return;
    m_mainWindow->editorTabs()->openFile(absolutePath);
}

void AppController::openRecentSolution(const QString& sspecPath)
{
    loadSolution(sspecPath);
}
