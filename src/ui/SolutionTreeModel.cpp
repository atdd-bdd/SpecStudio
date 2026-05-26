#include "SolutionTreeModel.h"
#include "../model/Solution.h"
#include "../model/Project.h"
#include "../model/ProjectFile.h"

SolutionTreeModel::SolutionTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    m_root = new Node();
    m_allNodes.append(m_root);
}

void SolutionTreeModel::setSolution(Solution* solution)
{
    beginResetModel();
    m_solution = solution;
    buildNodes();
    endResetModel();
}

void SolutionTreeModel::refresh()
{
    beginResetModel();
    buildNodes();
    endResetModel();
}

void SolutionTreeModel::clearNodes()
{
    qDeleteAll(m_allNodes);
    m_allNodes.clear();
    m_root = new Node();
    m_allNodes.append(m_root);
}

void SolutionTreeModel::buildNodes()
{
    clearNodes();
    if (!m_solution) return;

    auto* solNode = new Node();
    solNode->type     = NodeType::Solution;
    solNode->solution = m_solution;
    solNode->parent   = m_root;
    solNode->row      = 0;
    m_allNodes.append(solNode);

    int projRow = 0;
    for (auto* proj : m_solution->projects()) {
        auto* projNode = new Node();
        projNode->type    = NodeType::Project;
        projNode->project = proj;
        projNode->parent  = solNode;
        projNode->row     = projRow++;
        m_allNodes.append(projNode);

        int fileRow = 0;
        for (auto* file : proj->files()) {
            auto* fileNode = new Node();
            fileNode->type   = NodeType::File;
            fileNode->file   = file;
            fileNode->parent = projNode;
            fileNode->row    = fileRow++;
            m_allNodes.append(fileNode);
        }
    }
}

SolutionTreeModel::Node* SolutionTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid()) return m_root;
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex SolutionTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    Node* parentNode = nodeFromIndex(parent);

    // Collect children of parentNode
    QList<Node*> children;
    for (auto* n : m_allNodes)
        if (n->parent == parentNode)
            children.append(n);

    if (row < 0 || row >= children.size()) return {};
    return createIndex(row, column, children[row]);
}

QModelIndex SolutionTreeModel::parent(const QModelIndex& child) const
{
    Node* node = nodeFromIndex(child);
    if (!node || node->parent == m_root || node->parent == nullptr)
        return {};
    return createIndex(node->parent->row, 0, node->parent);
}

int SolutionTreeModel::rowCount(const QModelIndex& parent) const
{
    Node* parentNode = nodeFromIndex(parent);
    int count = 0;
    for (auto* n : m_allNodes)
        if (n->parent == parentNode) ++count;
    return count;
}

int SolutionTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant SolutionTreeModel::data(const QModelIndex& index, int role) const
{
    Node* node = nodeFromIndex(index);
    if (!node) return {};

    switch (role) {
    case Qt::DisplayRole:
        switch (node->type) {
        case NodeType::Solution: return node->solution->name();
        case NodeType::Project:  return node->project->name();
        case NodeType::File:     return node->file->fileName();
        default: return {};
        }

    case Qt::UserRole:
        // Absolute file path — used by double-click to open the file
        if (node->type == NodeType::File)
            return node->file->absolutePath();
        return {};

    case Qt::UserRole + 1:
        // Project root path — used by context menu to scope "New File" to a project
        if (node->type == NodeType::Project)
            return node->project->rootPath();
        if (node->type == NodeType::File)
            return node->file->absolutePath().left(
                node->file->absolutePath().length() -
                node->file->relativePath().length() - 1);
        return {};

    default:
        return {};
    }
}
