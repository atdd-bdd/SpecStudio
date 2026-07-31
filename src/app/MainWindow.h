#pragma once

#include <QList>
#include <QMainWindow>

class AppController;
class AttributeInspectorPanel;
class BaseEditor;
class EntityTreePanel;
class FindReplaceDialog;
class HelpDialog;
class Project;
class QAction;
class QMenu;
class QSplitter;
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

    SolutionExplorer*       solutionExplorer()      const { return m_solutionExplorer; }
    EditorTabWidget*        editorTabs()            const { return m_editorTabs; }
    OutputPanel*            outputPanel()           const { return m_outputPanel; }
    StatusBarManager*       statusBarMgr()          const { return m_statusBarMgr; }
    AttributeInspectorPanel* attributeInspector()  const { return m_attrInspector; }
    EntityTreePanel*         entityTree()           const { return m_entityTree; }

    BaseEditor*         currentEditor()     const;
    EditorTabWidget*    activeEditorTabs()  const;
    BaseEditor*         editorForPath(const QString& path) const;
    QList<BaseEditor*>  allOpenEditors()    const;

    void splitEditorRight();
    void closeSplit();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupMenuBar();
    void setupDocks();
    void setupStatusBar();

    void saveWindowState();
    void restoreWindowState();

    void populateRecentMenu();
    void populateConfigMenu();
    void populateAnalyzeMenu();
    void showHelp();
    void showAbout();
    void populateGitMenu();

    void stubAction(const QString& name);

    AppController*     m_controller      = nullptr;
    QMenu*             m_recentMenu         = nullptr;
    QMenu*             m_configMenu         = nullptr;
    QMenu*             m_analyzeProjectMenu = nullptr;
    QMenu*             m_gitMenu            = nullptr;
    // Carries a global Ctrl+D shortcut regardless of which Git-menu variant is
    // showing (Shared-Files solutions hide it from the menu, but the shortcut
    // must keep working) — created once, only conditionally re-inserted into
    // the menu by populateGitMenu().
    QAction*           m_actDiffCurrentFile = nullptr;
    // Held so Help reopens the same window instead of stacking copies.
    HelpDialog*        m_helpDialog = nullptr;
    FindReplaceDialog* m_findReplaceDlg  = nullptr;
    SolutionExplorer*  m_solutionExplorer = nullptr;
    QSplitter*         m_splitter        = nullptr;
    EditorTabWidget*   m_editorTabs      = nullptr;
    EditorTabWidget*   m_editorTabs2     = nullptr;
    OutputPanel*            m_outputPanel     = nullptr;
    StatusBarManager*       m_statusBarMgr    = nullptr;
    AttributeInspectorPanel* m_attrInspector  = nullptr;
    EntityTreePanel*         m_entityTree      = nullptr;
};
