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

    // When false (default), files whose FileType is "Other" (not one of the
    // types SpecStudio recognizes) are hidden from the tree.
    void setShowAllFiles(bool show);
    bool showAllFiles() const { return m_showAllFiles; }

    // Returns the Project for the given index (Project/Folder/File nodes),
    // or nullptr if the index represents the Solution or is invalid.
    Project* projectForIndex(const QModelIndex& idx) const;

    // QAbstractItemModel interface
    QModelIndex   index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex   parent(const QModelIndex& child) const override;
    int           rowCount(const QModelIndex& parent = {}) const override;
    int           columnCount(const QModelIndex& parent = {}) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    enum class NodeType { Root, Solution, Project, Folder, File };

    struct Node {
        NodeType     type     = NodeType::Root;
        Solution*    solution = nullptr;
        Project*     project  = nullptr;   // set on Project and Folder nodes
        ProjectFile* file     = nullptr;
        QString      name;                 // display name for Folder nodes
        Node*        parent   = nullptr;
        int          row      = 0;
    };

    void buildNodes();
    void clearNodes();

    Node*           nodeFromIndex(const QModelIndex& index) const;
    QModelIndex     indexForNode(Node* node) const;

    Solution*       m_solution     = nullptr;
    Node*           m_root         = nullptr;
    QList<Node*>    m_allNodes;
    bool            m_showAllFiles = false;
};
