#include "MainWindow.h"
#include "AppController.h"
#include "AppSettings.h"
#include "../model/Solution.h"
#include "../model/Project.h"
#include "../ui/SolutionExplorer.h"
#include "../ui/EditorTabWidget.h"
#include "../ui/OutputPanel.h"
#include "../ui/StatusBarManager.h"
#include "../ui/AttributeInspectorPanel.h"
#include "../ui/EntityTreePanel.h"
#include "../ui/dialogs/FindReplaceDialog.h"
#include "../editors/BaseEditor.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("SpecStudio");
    resize(1280, 800);

    // Docks and central widget must exist before AppController touches them
    m_splitter   = new QSplitter(Qt::Horizontal, this);
    m_editorTabs = new EditorTabWidget(m_splitter);
    m_splitter->addWidget(m_editorTabs);
    setCentralWidget(m_splitter);

    setupDocks();
    setupStatusBar();

    m_controller = new AppController(this, this);

    setupMenuBar();
    restoreWindowState();
}

MainWindow::~MainWindow() = default;

BaseEditor* MainWindow::currentEditor() const
{
    return activeEditorTabs()->currentEditor();
}

EditorTabWidget* MainWindow::activeEditorTabs() const
{
    if (m_editorTabs2 && m_editorTabs2->isVisible() &&
        m_editorTabs2->hasFocus())
        return m_editorTabs2;
    return m_editorTabs;
}

BaseEditor* MainWindow::editorForPath(const QString& path) const
{
    if (auto* ed = m_editorTabs->editorForPath(path)) return ed;
    if (m_editorTabs2) return m_editorTabs2->editorForPath(path);
    return nullptr;
}

QList<BaseEditor*> MainWindow::allOpenEditors() const
{
    QList<BaseEditor*> result;
    auto collect = [&](EditorTabWidget* tabs) {
        if (!tabs) return;
        for (int i = 0; i < tabs->count(); ++i)
            if (auto* ed = qobject_cast<BaseEditor*>(tabs->widget(i)))
                result.append(ed);
    };
    collect(m_editorTabs);
    collect(m_editorTabs2);
    return result;
}

void MainWindow::splitEditorRight()
{
    if (!m_editorTabs2) {
        m_editorTabs2 = new EditorTabWidget(m_splitter);
        m_splitter->addWidget(m_editorTabs2);
        // Equal split
        const int half = m_splitter->width() / 2;
        m_splitter->setSizes({half, half});
        // Wire so files can be opened via go-to-definition etc.
        connect(m_editorTabs2, &EditorTabWidget::fileOpenRequested,
                m_controller, &AppController::onOpenFile);
    }
    m_editorTabs2->show();

    // Open the active file in the second pane too, if there is one
    if (auto* ed = m_editorTabs->currentEditor())
        m_editorTabs2->openFile(ed->filePath());
}

void MainWindow::closeSplit()
{
    if (m_editorTabs2)
        m_editorTabs2->hide();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    event->accept();
}

void MainWindow::setupMenuBar()
{
    // ---- File ----
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* actNewSolution  = fileMenu->addAction(tr("New Solution..."));
    auto* actNewProject   = fileMenu->addAction(tr("New Project..."));
    auto* actNewFile      = fileMenu->addAction(tr("New File..."),     QKeySequence::New);
    auto* actOpenSolution = fileMenu->addAction(tr("Open Solution/Project..."));
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(tr("Recent Solutions"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, &MainWindow::populateRecentMenu);
    fileMenu->addSeparator();
    auto* actSave         = fileMenu->addAction(tr("Save"),     QKeySequence::Save);
    auto* actSaveAll      = fileMenu->addAction(tr("Save All"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    fileMenu->addSeparator();
    auto* actPrint        = fileMenu->addAction(tr("Print..."), QKeySequence::Print);
    fileMenu->addSeparator();
    auto* actSettings     = fileMenu->addAction(tr("Settings..."));

    connect(actNewSolution,  &QAction::triggered, m_controller, &AppController::onNewSolution);
    connect(actNewProject,   &QAction::triggered, m_controller, &AppController::onNewProject);
    connect(actNewFile,      &QAction::triggered, m_controller, [this]{ m_controller->onNewFile(); });
    connect(actOpenSolution, &QAction::triggered, m_controller, &AppController::onOpenSolution);
    connect(actSave,         &QAction::triggered, m_controller, &AppController::onSave);
    connect(actSaveAll,      &QAction::triggered, m_controller, &AppController::onSaveAll);
    connect(actPrint,        &QAction::triggered, m_controller, &AppController::onPrint);
    connect(actSettings,     &QAction::triggered, m_controller, &AppController::onSettings);

    // ---- Edit ----
    auto* editMenu = menuBar()->addMenu(tr("&Edit"));

    auto* actUndo      = editMenu->addAction(tr("Undo"),       QKeySequence::Undo);
    auto* actRedo      = editMenu->addAction(tr("Redo"),       QKeySequence::Redo);
    editMenu->addSeparator();
    auto* actCut       = editMenu->addAction(tr("Cut"),        QKeySequence::Cut);
    auto* actCopy      = editMenu->addAction(tr("Copy"),       QKeySequence::Copy);
    auto* actPaste     = editMenu->addAction(tr("Paste"),      QKeySequence::Paste);
    auto* actSelectAll = editMenu->addAction(tr("Select All"), QKeySequence::SelectAll);
    editMenu->addSeparator();
    auto* actGoToLine = editMenu->addAction(tr("Go to Line..."), QKeySequence(Qt::CTRL | Qt::Key_G));
    auto* actFind     = editMenu->addAction(tr("Find..."),       QKeySequence::Find);
    auto* actReplace  = editMenu->addAction(tr("Replace..."),    QKeySequence(Qt::CTRL | Qt::Key_H));

    connect(actUndo,      &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->undo(); });
    connect(actRedo,      &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->redo(); });
    connect(actCut,       &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->cut(); });
    connect(actCopy,      &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->copy(); });
    connect(actPaste,     &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->paste(); });
    connect(actSelectAll, &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->selectAll(); });
    editMenu->addSeparator();
    auto* actFindUsages = editMenu->addAction(tr("Find All Usages..."), QKeySequence(Qt::SHIFT | Qt::Key_F12));
    auto* actRename     = editMenu->addAction(tr("Rename Step..."),     QKeySequence(Qt::Key_F2));

    connect(actGoToLine, &QAction::triggered, this, [this] {
        auto* ed = m_editorTabs->currentEditor();
        if (!ed) return;
        bool ok;
        int line = QInputDialog::getInt(this, tr("Go to Line"), tr("Line number:"),
                                        1, 1, 999999, 1, &ok);
        if (ok) ed->goToLine(line);
    });
    connect(actFind,    &QAction::triggered, this, [this] {
        if (!m_findReplaceDlg) m_findReplaceDlg = new FindReplaceDialog(m_editorTabs, this);
        m_findReplaceDlg->showFind();
    });
    connect(actReplace, &QAction::triggered, this, [this] {
        if (!m_findReplaceDlg) m_findReplaceDlg = new FindReplaceDialog(m_editorTabs, this);
        m_findReplaceDlg->showReplace();
    });
    connect(actFindUsages, &QAction::triggered, m_controller, &AppController::onFindAllUsages);
    connect(actRename,     &QAction::triggered, m_controller, &AppController::onRenameStep);
    editMenu->addSeparator();
    auto* actFormatTable = editMenu->addAction(tr("Format Table"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
    connect(actFormatTable, &QAction::triggered, this, [this] { if (auto* ed = currentEditor()) ed->formatTable(); });
    auto* actEditTable  = editMenu->addAction(tr("Edit Table..."),  QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    auto* actEditString = editMenu->addAction(tr("Edit String..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q));
    connect(actEditTable,  &QAction::triggered, this, [this] { if (auto* ed = currentEditor()) ed->editTable(); });
    connect(actEditString, &QAction::triggered, this, [this] { if (auto* ed = currentEditor()) ed->editString(); });

    // ---- View ----
    auto* viewMenu = menuBar()->addMenu(tr("&View"));

    auto* actShowSolution  = viewMenu->addAction(tr("Solution Explorer"));
    auto* actShowSymTree   = viewMenu->addAction(tr("Symbol Tree"));
    auto* actShowAttrInsp  = viewMenu->addAction(tr("Attribute Inspector"));
    auto* actShowFiles     = viewMenu->addAction(tr("Files"));
    auto* actShowOutput    = viewMenu->addAction(tr("Output"));
    viewMenu->addSeparator();
    auto* actRefresh     = viewMenu->addAction(tr("Refresh"),              QKeySequence(Qt::Key_F5));
    viewMenu->addSeparator();
    auto* actSplitRight  = viewMenu->addAction(tr("Split Editor Right"), QKeySequence(Qt::CTRL | Qt::Key_Backslash));
    auto* actCloseSplit  = viewMenu->addAction(tr("Close Split"),         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backslash));

    connect(actShowSolution,  &QAction::triggered, m_solutionExplorer, &QDockWidget::show);
    connect(actShowSymTree,   &QAction::triggered, m_entityTree,        &QDockWidget::show);
    connect(actShowAttrInsp,  &QAction::triggered, m_attrInspector,     &QDockWidget::show);
    connect(actShowFiles,     &QAction::triggered, m_editorTabs,        &QWidget::show);
    connect(actShowOutput,    &QAction::triggered, m_outputPanel,       &QDockWidget::show);
    connect(actRefresh,       &QAction::triggered, m_controller, &AppController::onRefreshSolution);
    connect(actSplitRight,    &QAction::triggered, this, &MainWindow::splitEditorRight);
    connect(actCloseSplit,    &QAction::triggered, this, &MainWindow::closeSplit);

    // ---- Git ----
    auto* gitMenu = menuBar()->addMenu(tr("&Git"));

    auto* actCommitPush = gitMenu->addAction(tr("Commit and Push..."));
    auto* actFetch      = gitMenu->addAction(tr("Fetch"));
    auto* actPull       = gitMenu->addAction(tr("Pull"));
    gitMenu->addSeparator();
    auto* actDiff = gitMenu->addAction(tr("Diff Current File"), QKeySequence(Qt::CTRL | Qt::Key_D));

    connect(actCommitPush, &QAction::triggered, m_controller, &AppController::onCommitAndPush);
    connect(actFetch,      &QAction::triggered, m_controller, &AppController::onFetch);
    connect(actPull,       &QAction::triggered, m_controller, &AppController::onPull);
    connect(actDiff,       &QAction::triggered, m_controller, &AppController::onDiffCurrentFile);

    // ---- Build ----
    auto* buildMenu = menuBar()->addMenu(tr("&Build"));

    auto* actBuildFile     = buildMenu->addAction(tr("Current File"),  QKeySequence(Qt::Key_F6));
    auto* actBuildProject  = buildMenu->addAction(tr("Project"),       QKeySequence(Qt::SHIFT | Qt::Key_F6));
    auto* actBuildSolution = buildMenu->addAction(tr("Solution"),      QKeySequence(Qt::CTRL | Qt::Key_F6));
    buildMenu->addSeparator();
    m_configMenu = buildMenu->addMenu(tr("Configuration"));
    connect(m_configMenu, &QMenu::aboutToShow, this, &MainWindow::populateConfigMenu);

    connect(actBuildFile,     &QAction::triggered, m_controller, &AppController::onBuildCurrentFile);
    connect(actBuildProject,  &QAction::triggered, m_controller, &AppController::onBuildProject);
    connect(actBuildSolution, &QAction::triggered, m_controller, &AppController::onBuildSolution);

    // ---- Analyze ----
    auto* analyzeMenu = menuBar()->addMenu(tr("&Analyze"));

    auto* actAnalyzeProject  = analyzeMenu->addAction(tr("Project"),  QKeySequence(Qt::Key_F7));
    auto* actAnalyzeSolution = analyzeMenu->addAction(tr("Solution"), QKeySequence(Qt::SHIFT | Qt::Key_F7));

    connect(actAnalyzeProject,  &QAction::triggered, m_controller, &AppController::onAnalyze);
    connect(actAnalyzeSolution, &QAction::triggered, m_controller, &AppController::onAnalyzeSolution);
}

void MainWindow::setupDocks()
{
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

    m_solutionExplorer = new SolutionExplorer(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_solutionExplorer);

    m_entityTree = new EntityTreePanel(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_entityTree);
    tabifyDockWidget(m_solutionExplorer, m_entityTree);
    m_solutionExplorer->raise();

    m_attrInspector = new AttributeInspectorPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, m_attrInspector);
    m_attrInspector->hide();

    m_outputPanel = new OutputPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_outputPanel);
}

void MainWindow::setupStatusBar()
{
    m_statusBarMgr = new StatusBarManager(statusBar(), this);
}

void MainWindow::saveWindowState()
{
    QSettings settings;
    settings.setValue("Window/geometry", saveGeometry());
    settings.setValue("Window/state",    saveState());
}

void MainWindow::restoreWindowState()
{
    QSettings settings;
    if (settings.contains("Window/geometry"))
        restoreGeometry(settings.value("Window/geometry").toByteArray());
    if (settings.contains("Window/state"))
        restoreState(settings.value("Window/state").toByteArray());
}

void MainWindow::populateConfigMenu()
{
    m_configMenu->clear();

    Solution* sol = m_controller->currentSolution();
    if (!sol) {
        auto* none = m_configMenu->addAction(tr("(no solution open)"));
        none->setEnabled(false);
        return;
    }

    // Collect all projects that might be relevant
    QList<Project*> projs = sol->projects();
    if (projs.isEmpty()) {
        auto* none = m_configMenu->addAction(tr("(no projects)"));
        none->setEnabled(false);
        return;
    }

    // Show configs for the first project (or the active one if we can determine it)
    // Use a simple heuristic: list configs from all projects, grouped
    AppSettings settings;
    bool anyFound = false;

    for (Project* p : projs) {
        const QDir projDir(p->rootPath());
        const QStringList cfgFiles = projDir.entryList({ "*.specconfig" }, QDir::Files, QDir::Name);
        if (cfgFiles.isEmpty()) continue;
        anyFound = true;

        if (projs.size() > 1) {
            auto* label = m_configMenu->addAction(QStringLiteral("[%1]").arg(p->name()));
            label->setEnabled(false);
        }

        const QString active = settings.activeBuildConfig(p->rootPath());

        for (const QString& fname : cfgFiles) {
            const QString absPath = projDir.absoluteFilePath(fname);
            auto* act = m_configMenu->addAction(fname);
            act->setCheckable(true);
            act->setChecked(absPath == active || (active.isEmpty() && fname == cfgFiles.first() && cfgFiles.size() == 1));
            connect(act, &QAction::triggered, m_controller,
                    [this, absPath] { m_controller->onSetActiveBuildConfig(absPath); });
        }
        if (projs.size() > 1) m_configMenu->addSeparator();
    }

    if (!anyFound) {
        auto* none = m_configMenu->addAction(tr("(no .specconfig files found)"));
        none->setEnabled(false);
    }
}

void MainWindow::populateRecentMenu()
{
    m_recentMenu->clear();

    AppSettings tmp;
    const QStringList recents = tmp.recentSolutions();

    if (recents.isEmpty()) {
        auto* none = m_recentMenu->addAction(tr("(no recent solutions)"));
        none->setEnabled(false);
        return;
    }

    for (const QString& path : recents) {
        const QString label = QFileInfo(path).fileName() + "  [" + QFileInfo(path).absolutePath() + "]";
        auto* act = m_recentMenu->addAction(label);
        connect(act, &QAction::triggered, this, [this, path] {
            m_controller->openRecentSolution(path);
        });
    }
}
