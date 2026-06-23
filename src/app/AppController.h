#pragma once

#include <QObject>

class MainWindow;
class Solution;
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
    void onSaveAll();
    void onPrint();
    void onSettings();
    void onCommitAndPush();
    void onFetch();
    void onPull();
    void onDiffCurrentFile();
    void onBuildCurrentFile();
    void onBuildProject();
    void onAnalyze();
    void onFindAllUsages();
    void onRenameStep();
    void onRenameFile(const QString& absolutePath);
    void onDeleteFile(const QString& absolutePath);
    void onOpenFile(const QString& absolutePath);
    void openRecentSolution(const QString& sspecPath);

signals:
    void solutionLoaded(Solution* solution);

private:
    void loadSolution(const QString& sspecPath);
    void setSolution(Solution* solution);
    void applyFonts();
    void applyAutoReload();
    void setupBuildConnections();
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
};
