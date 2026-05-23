#pragma once

#include <QMainWindow>

class AppController;
class SolutionExplorer;
class EditorTabWidget;
class OutputPanel;
class StatusBarManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    SolutionExplorer* solutionExplorer() const { return m_solutionExplorer; }
    EditorTabWidget*  editorTabs()        const { return m_editorTabs; }
    OutputPanel*      outputPanel()       const { return m_outputPanel; }
    StatusBarManager* statusBarMgr()      const { return m_statusBarMgr; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupMenuBar();
    void setupDocks();
    void setupStatusBar();

    void saveWindowState();
    void restoreWindowState();

    void stubAction(const QString& name);

    AppController*    m_controller       = nullptr;
    SolutionExplorer* m_solutionExplorer = nullptr;
    EditorTabWidget*  m_editorTabs       = nullptr;
    OutputPanel*      m_outputPanel      = nullptr;
    StatusBarManager* m_statusBarMgr     = nullptr;
};
