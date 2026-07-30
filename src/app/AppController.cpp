#include "AppController.h"
#include "../ToolPath.h"
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
#include "../ui/dialogs/ShareChangesDialog.h"
#include "../ui/dialogs/SettingsDialog.h"
#include "../ui/dialogs/RepositorySettingsDialog.h"
#include "../ui/dialogs/GitHubRemoteSetupDialog.h"
#include "../ui/dialogs/SharingModeDialog.h"
#include "../ui/dialogs/CloneSolutionDialog.h"
#include "AppSettings.h"
#include "../git/GitInstaller.h"
#include "../git/GitHubClient.h"
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
#include <QSet>
#include <QTimer>
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
    m_treeModel->setShowAllFiles(m_settings->showAllFiles());
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
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::fileCopyRequested,
            this, &AppController::onCopyFile);
    connect(mainWindow->solutionExplorer(), &SolutionExplorer::filePasteRequested,
            this, &AppController::onPasteFile);
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
        // Auto-analyze all projects after load
        QTimer::singleShot(0, this, [this] {
            if (m_solution && !m_solution->projects().isEmpty())
                doAnalyze(m_solution->projects());
        });
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

    // Shared-Files solutions make no git calls at all. GitHub-mode solutions
    // get everyone else's changes silently — no confirmation dialog on the
    // normal path, only a warning if it couldn't be done (e.g. offline).
    if (sol->sharingMode() == Solution::SharingMode::GitHub && !sol->projects().isEmpty()) {
        GitClient* git = gitFor(sol->projects().first());
        connect(git, &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);

        const QString branch = m_settings->solutionGitBranch(sol->rootPath());
        const bool ok = git->pullRebase("origin", branch);

        if (!ok) {
            const QStringList conflicts = git->conflictedFiles();
            if (!conflicts.isEmpty()) {
                showConflictDialog(sol->projects().first(), git, sol->rootPath(), conflicts);
            } else {
                QMessageBox::warning(m_mainWindow, tr("Could Not Get Latest"),
                    tr("Could not get everyone else's changes (you may be offline).\n"
                       "Your local copy may not be up to date."));
            }
        } else {
            for (auto* proj : sol->projects()) proj->scanFiles();
            m_treeModel->refresh();
        }
    }
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

    if (!configureNewSolutionSharing(solution, dlg.useGitHub())) {
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
        SharingModeDialog modeDlg(newSol->name(), m_mainWindow);
        const bool useGitHub = (modeDlg.exec() == QDialog::Accepted) && modeDlg.useGitHub();
        if (!configureNewSolutionSharing(newSol, useGitHub)) {
            delete newSol;
            return;
        }
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
        SharingModeDialog modeDlg(newSol->name(), m_mainWindow);
        const bool useGitHub = (modeDlg.exec() == QDialog::Accepted) && modeDlg.useGitHub();
        if (!configureNewSolutionSharing(newSol, useGitHub)) {
            delete newSol;
            return;
        }
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

    // Run git init once at the solution root — every project in the solution
    // shares that one repo. Shared-Files solutions make no git calls at all.
    if (m_solution->sharingMode() == Solution::SharingMode::GitHub) {
        const QString initDir = m_solution->rootPath();
        const bool alreadyInitialized = QDir(initDir + "/.git").exists();

        if (!alreadyInitialized) {
            QProcess proc;
            proc.setWorkingDirectory(initDir);
            proc.start("git", {"init"});
            const bool ok = proc.waitForFinished(10000) && proc.exitCode() == 0;
            if (!ok) {
                QMessageBox::warning(m_mainWindow, tr("Git Init Failed"),
                    tr("Could not run 'git init' in '%1'.\n"
                       "Make sure git is installed and on your PATH.\n\n"
                       "The project was created but has no git repository.")
                    .arg(initDir));
            } else {
                m_mainWindow->outputPanel()->appendBuildOutput(
                    tr("Initialized git repository in %1").arg(initDir));
            }
        }
    }

    // Write a build configuration so the project is ready to translate.
    //
    // This used to write a file called ".specconfig" -- no name at all. Two
    // things went wrong with that. Project::scanFiles skips anything beginning
    // with a dot, so it never appeared in the Solution Explorer; and it held a
    // default-constructed SpecConfig, which means C# with MSTest, not what
    // anyone creating a project here is likely to want. The effect was a
    // project that looked as though it had no configuration.
    //
    // Named after the language, matching the convention the example projects
    // use (Java.specconfig, CSharp.specconfig, ...), so a project can hold one
    // per target language and the Configuration menu can tell them apart.
    {
        const QString cfgPath = projDir + QDir::separator() + "Java.specconfig";
        if (!QFile::exists(cfgPath)) {
            SpecConfig cfg;
            cfg.language        = "Java";
            cfg.framework       = "JUnit";
            cfg.namespacePrefix = "spectable";

            // Relative to the .specconfig, and laid out the way a Maven or
            // Gradle project expects, so the generated tests land somewhere a
            // Java build already looks.
            cfg.outputDirectory          = "src/test/java/spectable";
            cfg.createProductionClasses  = true;
            cfg.productionClassesDir     = "src/main/java/production";
            cfg.productionClassesPackage = "production";

            if (!cfg.save(cfgPath))
                QMessageBox::warning(m_mainWindow, tr("Configuration Not Written"),
                    tr("Could not write the build configuration:\n%1\n\n"
                       "The project was created. Add a configuration with "
                       "File > New File before building.").arg(cfgPath));
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

void AppController::onCloneSolution()
{
    if (!GitInstaller::ensureGitInstalled(m_mainWindow, tr("clone a solution"))) {
        QMessageBox::critical(m_mainWindow, tr("Cannot Clone"),
            tr("Cannot clone due to git not being installed."));
        return;
    }

    CloneSolutionDialog dlg(m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString url        = dlg.remoteUrl();
    const QString parentDir  = dlg.localDirectory();

    QDir dir(parentDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        QMessageBox::critical(m_mainWindow, tr("Error"),
            tr("Cannot create folder: %1").arg(parentDir));
        return;
    }

    QProcess proc;
    proc.setWorkingDirectory(parentDir);
    GitClient::applyCredentialEnv(proc, dlg.username(), dlg.personalAccessToken());
    proc.start("git", {"clone", url});
    // Clones can take a while for large repos — no fixed timeout.
    const bool ok = proc.waitForFinished(-1) && proc.exitCode() == 0;
    if (!ok) {
        QMessageBox::critical(m_mainWindow, tr("Clone Failed"),
            tr("git clone failed:\n%1").arg(QString::fromUtf8(proc.readAllStandardError())));
        return;
    }

    const QString repoDirName = QFileInfo(url).completeBaseName();
    const QString clonedRoot  = parentDir + "/" + repoDirName;

    QDir cloned(clonedRoot);
    const QStringList sspecs = cloned.entryList({"*.sspec"}, QDir::Files);
    if (sspecs.isEmpty()) {
        QMessageBox::warning(m_mainWindow, tr("No Solution File"),
            tr("The cloned repository does not contain an .sspec file."));
        return;
    }

    QString sspecName = sspecs.first();
    if (sspecs.size() > 1) {
        bool chosen = false;
        sspecName = QInputDialog::getItem(m_mainWindow, tr("Choose Solution"),
            tr("The cloned repository contains more than one solution file:"),
            sspecs, 0, false, &chosen);
        if (!chosen) return;
    }

    if (!dlg.username().isEmpty() || !dlg.personalAccessToken().isEmpty()) {
        m_settings->setSolutionGitUser(clonedRoot, dlg.username());
        m_settings->setSolutionGitPassword(clonedRoot, dlg.personalAccessToken());
    }

    loadSolution(clonedRoot + "/" + sspecName);
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

        if (m_solution && m_solution->sharingMode() == Solution::SharingMode::GitHub) {
            Project* proj = m_solution->projectForFile(ed->filePath());
            if (proj) {
                autoCommit(proj, tr("Auto-save"));
            }
        }
    }
}

void AppController::onSaveAs()
{
    auto* tabs = m_mainWindow->editorTabs();
    auto* ed   = tabs->currentEditor();
    if (!ed) return;

    const QString oldPath = ed->filePath();
    const QString dir     = QFileInfo(oldPath).absolutePath();
    const QString filter  = QString("Files (*%1);;All Files (*)").arg(QFileInfo(oldPath).suffix().isEmpty()
                                ? "" : "." + QFileInfo(oldPath).suffix());
    const QString newPath = QFileDialog::getSaveFileName(
        m_mainWindow, tr("Save As"), dir, filter);
    if (newPath.isEmpty()) return;

    // Flush in-memory content to the original path, then copy to new path
    ed->save();
    QFile::remove(newPath);  // remove if exists (user confirmed in dialog)
    if (!QFile::copy(oldPath, newPath)) {
        QMessageBox::critical(m_mainWindow, tr("Save As Failed"),
            tr("Could not write to '%1'.").arg(newPath));
        return;
    }
    onOpenFile(newPath);
}

void AppController::onSaveAll()
{
    m_mainWindow->editorTabs()->saveAllFiles();
    // Every project in the solution shares one repo — commit once, not once per
    // project. Shared-Files solutions make no git calls at all.
    if (m_solution) {
        // One repo per solution, so this commits once rather than once per
        // project — hence not autoCommit(), which is per-project. Same two
        // guards: Shared-Files makes no git calls, and a solution whose
        // 'git init' never succeeded has nothing to commit to.
        if (!m_solution->projects().isEmpty() &&
            m_solution->sharingMode() == Solution::SharingMode::GitHub &&
            QDir(m_solution->git()->repoPath() + "/.git").exists()) {
            connect(m_solution->git(), &GitClient::outputReady,
                    m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                    Qt::UniqueConnection);
            m_solution->git()->commitAll(tr("Auto-save all"));
            promptAndMaybePush();
        }
        // Re-analyze after saving so diagnostics reflect the saved state
        QTimer::singleShot(0, this, [this] {
            if (m_solution && !m_solution->projects().isEmpty())
                doAnalyze(m_solution->projects());
        });
    }
}

void AppController::promptAndMaybePush()
{
    QMap<Project*, GitClient*> gitClients;
    for (auto* proj : m_solution->projects())
        gitClients.insert(proj, gitFor(proj));

    ShareChangesDialog dlg(m_solution->projects(), gitClients, m_mainWindow);
    // The local commit already happened via onSaveAll()'s own commitAll() call
    // just above, so hasAnyChanges() here reflects only what's ahead of the
    // remote — if there's truly nothing new, skip the dialog silently.
    if (!dlg.hasAnyChanges()) return;

    dlg.exec();
    if (dlg.shareResult() != ShareChangesDialog::ShareResult::SharePushed) return;

    QString reason = dlg.description();
    if (reason.isEmpty()) reason = tr("Update");

    GitClient* git = m_solution->git();
    QString branch = m_settings->solutionGitBranch(m_solution->rootPath());
    if (branch.isEmpty()) branch = "main";

    if (!git->commitAndPush(reason, "origin", branch)) {
        QMessageBox::warning(m_mainWindow, tr("Trouble Sharing Changes"),
            tr("Your changes were saved locally, but could not be pushed.\n"
               "Try \"Get Others' Changes\" first, then share again."));
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

void AppController::onRepositorySettings()
{
    if (!m_solution) {
        QMessageBox::information(m_mainWindow, tr("No Solution"),
            tr("Open a solution first."));
        return;
    }

    RepositorySettingsDialog dlg(m_settings, m_solution, m_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;

    QString error;
    if (!SolutionSerializer::save(m_solution, error))
        QMessageBox::warning(m_mainWindow, tr("Save Warning"), error);
}

void AppController::onShareWithGit()
{
    if (!m_solution) return;
    // The user choosing this menu item *is* the choice — no radio prompt
    // needed, just reuse the same GitHub-setup flow used at solution creation.
    configureNewSolutionSharing(m_solution, /*useGitHub=*/true);
}

bool AppController::shareChanges()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project before committing."));
        return false;
    }

    QMap<Project*, GitClient*> gitClients;
    for (auto* proj : m_solution->projects())
        gitClients.insert(proj, gitFor(proj));

    ShareChangesDialog dlg(m_solution->projects(), gitClients, m_mainWindow);
    if (!dlg.hasAnyChanges()) {
        QMessageBox::information(m_mainWindow, tr("Nothing to Share"),
            tr("There are no changes to share right now."));
        return false;
    }
    dlg.exec();
    if (dlg.shareResult() == ShareChangesDialog::ShareResult::Cancelled) return false;

    QString reason = dlg.description();
    if (reason.isEmpty()) reason = tr("Update");  // git requires a non-empty commit message
    m_mainWindow->outputPanel()->showBuildTab();
    m_mainWindow->outputPanel()->appendBuildOutput(tr("--- Share Changes ---"));

    QStringList failedProjects;
    {
        // Every project resolves to the same shared GitClient -- act once.
        GitClient* git = m_solution->git();
        connect(git, &GitClient::outputReady,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);
        connect(git, &GitClient::errorOccurred,
                m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
                Qt::UniqueConnection);

        QString remote = "origin";
        QString branch = m_settings->solutionGitBranch(m_solution->rootPath());
        if (branch.isEmpty()) branch = "main";

        const bool ok = dlg.shareResult() == ShareChangesDialog::ShareResult::SharePushed
            ? git->commitAndPush(reason, remote, branch)
            : git->commitAll(reason);
        if (!ok) failedProjects << m_solution->name();
    }

    if (!failedProjects.isEmpty()) {
        QMessageBox mb(QMessageBox::Warning,
            tr("Trouble Sharing Changes"),
            tr("Your changes were saved, but could not be shared for: %1.\n\n"
               "Try \"Get Others' Changes\" first, then share again.")
                .arg(failedProjects.join(", ")),
            QMessageBox::Ok, m_mainWindow);
        mb.setDetailedText(tr("Check the Output panel for the full git error text."));
        mb.exec();
        return false;
    }

    m_mainWindow->outputPanel()->appendBuildOutput(tr("Your changes have been shared."));
    return true;
}

void AppController::onCommitAndPush()
{
    shareChanges();
}

void AppController::promptShareOnExit()
{
    if (!m_solution || m_solution->projects().isEmpty()) return;
    // Shared-Files solutions have nothing to push, and must make no git calls at all.
    if (m_solution->sharingMode() != Solution::SharingMode::GitHub) return;

    if (!gitFor(m_solution->projects().first())->hasUncommittedChanges()) return;

    const auto reply = QMessageBox::question(m_mainWindow, tr("Share Changes?"),
        tr("You have changes that haven't been shared yet.\n"
           "Share them with everyone before closing?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes)
        shareChanges();
}

// Resolve what a diff command should act on: the repository holding the current
// file, the path git knows it by, and a title for the pane.
bool AppController::resolveDiffTarget(GitClient** gitOut, QString* relPathOut, QString* titleOut)
{
    auto* ed = m_mainWindow->currentEditor();
    if (!ed || !m_solution) {
        QMessageBox::information(m_mainWindow, tr("No File"), tr("Open a file first."));
        return false;
    }

    const QString filePath = ed->filePath();
    Project* ownerProject  = nullptr;
    for (auto* proj : m_solution->projects())
        if (filePath.startsWith(proj->rootPath())) { ownerProject = proj; break; }

    if (!ownerProject) {
        QMessageBox::information(m_mainWindow, tr("Not in Project"),
            tr("The current file is not part of any project."));
        return false;
    }

    GitClient* git = gitFor(ownerProject);
    if (!git || !QDir(git->repoPath() + "/.git").exists()) {
        QMessageBox::information(m_mainWindow, tr("No Repository"),
            tr("This solution has no git repository, so there are no earlier "
               "versions to compare against."));
        return false;
    }

    // Relative to the repository, not to the project. In GitHub sharing mode one
    // repository covers the whole solution, so a path relative to the project
    // folder names nothing git knows about: asking about
    // "AccountWithdrawal.spectable" when the repository holds
    // "TestProject/AccountWithdrawal.spectable" matched no file at all and
    // returned an empty diff, which read as "nothing changed".
    *gitOut     = git;
    *relPathOut = QDir(git->repoPath()).relativeFilePath(filePath);
    *titleOut   = QFileInfo(filePath).fileName();
    return true;
}

// Compare the file as it is now against how it was `stepsBack` versions ago,
// counting only commits that actually touched this file. One step back is the
// state before the most recent change to it.
void AppController::diffAgainstVersionsBack(int stepsBack)
{
    GitClient* git = nullptr;
    QString relPath, title;
    if (!resolveDiffTarget(&git, &relPath, &title)) return;

    const QList<GitClient::FileVersion> versions = git->fileVersions(relPath, stepsBack + 1);
    if (versions.size() <= stepsBack) {
        m_mainWindow->outputPanel()->showDiff(
            versions.isEmpty()
                ? tr("This file has no history in the repository yet.")
                : tr("This file only has %n version(s) in the repository, so there is "
                     "nothing that far back.", "", int(versions.size())),
            title);
        return;
    }

    const GitClient::FileVersion& v = versions[stepsBack];
    showVersionDiff(git, relPath, title, v.commit, v.date, v.subject);
}

// Takes the version's fields rather than a GitClient::FileVersion so that
// AppController.h can keep its forward declaration of GitClient.
void AppController::showVersionDiff(GitClient* git, const QString& relPath,
                                     const QString& title, const QString& commit,
                                     const QString& date, const QString& subject)
{
    const QString diffText = git->diffAgainst(commit, relPath);
    const QString header   = tr("--- against %1  (%2) ---\n\n")
                                 .arg(date, subject.isEmpty() ? tr("no message") : subject);

    if (diffText.trimmed().isEmpty()) {
        m_mainWindow->outputPanel()->showDiff(
            header + tr("This file is identical to that version."), title);
        return;
    }
    m_mainWindow->outputPanel()->showDiff(header + diffText, title);
}

void AppController::onDiffCurrentFile()      { diffAgainstVersionsBack(1); }
void AppController::onDiffTwoVersionsBack()  { diffAgainstVersionsBack(2); }

// Pick any earlier version of this file by date and message.
void AppController::onDiffChooseVersion()
{
    GitClient* git = nullptr;
    QString relPath, title;
    if (!resolveDiffTarget(&git, &relPath, &title)) return;

    const QList<GitClient::FileVersion> versions = git->fileVersions(relPath, 100);
    if (versions.isEmpty()) {
        m_mainWindow->outputPanel()->showDiff(
            tr("This file has no history in the repository yet."), title);
        return;
    }

    QStringList labels;
    labels.reserve(versions.size());
    for (const GitClient::FileVersion& v : versions)
        labels << QStringLiteral("%1   %2").arg(
            v.date, v.subject.isEmpty() ? tr("(no message)") : v.subject);

    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        m_mainWindow, tr("Compare With Version"),
        tr("Compare %1 with which earlier version?").arg(title),
        labels, 0, false, &ok);
    if (!ok || chosen.isEmpty()) return;

    const int index = labels.indexOf(chosen);
    if (index < 0) return;
    const GitClient::FileVersion& v = versions[index];
    showVersionDiff(git, relPath, title, v.commit, v.date, v.subject);
}

void AppController::onPull()
{
    if (!m_solution || m_solution->projects().isEmpty() ||
        m_solution->sharingMode() != Solution::SharingMode::GitHub) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a GitHub-shared solution before getting changes."));
        return;
    }

    // Every project in the solution shares one repo — act once.
    Project* proj = m_solution->projects().first();
    GitClient* git = gitFor(proj);
    connect(git, &GitClient::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    connect(git, &GitClient::errorOccurred,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);

    m_mainWindow->outputPanel()->showBuildTab();
    m_mainWindow->outputPanel()->appendBuildOutput(tr("--- Get Others' Changes ---"));

    const QString branch = m_settings->solutionGitBranch(m_solution->rootPath());
    const bool ok = git->pullRebase("origin", branch);

    if (!ok) {
        const QStringList conflicts = git->conflictedFiles();
        if (!conflicts.isEmpty()) {
            showConflictDialog(proj, git, m_solution->rootPath(), conflicts);
        } else {
            m_mainWindow->outputPanel()->appendBuildOutput(tr("Pull failed — check output."));
        }
    } else {
        for (auto* p : m_solution->projects()) p->scanFiles();
        m_treeModel->refresh();
    }
}

// Auto-detect SpecTableConverter.exe: check same dir as SpecStudio, then dev build locations
static QString autoDetectConverter()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exe    = toolpath::exeName("SpecTableConverter");

    // Production: converter deployed next to SpecStudio
    const QString prod = appDir + "/" + exe;
    if (QFile::exists(prod)) return prod;

    // Development. Visual Studio nests by configuration
    // (build/src/Debug -> build/converter/Debug); single-config generators such
    // as Ninja and Makefiles do not (build/src -> build/converter), which is
    // what Linux and macOS builds use.
    for (const QString& rel : { QStringLiteral("/../../converter/Debug/"),
                                QStringLiteral("/../../converter/Release/"),
                                QStringLiteral("/../converter/") }) {
        const QString candidate = appDir + rel + exe;
        if (QFile::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
    }
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

// Resolve the production-classes directory the same way. It used to be passed
// to --prod-dir exactly as written, so a relative path resolved against
// whatever the working directory happened to be rather than against the
// project -- which meant only absolute paths worked, and a configuration
// written by hand with a relative one silently scattered files elsewhere.
static QString resolveProductionDir(const SpecConfig& cfg, const QString& configFilePath,
                                    const QString& fallbackBase)
{
    const QString dir = cfg.productionClassesDir.trimmed();
    if (dir.isEmpty()) return {};
    if (QFileInfo(dir).isAbsolute()) return dir;

    const QString base = configFilePath.isEmpty()
        ? fallbackBase : QFileInfo(configFilePath).dir().absolutePath();
    return QDir(base).absoluteFilePath(dir);
}

// Delete the generated *String / *Typed classes from one common folder, and
// report how many went.
//
// Building a whole project or solution converts every .spectable in it, so
// every one of these classes is about to be written again. Anything still
// standing afterwards belongs to an AttributeSet that was renamed or removed:
// dead code that still compiles, and in the languages with a common index, a
// stale entry pointing at it. Clearing first makes the folder reflect the
// specifications as they are now.
//
// Naming differs by language — AddressString.java, address_string.py,
// address_string.h — so both spellings are matched. Nothing else in common is
// touched: the JSON reader, the index/module file, YesNo, TableHelper and
// ProductionHelper are not per-AttributeSet, and converting a single file still
// expects to find them.
static int clearGeneratedCommonClasses(const QString& commonDir)
{
    QDir dir(commonDir);
    if (!dir.exists()) return 0;

    static const QStringList patterns = {
        "*String.*", "*Typed.*", "*_string.*", "*_typed.*"
    };

    int removed = 0;
    for (const QFileInfo& fi : dir.entryInfoList(patterns, QDir::Files))
        if (QFile::remove(fi.absoluteFilePath())) ++removed;
    return removed;
}

// Rewrite a single .spectable file's Insert "..." / Insert '...' / Insert <...> /
// Import "..." statements that resolve to oldAbsPath, pointing them at newAbsPath
// instead (as a path relative to this file). Returns true if the file was changed.
static bool rewriteInsertImportRefs(const QString& spectablePath,
                                    const QString& oldAbsPath, const QString& newAbsPath)
{
    QFile f(spectablePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString content = QTextStream(&f).readAll();
    f.close();

    const QString fileDir  = QFileInfo(spectablePath).absolutePath();
    const QString oldCanon = QFileInfo(oldAbsPath).absoluteFilePath();

    static QRegularExpression reRef(
        R"re(^([ \t]*(?:Insert|Import)[ \t]+)(?:"([^"]+)"|'([^']+)'|<([^>]+)>))re",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);

    struct Repl { qsizetype start; qsizetype len; QString text; };
    QVector<Repl> repls;

    auto it = reRef.globalMatch(content);
    while (it.hasNext()) {
        const auto m = it.next();
        const int grp = !m.captured(2).isEmpty() ? 2 : !m.captured(3).isEmpty() ? 3
                       : !m.captured(4).isEmpty() ? 4 : -1;
        if (grp < 0) continue;
        const QString ref = m.captured(grp);
        const QString refAbs = QFileInfo(fileDir + "/" + ref).absoluteFilePath();
        if (refAbs.compare(oldCanon, Qt::CaseInsensitive) != 0) continue;
        repls.push_back({ m.capturedStart(grp), m.capturedLength(grp),
                          QDir(fileDir).relativeFilePath(newAbsPath) });
    }
    if (repls.isEmpty()) return false;

    for (auto rit = repls.crbegin(); rit != repls.crend(); ++rit)
        content.replace(rit->start, rit->len, rit->text);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream(&f) << content;
    return true;
}

// Rewrite a .specconfig's externalSpectables[].file entries that resolve to
// oldAbsPath, pointing them at newAbsPath instead. Returns true if changed.
static bool rewriteSpecConfigRefs(const QString& cfgPath,
                                   const QString& oldAbsPath, const QString& newAbsPath)
{
    SpecConfig cfg = SpecConfig::load(cfgPath);
    if (cfg.externalSpectables.isEmpty()) return false;

    const QString cfgDir   = QFileInfo(cfgPath).absolutePath();
    const QString oldCanon = QFileInfo(oldAbsPath).absoluteFilePath();
    bool changed = false;
    for (ExternalSpectable& es : cfg.externalSpectables) {
        const QString abs = QFileInfo(es.file).isAbsolute()
            ? es.file : QDir(cfgDir).absoluteFilePath(es.file);
        if (QFileInfo(abs).absoluteFilePath().compare(oldCanon, Qt::CaseInsensitive) != 0)
            continue;
        es.file = QDir(cfgDir).relativeFilePath(newAbsPath);
        changed = true;
    }
    return changed && cfg.save(cfgPath);
}

// Update every .spectable Insert/Import statement and every .specconfig
// externalSpectables entry across the whole solution that points at oldAbsPath,
// so a file Move/Rename doesn't silently break references to it.
static void rewriteReferencesAfterMove(Solution* solution,
                                        const QString& oldAbsPath, const QString& newAbsPath)
{
    if (!solution) return;
    for (auto* proj : solution->projects()) {
        for (auto* pf : proj->files()) {
            if (pf->absolutePath() == oldAbsPath) continue; // the moved file itself
            if (pf->type() == FileType::SpecTable)
                rewriteInsertImportRefs(pf->absolutePath(), oldAbsPath, newAbsPath);
            else if (pf->type() == FileType::SpecConfig)
                rewriteSpecConfigRefs(pf->absolutePath(), oldAbsPath, newAbsPath);
        }
    }
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

bool AppController::configureNewSolutionSharing(Solution* solution, bool useGitHub)
{
    solution->setSharingMode(useGitHub ? Solution::SharingMode::GitHub
                                        : Solution::SharingMode::SharedFiles);

    QString error;
    if (!SolutionSerializer::save(solution, error)) {
        QMessageBox::critical(m_mainWindow, tr("Save Failed"), error);
        return false;
    }

    if (!useGitHub)
        return true; // Shared Files: no git calls at all.

    if (!GitInstaller::ensureGitInstalled(m_mainWindow, tr("set up GitHub sharing"))) {
        solution->setSharingMode(Solution::SharingMode::SharedFiles);
        SolutionSerializer::save(solution, error);
        QMessageBox::information(m_mainWindow, tr("Using Shared Files"),
            tr("Git isn't available, so this solution will use shared-file-system sharing instead."));
        return true;
    }

    const QString rootPath = solution->rootPath();
    if (!QDir(rootPath + "/.git").exists()) {
        QProcess proc;
        proc.setWorkingDirectory(rootPath);
        proc.start("git", {"init"});
        const bool ok = proc.waitForFinished(10000) && proc.exitCode() == 0;
        if (!ok) {
            QMessageBox::warning(m_mainWindow, tr("Git Init Failed"),
                tr("Could not run 'git init' in '%1'. Using shared-file-system sharing instead.")
                    .arg(rootPath));
            solution->setSharingMode(Solution::SharingMode::SharedFiles);
            SolutionSerializer::save(solution, error);
            return true;
        }
    }

    GitHubRemoteSetupDialog remoteDlg(solution->name(), m_settings->lastGitHubHost(), m_mainWindow);
    if (remoteDlg.exec() != QDialog::Accepted) {
        solution->setSharingMode(Solution::SharingMode::SharedFiles);
        SolutionSerializer::save(solution, error);
        return true;
    }

    solution->setGitHubHost(remoteDlg.host());
    m_settings->setLastGitHubHost(remoteDlg.host());

    GitHubClient gh(remoteDlg.host(), remoteDlg.personalAccessToken());
    const GitHubClient::CreateRepoResult result = gh.createPrivateRepo(remoteDlg.repoName());
    if (!result.ok) {
        QMessageBox::critical(m_mainWindow, tr("GitHub Error"),
            tr("Could not create the GitHub repository: %1\n\n"
               "Using shared-file-system sharing instead.").arg(result.errorMessage));
        solution->setSharingMode(Solution::SharingMode::SharedFiles);
        SolutionSerializer::save(solution, error);
        return true;
    }

    m_settings->setSolutionGitUser(rootPath, remoteDlg.username());
    m_settings->setSolutionGitPassword(rootPath, remoteDlg.personalAccessToken());
    m_settings->setSolutionGitRemoteUrl(rootPath, result.cloneUrl);

    // Persist the final GitHub-mode .sspec now, before the initial commit, so
    // the very first commit already reflects the chosen sharing mode.
    SolutionSerializer::save(solution, error);

    GitClient* git = solution->git();
    git->setCredentials(remoteDlg.username(), remoteDlg.personalAccessToken());
    git->addRemote("origin", result.cloneUrl);

    const QString branch = m_settings->solutionGitBranch(rootPath);
    if (!git->commitAndPush(tr("Initial commit"), "origin", branch)) {
        m_mainWindow->outputPanel()->appendBuildOutput(
            tr("Warning: could not push the initial commit to %1.").arg(result.cloneUrl));
    }

    return true;
}

void AppController::showConflictDialog(Project* proj, GitClient* git, const QString& rootPath,
                                        const QStringList& conflicts)
{
    auto* dlg = new ConflictResolutionDialog(proj->name(), git, rootPath, conflicts, m_mainWindow);
    connect(dlg, &ConflictResolutionDialog::openFileRequested, this, &AppController::onOpenFile);
    dlg->exec();
    dlg->deleteLater();
    // Reload any modified files
    proj->scanFiles();
    m_treeModel->refresh();
}

// Commit that happens as a side effect of an edit — a save, a file operation,
// a rename — rather than because the user asked for one.
//
// Two ways there is no repository to commit to: a Shared-Files solution, which
// makes no git calls at all, and a solution whose 'git init' failed or was
// never run. Both used to reach git anyway and put its error in the Output
// panel, reporting a failure for something the user never requested.
//
// Deliberately silent: there is nothing for the user to do about either case.
void AppController::autoCommit(Project* proj, const QString& message)
{
    if (!proj || !m_solution) return;
    if (m_solution->sharingMode() != Solution::SharingMode::GitHub) return;

    GitClient* git = gitFor(proj);
    if (!git) return;
    if (!QDir(git->repoPath() + "/.git").exists()) return;

    connect(git, &GitClient::outputReady,
            m_mainWindow->outputPanel(), &OutputPanel::appendBuildOutput,
            Qt::UniqueConnection);
    git->commitAll(message);
}

GitClient* AppController::gitFor(Project* proj) const
{
    if (!proj) return nullptr;
    if (m_solution) {
        if (m_solution->sharingMode() == Solution::SharingMode::GitHub) {
            const QString root = m_solution->rootPath();
            m_solution->git()->setCredentials(m_settings->solutionGitUser(root),
                                               m_settings->solutionGitPassword(root));
        }
        return m_solution->git();
    }
    return proj->git();
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

    // Locate config — prefer the active (most-recently-set) config for this project
    Project* ownerForCfg = m_solution ? m_solution->projectForFile(ed->filePath()) : nullptr;
    const QString projRoot = ownerForCfg ? ownerForCfg->rootPath() : QString();
    QString cfgPath = ownerForCfg ? resolveActiveConfig(ownerForCfg) : QString();
    if (cfgPath.isEmpty())
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
        args << "--prod-dir"
             << resolveProductionDir(cfg, cfgPath,
                                     QFileInfo(ed->filePath()).dir().absolutePath());
    if (!cfg.productionClassesPackage.isEmpty())
        args << "--prod-package" << cfg.productionClassesPackage;
    if (!cfg.failEveryTest)             args << "--no-fail-every-test";
    // Pass other .spectable files from the same project as context
    if (m_solution) {
        auto* ownerProj = m_solution->projectForFile(ed->filePath());
        if (ownerProj)
            for (auto* pf : ownerProj->files())
                if (pf->type() == FileType::SpecTable && pf->absolutePath() != ed->filePath())
                    args << "--context" << pf->absolutePath();
    }
    // External spectables from config — add as context (symbols) and their code imports
    {
        const QString cfgDir = cfgPath.isEmpty()
            ? QFileInfo(ed->filePath()).dir().absolutePath()
            : QFileInfo(cfgPath).absolutePath();
        for (const ExternalSpectable& es : cfg.externalSpectables) {
            const QString abs = QFileInfo(es.file).isAbsolute()
                ? es.file : QDir(cfgDir).absoluteFilePath(es.file);
            if (QFile::exists(abs))
                args << "--context" << abs;
            for (const QString& imp : es.codeImports)
                if (!imp.trimmed().isEmpty()) args << "--import" << imp.trimmed();
        }
    }
    if (!QDir(outDir).exists() && !QDir().mkpath(outDir)) {
        m_mainWindow->outputPanel()->appendBuildOutput(
            tr("Cannot create output directory: %1").arg(outDir));
        QMessageBox::warning(m_mainWindow, tr("Not Written"),
            tr("Output directory could not be created:\n%1\n\nNo files were written.").arg(outDir));
        return;
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

    // Resolve every project's configuration up front. resolveActiveConfig can
    // put up a dialog when a project holds more than one .specconfig, and the
    // clearing pass below would otherwise ask a second time.
    struct BuildTarget {
        Project*   proj;
        QString    cfgPath;
        SpecConfig cfg;
        QString    converter;
    };
    QList<BuildTarget> plan;
    for (auto* proj : targets) {
        // Prefer the active (most-recently-set) config for this project
        const QString cfgPath = resolveActiveConfig(proj);
        const SpecConfig cfg  = cfgPath.isEmpty() ? SpecConfig{} : SpecConfig::load(cfgPath);
        plan.append({ proj, cfgPath, cfg,
                      cfg.converterPath.isEmpty() ? autoDetectConverter()
                                                  : cfg.converterPath });
    }

    // Clear the previously generated String/Typed classes before converting
    // anything. This has to finish before the first converter starts: run()
    // below returns as soon as the process is spawned, so every conversion is
    // in flight at once, and deleting from inside that loop would throw away
    // files a sibling conversion had already written.
    // Build > Current File deliberately does not do this — one .spectable
    // cannot tell which classes belong to the specs it was not given.
    {
        QSet<QString> seen;
        int removed = 0;
        for (const BuildTarget& t : plan) {
            for (auto* pf : t.proj->files()) {
                if (pf->type() != FileType::SpecTable) continue;
                const QString commonDir =
                    QDir(resolveOutputDir(t.cfg, t.cfgPath, pf->absolutePath()))
                        .absoluteFilePath("common");
                if (seen.contains(commonDir)) continue;
                seen.insert(commonDir);
                removed += clearGeneratedCommonClasses(commonDir);
            }
        }
        if (removed > 0)
            m_mainWindow->outputPanel()->appendBuildOutput(
                tr("Cleared %1 generated String/Typed file(s) — all are rebuilt below")
                    .arg(removed));
    }

    for (const BuildTarget& t : plan) {
        Project* proj           = t.proj;
        const QString cfgPath   = t.cfgPath;
        const SpecConfig& cfg   = t.cfg;
        const QString converter = t.converter;
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
                    args << "--prod-dir"
                         << resolveProductionDir(cfg, cfgPath, proj->rootPath());
                if (!cfg.productionClassesPackage.isEmpty())
                    args << "--prod-package" << cfg.productionClassesPackage;
                if (!cfg.failEveryTest)       args << "--no-fail-every-test";
                for (auto* other : proj->files())
                    if (other->type() == FileType::SpecTable && other->absolutePath() != pf->absolutePath())
                        args << "--context" << other->absolutePath();
                // External spectables from config
                {
                    const QString cfgDir = cfgPath.isEmpty()
                        ? proj->rootPath() : QFileInfo(cfgPath).absolutePath();
                    for (const ExternalSpectable& es : cfg.externalSpectables) {
                        const QString abs = QFileInfo(es.file).isAbsolute()
                            ? es.file : QDir(cfgDir).absoluteFilePath(es.file);
                        if (QFile::exists(abs))
                            args << "--context" << abs;
                        for (const QString& imp : es.codeImports)
                            if (!imp.trimmed().isEmpty()) args << "--import" << imp.trimmed();
                    }
                }
                if (!QDir(outDir).exists() && !QDir().mkpath(outDir)) {
                    m_mainWindow->outputPanel()->appendBuildOutput(
                        tr("Cannot create output directory: %1 — skipping").arg(outDir));
                    QMessageBox::warning(m_mainWindow, tr("Not Written"),
                        tr("Output directory could not be created:\n%1\n\nFile was not written: %2")
                        .arg(outDir, pf->fileName()));
                    continue;
                }
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
    // The Configuration menu lists the .specconfig files of every project in
    // the solution, so the owning project has to come from the chosen path.
    // Using activeProject() stored the setting against whichever project
    // happened to be selected, which left the clicked entry unchecked (the
    // menu reads the setting back per project) and pointed that other project
    // at a configuration belonging to someone else.
    Project* proj = projectForConfig(configAbsPath);
    if (!proj) proj = activeProject();
    if (!proj) return;
    m_settings->setActiveBuildConfig(proj->rootPath(), configAbsPath);
    m_mainWindow->outputPanel()->appendBuildOutput(
        tr("Build configuration for %1 set to: %2")
            .arg(proj->name(), QFileInfo(configAbsPath).fileName()));
}

// The project whose root directory holds this .specconfig, or nullptr.
Project* AppController::projectForConfig(const QString& configAbsPath) const
{
    if (!m_solution) return nullptr;
    const QString cfgDir = QFileInfo(configAbsPath).absolutePath();
    for (Project* p : m_solution->projects()) {
        if (QFileInfo(p->rootPath()).absoluteFilePath() == cfgDir)
            return p;
    }
    return nullptr;
}

QString AppController::resolveActiveConfig(Project* proj)
{
    if (!proj) return {};

    const QString active = m_settings->activeBuildConfig(proj->rootPath());
    if (!active.isEmpty() && QFile::exists(active))
        return active;

    const QDir projDir(proj->rootPath());
    const QStringList cfgFiles = projDir.entryList({ "*.specconfig" }, QDir::Files, QDir::Name);
    if (cfgFiles.isEmpty())
        return {};
    if (cfgFiles.size() == 1) {
        const QString only = projDir.absoluteFilePath(cfgFiles.first());
        m_settings->setActiveBuildConfig(proj->rootPath(), only);
        return only;
    }

    // Multiple candidates and no active choice recorded — ask which to use.
    bool ok = false;
    const QString picked = QInputDialog::getItem(
        m_mainWindow, tr("Select Build Configuration"),
        tr("Project '%1' has more than one .specconfig and no active configuration is set.\n"
           "Which one should Analyze/Build use?").arg(proj->name()),
        cfgFiles, 0, false, &ok);
    if (!ok || picked.isEmpty())
        return {};

    const QString chosen = projDir.absoluteFilePath(picked);
    m_settings->setActiveBuildConfig(proj->rootPath(), chosen);
    return chosen;
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
    // Analysis rebuilds the indexes, re-marks errors and refreshes the
    // completion lists across every open editor, and shows the Analysis tab.
    // None of that should cost the user their place, so note where each caret
    // and viewport were and put them back at the end.
    struct EditorPlace { BaseEditor* editor; int cursor; int scroll; };
    QList<EditorPlace> places;
    for (auto* ed : m_mainWindow->allOpenEditors())
        places.append({ ed, ed->cursorPosition(), ed->verticalScroll() });

    // Save first, so the analysis reads what is on disk. Taking the positions
    // above this means a save that reloads the document is covered too.
    for (auto* ed : m_mainWindow->allOpenEditors())
        if (ed->isDirty()) ed->save();

    QList<Diagnostic>  all;
    QList<CoverageEntry> coverageEntries;
    QStringList allTags;

    for (auto* proj : targets) {
        const int batchStart = all.size();

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

        // Resolve external spectables from config
        QStringList externalFiles;
        {
            const QString cfgPath = resolveActiveConfig(proj);
            if (!cfgPath.isEmpty()) {
                const SpecConfig extCfg = SpecConfig::load(cfgPath);
                const QString cfgDir = QFileInfo(cfgPath).absolutePath();
                for (const ExternalSpectable& es : extCfg.externalSpectables) {
                    const QString abs = QFileInfo(es.file).isAbsolute()
                        ? es.file : QDir(cfgDir).absoluteFilePath(es.file);
                    if (QFile::exists(abs)) {
                        externalFiles << abs;
                    } else {
                        Diagnostic d;
                        d.filePath = cfgPath;
                        d.line     = 0;
                        d.message  = tr("External spectable not found: %1").arg(es.file);
                        d.severity = Diagnostic::Severity::Warning;
                        all.append(d);
                    }
                }
            }
        }

        if (!specTableFiles.isEmpty() || !externalFiles.isEmpty()) {
            m_specTableIndex->rebuildProject(specTableFiles, externalFiles);

            // Info diagnostics for each successfully loaded external file
            for (const QString& extFile : externalFiles) {
                const SpecTableSymbols sym = m_specTableIndex->symbolsForFile(extFile);
                QStringList names;
                for (auto it = sym.dataTypes.cbegin();  it != sym.dataTypes.cend();  ++it) names << it.key();
                for (auto it = sym.entities.cbegin();   it != sym.entities.cend();   ++it) names << it.key();
                for (auto it = sym.attributes.cbegin(); it != sym.attributes.cend(); ++it) names << it.key();
                names.sort();
                Diagnostic d;
                d.filePath = extFile;
                d.line     = 1;
                d.message  = tr("External: %1").arg(
                    names.isEmpty() ? tr("(no types found)") : names.join(", "));
                d.severity = Diagnostic::Severity::Info;
                all.append(d);
            }

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

        // Tag all diagnostics in this batch with the project name
        for (int i = batchStart; i < all.size(); ++i)
            all[i].projectName = proj->name();
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

    // Restore only editors that are still open — analysis can close one whose
    // file has gone away.
    const QList<BaseEditor*> stillOpen = m_mainWindow->allOpenEditors();
    for (const EditorPlace& p : places) {
        if (!stillOpen.contains(p.editor)) continue;
        if (p.cursor >= 0) p.editor->setCursorPosition(p.cursor);
        if (p.scroll >= 0) p.editor->setVerticalScroll(p.scroll);
    }
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
    doAnalyze({ active });
}

void AppController::onAnalyzeProject(const QString& projectRootPath)
{
    if (!m_solution) return;
    for (auto* p : m_solution->projects()) {
        if (p->rootPath() == projectRootPath) {
                    doAnalyze({ p });
            return;
        }
    }
}

void AppController::onAnalyzeSolution()
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Solution"),
            tr("Open a solution first."));
        return;
    }
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

    rewriteReferencesAfterMove(m_solution, absolutePath, newPath);

    if (m_solution) {
        for (auto* proj : m_solution->projects()) {
            proj->scanFiles();
            autoCommit(proj, tr("Rename %1 to %2").arg(fi.fileName(), newName));
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

    rewriteReferencesAfterMove(m_solution, absolutePath, newPath);

    for (auto* proj : m_solution->projects()) {
        proj->scanFiles();
        autoCommit(proj, tr("Move %1").arg(fileName));
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
            autoCommit(proj, tr("Delete %1").arg(name));
        }
        m_treeModel->refresh();
        m_mainWindow->solutionExplorer()->treeView()->expandAll();
    }
}

void AppController::onCopyFile(const QString& absolutePath)
{
    m_copiedFilePath = absolutePath;
    m_mainWindow->outputPanel()->appendBuildOutput(
        tr("Copied: %1").arg(QFileInfo(absolutePath).fileName()));
}

void AppController::onPasteFile(const QString& targetProjectRoot)
{
    if (m_copiedFilePath.isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("Paste"),
            tr("Nothing copied. Right-click a file and choose Copy first."));
        return;
    }

    const QFileInfo srcInfo(m_copiedFilePath);
    bool ok;
    const QString newName = QInputDialog::getText(
        m_mainWindow, tr("Paste File"),
        tr("New file name:"), QLineEdit::Normal, srcInfo.fileName(), &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    const QString destPath = targetProjectRoot + QDir::separator() + newName.trimmed();
    if (QFile::exists(destPath)) {
        QMessageBox::warning(m_mainWindow, tr("Paste Failed"),
            tr("'%1' already exists.").arg(newName.trimmed()));
        return;
    }

    if (!QFile::copy(m_copiedFilePath, destPath)) {
        QMessageBox::critical(m_mainWindow, tr("Paste Failed"),
            tr("Could not copy '%1'.").arg(srcInfo.fileName()));
        return;
    }

    if (m_solution) {
        for (auto* proj : m_solution->projects()) {
            proj->scanFiles();
            autoCommit(proj,
                       tr("Add %1 (copied from %2)").arg(newName.trimmed(), srcInfo.fileName()));
        }
        m_treeModel->refresh();
        m_mainWindow->solutionExplorer()->treeView()->expandAll();
    }
    onOpenFile(destPath);
}

void AppController::onRefreshSolution()
{
    if (!m_solution) return;
    for (auto* proj : m_solution->projects())
        proj->scanFiles();
    m_treeModel->refresh();
    m_mainWindow->solutionExplorer()->treeView()->expandAll();
}

void AppController::onToggleShowAllFiles(bool show)
{
    m_settings->setShowAllFiles(show);
    m_treeModel->setShowAllFiles(show);
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

namespace {

// Glue file names produced by the nine generators: <Spec>_glue.<ext> for eight
// of them, <Spec>Glue.swift for Swift. Listed explicitly rather than as
// "*_glue.*" so a copied .spectable or an editor backup is never rewritten.
QStringList glueFileFilters()
{
    return { "*_glue.java", "*_glue.cs",  "*_glue.py", "*_glue.go",
             "*_glue.rs",   "*_glue.js",  "*_glue.ts", "*_glue.h",
             "*_glue.cpp",  "*Glue.swift" };
}

// Where a project's glue can be. Every .specconfig names its own
// outputDirectory, and that folder is frequently not inside the project at all
// — it usually points into the language's own test tree, in another repository.
// Searching only the project root therefore finds nothing in the common case.
QStringList glueSearchRoots(Project* proj)
{
    QStringList roots{ proj->rootPath() };
    QDir rootDir(proj->rootPath());
    for (const QString& name : rootDir.entryList({ "*.specconfig" }, QDir::Files)) {
        const SpecConfig cfg = SpecConfig::load(rootDir.absoluteFilePath(name));
        QString out = cfg.outputDirectory.trimmed();
        if (out.isEmpty()) continue;
        if (!QFileInfo(out).isAbsolute()) out = rootDir.absoluteFilePath(out);
        out = QDir::cleanPath(out);
        if (!roots.contains(out) && QDir(out).exists()) roots.append(out);
    }
    return roots;
}

// Glue files under any of a project's search roots. A root nested inside
// another would otherwise be walked twice, so paths are de-duplicated.
QStringList glueFilesFor(Project* proj)
{
    QStringList found;
    for (const QString& root : glueSearchRoots(proj)) {
        QDirIterator it(root, glueFileFilters(), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = QDir::cleanPath(it.next());
            if (!found.contains(path)) found.append(path);
        }
    }
    return found;
}

// A glue method's name is derived from the step's keyword and text, in one of
// five shapes depending on the language. These mirror toMethodName/toFnName in
// the generators — keep them in step with converter/*Generator.cpp.

// Java, C#  ->  When_item_added
QString glueNameSnakeMixed(const QString& kw, const QString& text)
{
    QString s = kw + "_" + text;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    return s.remove(QRegularExpression("^_+|_+$"));
}

// Python, Rust, C++  ->  when_item_added
QString glueNameSnakeLower(const QString& kw, const QString& text)
{
    return glueNameSnakeMixed(kw, text).toLower();
}

// Go  ->  WhenItemAdded
QString glueNamePascal(const QString& kw, const QString& text)
{
    QString s = kw + " " + text;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), " ");
    QString result;
    for (const QString& p : s.split(' ', Qt::SkipEmptyParts))
        result += p[0].toUpper() + p.mid(1);
    if (!result.isEmpty() && result[0].isDigit()) result.prepend('_');
    return result;
}

// Swift  ->  whenItemAdded (the whole step is lowercased first, so interior
// capitals in the step text are flattened)
QString glueNameCamelSwift(const QString& kw, const QString& text)
{
    QString combined = (kw + " " + text).toLower();
    combined.replace(QRegularExpression(R"([^a-z0-9]+)"), " ");
    const QStringList parts = combined.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return QStringLiteral("step");
    QString result = parts[0];
    for (int i = 1; i < parts.size(); ++i)
        result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

// JavaScript, TypeScript  ->  whenItemAdded, but interior capitals survive
QString glueNameCamelJs(const QString& kw, const QString& text)
{
    QString combined = kw + "_" + text;
    combined.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    combined.remove(QRegularExpression("^_+|_+$"));
    const QStringList parts = combined.split('_', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return combined.toLower();
    QString result = parts[0].toLower();
    for (int i = 1; i < parts.size(); ++i)
        result += parts[i][0].toUpper() + parts[i].mid(1);
    return result;
}

// Every (old, new) glue-method pair a step rename could imply. "And" steps are
// recorded under the keyword they continue, so only the governing keywords are
// tried.
QVector<QPair<QString, QString>> glueMethodRenames(const QString& oldText,
                                                   const QString& newText)
{
    using NameFn = QString (*)(const QString&, const QString&);
    static const QVector<NameFn> shapes = {
        &glueNameSnakeMixed, &glueNameSnakeLower, &glueNamePascal,
        &glueNameCamelSwift, &glueNameCamelJs
    };
    static const QStringList keywords = { "Given", "When", "Then", "WhenThen" };

    QVector<QPair<QString, QString>> out;
    for (const QString& kw : keywords) {
        for (NameFn fn : shapes) {
            const QPair<QString, QString> pair{ fn(kw, oldText), fn(kw, newText) };
            if (pair.first.isEmpty() || pair.second.isEmpty()) continue;
            if (pair.first == pair.second) continue;
            if (!out.contains(pair)) out.append(pair);
        }
    }
    return out;
}

} // namespace

void AppController::onRenameStep()
{
    // Edit > Rename Step works on the step the caret is in, the same step the
    // editor's context menu would offer. A selection wins if there is one, so
    // renaming part of a step is still possible.
    QString stepText;
    if (auto* ed = qobject_cast<PlainTextEditor*>(m_mainWindow->currentEditor())) {
        stepText = ed->textEdit()->textCursor().selectedText().trimmed();
        if (stepText.isEmpty()) {
            static const QRegularExpression reStep(
                R"(^\s*(?:Given|When|Then|And|WhenThen)\s+(.+?)(?:\s*:.*)?$)",
                QRegularExpression::CaseInsensitiveOption);
            const auto m = reStep.match(ed->textEdit()->textCursor().block().text());
            if (m.hasMatch()) stepText = m.captured(1).trimmed();
        }
    }
    if (stepText.isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("Rename Step"),
            tr("Put the cursor on a Given/When/Then line, or select the text to "
               "rename, and try again."));
        return;
    }
    renameStepText(stepText);
}

void AppController::renameStepText(const QString& oldText)
{
    if (!m_solution || m_solution->projects().isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("No Project"),
            tr("Open a project first."));
        return;
    }
    if (oldText.isEmpty()) return;

    // One prompt, pre-filled with the current text — the same shape as
    // Rename Symbol. The old name is already known from the caret.
    bool ok;
    const QString newText = QInputDialog::getText(
        m_mainWindow, tr("Rename Step"),
        tr("Rename step '%1' to:").arg(oldText),
        QLineEdit::Normal, oldText, &ok);
    if (!ok || newText.isEmpty() || newText == oldText) return;

    int filesChanged = 0;
    int totalReplaced = 0;
    int glueFilesChanged = 0;
    int glueMethodsRenamed = 0;

    // A glue method is named after the step, mangled into an identifier, so the
    // literal replacement below can never reach it. Rename those separately.
    const QVector<QPair<QString, QString>> methodRenames =
        glueMethodRenames(oldText, newText);

    // Projects whose sources changed; their generated tests still call the old
    // names, so they are rebuilt once the rename is done.
    QList<Project*> touched;

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

        // Glue lives under the config's outputDirectory, which Project::scanFiles
        // deliberately skips, so it is not in proj->files() — walk it directly.
        if (!methodRenames.isEmpty()) {
            for (const QString& gluePath : glueFilesFor(proj)) {
                QFile gf(gluePath);
                if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                QString content = QTextStream(&gf).readAll();
                gf.close();

                int renamedHere = 0;
                for (const auto& pair : methodRenames) {
                    const QRegularExpression re(
                        QStringLiteral("\\b%1\\b").arg(
                            QRegularExpression::escape(pair.first)));
                    const int n = content.count(re);
                    if (n == 0) continue;
                    content.replace(re, pair.second);
                    renamedHere += n;
                }
                if (renamedHere == 0) continue;

                if (!gf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
                    continue;
                QTextStream(&gf) << content;
                gf.close();

                glueMethodsRenamed += renamedHere;
                ++glueFilesChanged;
                projModified = true;

                if (auto* ed = m_mainWindow->editorTabs()->editorForPath(gluePath))
                    ed->load(gluePath);
            }
        }

        if (projModified) {
            touched.append(proj);
            autoCommit(proj, tr("Rename: %1 → %2").arg(oldText, newText));
        }
    }

    QString summary = tr("Replaced %1 occurrence(s) in %2 file(s).")
                          .arg(totalReplaced).arg(filesChanged);
    if (glueMethodsRenamed > 0)
        summary += tr("\n\nRenamed %1 glue method(s) in %2 glue file(s).")
                       .arg(glueMethodsRenamed).arg(glueFilesChanged);
    // Deliberately not rebuilt here: a build can take a while, and it is the
    // developer's call when to spend that time.
    if (!touched.isEmpty())
        summary += tr("\n\nBuild %n project(s) — the generated tests still call "
                      "the old names.", "", touched.size());
    QMessageBox::information(m_mainWindow, tr("Rename Complete"), summary);
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
    // Projects whose sources changed; their generated tests still refer to the
    // old name, so they are rebuilt once the rename is done.
    QList<Project*> touched;
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
            touched.append(proj);
            autoCommit(proj, tr("Rename: %1 → %2").arg(oldName, newName));
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
            bool glueChangedHere = false;
            for (const QString& gluePath : glueFilesFor(proj)) {
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
                glueChangedHere = true;

                if (auto* pte = qobject_cast<PlainTextEditor*>(
                        m_mainWindow->editorForPath(gluePath)))
                    pte->suppressNextExternalChange();
            }
            if (glueChangedHere && !touched.contains(proj))
                touched.append(proj);
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
    // Deliberately not rebuilt here — see onRenameStep.
    if (!touched.isEmpty())
        msg += tr("\n\nBuild %n project(s) — the generated tests still use the "
                  "old name.", "", touched.size());
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
        QStringLiteral(R"(^\s*(?:Given|When|Then|And|WhenThen)\s+%1\s*(?::.*)?$)").arg(escapedText),
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
        if (m_solution) {
            Project* owner = m_solution->projectForFile(absolutePath);
            if (owner) ste->setProjectRoot(owner->rootPath());
            ste->setSolutionRoot(m_solution->rootPath());
        }

        connect(ste, &SpecTableEditor::goToDefinitionRequested,
                this, &AppController::navigateToLine, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::findReferencesRequested,
                this, &AppController::findReferencesForSymbol, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::findStepUsagesRequested,
                this, &AppController::findStepUsages, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::renameStepRequested,
                this, &AppController::renameStepText, Qt::UniqueConnection);
        connect(ste, &SpecTableEditor::renameSymbolRequested,
                this, &AppController::renameSpecTableSymbol, Qt::UniqueConnection);

        connect(ste, &SpecTableEditor::symbolAtCursor,
                this, &AppController::onSymbolAtCursor, Qt::UniqueConnection);
    }
}

void AppController::onOpenFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        m_mainWindow,
        tr("Open File"),
        {},
        tr("SpecTable / Text / Markdown (*.spectable *.txt *.md);;All Files (*)"));
    if (!path.isEmpty())
        onOpenFile(path);
}

void AppController::openRecentSolution(const QString& sspecPath)
{
    loadSolution(sspecPath);
}
