#pragma once

#include <QDockWidget>

class QTreeView;
class QAbstractItemModel;

class SolutionExplorer : public QDockWidget
{
    Q_OBJECT

public:
    explicit SolutionExplorer(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model);
    QTreeView* treeView() const { return m_tree; }

signals:
    void fileDoubleClicked(const QString& absolutePath);
    void newFileRequested(const QString& projectRootPath);
    void fileRenameRequested(const QString& absolutePath);
    void fileDeleteRequested(const QString& absolutePath);

private:
    QTreeView* m_tree = nullptr;
};
