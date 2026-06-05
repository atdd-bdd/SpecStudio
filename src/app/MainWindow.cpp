#include "MainWindow.h"
#include "AppController.h"
#include "AppSettings.h"
#include "../ui/SolutionExplorer.h"
#include "../ui/EditorTabWidget.h"
#include "../ui/OutputPanel.h"
#include "../ui/StatusBarManager.h"
#include "../ui/dialogs/FindReplaceDialog.h"
#include "../editors/BaseEditor.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("SpecStudio");
    resize(1280, 800);

    // Docks and central widget must exist before AppController touches them
    m_editorTabs = new EditorTabWidget(this);
    setCentralWidget(m_editorTabs);

    setupDocks();
    setupStatusBar();

    m_controller = new AppController(this, this);

    setupMenuBar();
    restoreWindowState();
}

MainWindow::~MainWindow() = default;

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

    auto* actCut   = editMenu->addAction(tr("Cut"),   QKeySequence::Cut);
    auto* actCopy  = editMenu->addAction(tr("Copy"),  QKeySequence::Copy);
    auto* actPaste = editMenu->addAction(tr("Paste"), QKeySequence::Paste);
    editMenu->addSeparator();
    auto* actFind    = editMenu->addAction(tr("Find..."),    QKeySequence::Find);
    auto* actReplace = editMenu->addAction(tr("Replace..."), QKeySequence(Qt::CTRL | Qt::Key_H));

    connect(actCut,   &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->cut(); });
    connect(actCopy,  &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->copy(); });
    connect(actPaste, &QAction::triggered, this, [this] { if (auto* ed = m_editorTabs->currentEditor()) ed->paste(); });
    connect(actFind,    &QAction::triggered, this, [this] {
        if (!m_findReplaceDlg) m_findReplaceDlg = new FindReplaceDialog(m_editorTabs, this);
        m_findReplaceDlg->showFind();
    });
    connect(actReplace, &QAction::triggered, this, [this] {
        if (!m_findReplaceDlg) m_findReplaceDlg = new FindReplaceDialog(m_editorTabs, this);
        m_findReplaceDlg->showReplace();
    });

    // ---- View ----
    auto* viewMenu = menuBar()->addMenu(tr("&View"));

    auto* actShowSolution = viewMenu->addAction(tr("Solution Explorer"));
    auto* actShowFiles    = viewMenu->addAction(tr("Files"));
    auto* actShowOutput   = viewMenu->addAction(tr("Output"));

    connect(actShowSolution, &QAction::triggered, m_solutionExplorer, &QDockWidget::show);
    connect(actShowFiles,    &QAction::triggered, m_editorTabs,        &QWidget::show);
    connect(actShowOutput,   &QAction::triggered, m_outputPanel,       &QDockWidget::show);

    // ---- Git ----
    auto* gitMenu = menuBar()->addMenu(tr("&Git"));

    auto* actCommitPush = gitMenu->addAction(tr("Commit and Push..."));
    auto* actFetch      = gitMenu->addAction(tr("Fetch"));

    connect(actCommitPush, &QAction::triggered, m_controller, &AppController::onCommitAndPush);
    connect(actFetch,      &QAction::triggered, m_controller, &AppController::onFetch);

    // ---- Build ----
    auto* buildMenu = menuBar()->addMenu(tr("&Build"));

    auto* actBuildFile    = buildMenu->addAction(tr("Current File"),    QKeySequence(Qt::Key_F6));
    auto* actBuildProject = buildMenu->addAction(tr("Entire Project"),  QKeySequence(Qt::SHIFT | Qt::Key_F6));

    connect(actBuildFile,    &QAction::triggered, m_controller, &AppController::onBuildCurrentFile);
    connect(actBuildProject, &QAction::triggered, m_controller, &AppController::onBuildProject);

    // ---- Analyze ----
    auto* analyzeMenu = menuBar()->addMenu(tr("&Analyze"));

    auto* actAnalyze = analyzeMenu->addAction(tr("Check Syntax"), QKeySequence(Qt::Key_F7));
    connect(actAnalyze, &QAction::triggered, m_controller, &AppController::onAnalyze);
}

void MainWindow::setupDocks()
{
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

    m_solutionExplorer = new SolutionExplorer(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_solutionExplorer);

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
