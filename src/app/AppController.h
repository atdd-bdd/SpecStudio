#pragma once

#include <QObject>

class MainWindow;
class Solution;
class Project;
class AppSettings;
class SolutionTreeModel;
class ProjectIndex;
class FeatureXAnalyzer;
class SpecTableIndex;
class SpecTableAnalyzer;
class BuildController;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(MainWindow* mainWindow, QObject* parent = nullptr);
    ~AppController() override;

    Solution* currentSolution() const { return m_solution; }

public slots:
    void onNewSolution();
    void onNewProject();
    void onNewFile(const QString& projectRootHint = {});
    void onOpenSolution();
    void onSave();
    void onSaveAs();
    void onSaveAll();
    void onPrint();
    void onSettings();
    void onCommitAndPush();
    void onFetch();
    void onPull();
    void onDiffCurrentFile();
    void onBuildCurrentFile();
    void onBuildProject();
    void onBuildSolution();
    void onSetActiveBuildConfig(const QString& configAbsPath);
    void onAnalyze();
    void onAnalyzeProject(const QString& projectRootPath);
    void onAnalyzeSolution();
    void onFindAllUsages();
    void onFindAll(const QString& term, bool caseSensitive, bool useRegex);
    void onRenameStep();
    void onRenameFile(const QString& absolutePath);
    void onMoveFile(const QString& absolutePath);
    void onDeleteFile(const QString& absolutePath);
    void onCopyFile(const QString& absolutePath);
    void onPasteFile(const QString& targetProjectRoot);
    void onRenameProject(const QString& projectRootPath);
    void onMoveProject(const QString& projectRootPath);
    void onOpenFile(const QString& absolutePath);
    void onOpenFileDialog();
    void openRecentSolution(const QString& sspecPath);
    void onRefreshSolution();
    void onToggleShowAllFiles(bool show);

    // Called from MainWindow::closeEvent, after the Save All prompt. Asks
    // whether to share pending changes before exiting; does not block exit
    // either way. No-op if there's no solution or nothing to share.
    void promptShareOnExit();

signals:
    void solutionLoaded(Solution* solution);

private:
    void loadSolution(const QString& sspecPath);
    void setSolution(Solution* solution);
    bool shareChanges();  // returns true if changes were shared successfully
    void applyFonts();
    void applyAutoReload();
    void setupBuildConnections();
    void doBuildProjects(const QList<Project*>& targets);
    void doAnalyze(const QList<Project*>& targets);
    // Resolves the .specconfig to use for a project: the active (most-recently-set)
    // configuration if one is set, the sole .specconfig if the project root has
    // exactly one, or — if there's no active choice and multiple candidates exist —
    // prompts the user to pick one and remembers it as active. Returns an absolute
    // path, or empty if no .specconfig exists for the project.
    QString resolveActiveConfig(Project* proj);
    Project* activeProject() const;  // project in Explorer selection, or current editor's project
    void navigateToLine(const QString& filePath, int line);
    void findReferencesForSymbol(const QString& symbolName);
    void findStepUsages(const QString& keyword, const QString& stepText);
    void renameSpecTableSymbol(const QString& symbolName);
    void onSymbolAtCursor(const QString& name);

    MainWindow*        m_mainWindow = nullptr;
    Solution*          m_solution   = nullptr;
    AppSettings*       m_settings   = nullptr;
    SolutionTreeModel* m_treeModel  = nullptr;
    ProjectIndex*      m_index          = nullptr;
    FeatureXAnalyzer*  m_analyzer       = nullptr;
    SpecTableIndex*    m_specTableIndex = nullptr;
    SpecTableAnalyzer* m_specAnalyzer   = nullptr;
    BuildController*   m_builder        = nullptr;
    QString            m_buildAccum;    // accumulates build output for diagnostic parsing
    QString            m_buildLogPath;  // path to write build.log; set before each build
    QString            m_copiedFilePath;
};
