#pragma once

#include <QDockWidget>

class QTreeView;
class QAbstractItemModel;
class Project;
class SolutionTreeModel;

class SolutionExplorer : public QDockWidget
{
    Q_OBJECT

public:
    explicit SolutionExplorer(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model);
    QTreeView* treeView() const { return m_tree; }

    // Returns the Project that owns the currently selected node,
    // or nullptr if the Solution root (or nothing) is selected.
    Project* selectedProject(SolutionTreeModel* model) const;

signals:
    void fileDoubleClicked(const QString& absolutePath);
    void newFileRequested(const QString& projectRootPath);
    void fileRenameRequested(const QString& absolutePath);
    void fileDeleteRequested(const QString& absolutePath);
    void fileMoveRequested(const QString& absolutePath);
    void fileCopyRequested(const QString& absolutePath);
    void filePasteRequested(const QString& targetProjectRoot);
    void projectRenameRequested(const QString& projectRootPath);
    void projectMoveRequested(const QString& projectRootPath);

private:
    QTreeView* m_tree = nullptr;
};
