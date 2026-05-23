#pragma once

#include <QAbstractItemModel>

class Solution;
class Project;
class ProjectFile;

class SolutionTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SolutionTreeModel(QObject* parent = nullptr);

    void setSolution(Solution* solution);
    void refresh();

    // QAbstractItemModel interface
    QModelIndex   index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex   parent(const QModelIndex& child) const override;
    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    enum class NodeType { Root, Solution, Project, File };

    struct Node {
        NodeType    type   = NodeType::Root;
        Solution*   solution = nullptr;
        Project*    project  = nullptr;
        ProjectFile* file    = nullptr;
        Node*       parent   = nullptr;
        int         row      = 0;
    };

    void buildNodes();
    void clearNodes();

    Node*           nodeFromIndex(const QModelIndex& index) const;
    QModelIndex     indexForNode(Node* node) const;

    Solution*       m_solution = nullptr;
    Node*           m_root     = nullptr;
    QList<Node*>    m_allNodes;
};
