#pragma once

#include <QObject>

class MainWindow;
class Solution;
class AppSettings;
class SolutionTreeModel;
class ProjectIndex;
class FeatureXAnalyzer;
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
    void onBuildCurrentFile();
    void onBuildProject();
    void onAnalyze();
    void onOpenFile(const QString& absolutePath);
    void openRecentSolution(const QString& sspecPath);

signals:
    void solutionLoaded(Solution* solution);

private:
    void loadSolution(const QString& sspecPath);
    void setSolution(Solution* solution);

    MainWindow*        m_mainWindow = nullptr;
    Solution*          m_solution   = nullptr;
    AppSettings*       m_settings   = nullptr;
    SolutionTreeModel* m_treeModel  = nullptr;
    ProjectIndex*      m_index      = nullptr;
    FeatureXAnalyzer*  m_analyzer   = nullptr;
    BuildController*   m_builder    = nullptr;
};
