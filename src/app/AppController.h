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
    void openRecentSolution(const QString& sspecPath);
    void onRefreshSolution();

signals:
    void solutionLoaded(Solution* solution);

private:
    void loadSolution(const QString& sspecPath);
    void setSolution(Solution* solution);
    void applyFonts();
    void applyAutoReload();
    void setupBuildConnections();
    void doBuildProjects(const QList<Project*>& targets);
    void doAnalyze(const QList<Project*>& targets);
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
