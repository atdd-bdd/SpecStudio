#include "SolutionTreeModel.h"
#include "../model/Solution.h"
#include "../model/Project.h"
#include "../model/ProjectFile.h"
#include "../model/FileType.h"

#include <QFileInfo>
#include <QMap>
#include <algorithm>
#include <functional>

namespace {

// Order files take in the Solution Explorer, within one folder: the
// specifications first, then the notes about them, then whatever else the
// project holds, and the configuration last — it is set once and rarely read.
int listingRank(FileType type)
{
    switch (type) {
        case FileType::SpecTable:  return 0;
        case FileType::Markdown:   return 1;
        case FileType::SpecConfig: return 3;
        default:                   return 2;
    }
}

} // namespace

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

void SolutionTreeModel::setShowAllFiles(bool show)
{
    if (m_showAllFiles == show) return;
    m_showAllFiles = show;
    refresh();
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

        // Map from relative folder path → folder node; tracks next child row per parent.
        QMap<QString, Node*> folderMap;
        QMap<Node*, int>     nextChildRow;

        // Returns the node that should parent a file/folder at relFolderPath.
        // "." or "" means the project node itself.
        std::function<Node*(const QString&)> getOrCreateFolder =
            [&](const QString& relPath) -> Node* {
            if (relPath.isEmpty() || relPath == ".")
                return projNode;
            if (folderMap.contains(relPath))
                return folderMap[relPath];

            // Ensure the parent folder exists first
            QString parentPath = QFileInfo(relPath).path();
            Node* parentNode = getOrCreateFolder(parentPath);

            auto* folderNode = new Node();
            folderNode->type    = NodeType::Folder;
            folderNode->project = proj;
            folderNode->name    = QFileInfo(relPath).fileName();
            folderNode->parent  = parentNode;
            folderNode->row     = nextChildRow[parentNode]++;
            m_allNodes.append(folderNode);
            folderMap.insert(relPath, folderNode);
            return folderNode;
        };

        // The files to show, and the folders that hold them. Project::scanFiles
        // hands them over in QDirIterator order, which is neither grouped nor
        // stable across machines.
        QList<ProjectFile*> ordered;
        QStringList dirs;
        for (auto* file : proj->files()) {
            if (!m_showAllFiles && file->type() == FileType::Other) continue;
            ordered.append(file);
            const QString dir = QFileInfo(file->relativePath()).path();
            if (!dir.isEmpty() && dir != "." && !dirs.contains(dir))
                dirs.append(dir);
        }

        // Create every folder before any file, so folders head each level the
        // way Visual Studio's Solution Explorer shows them — and so the
        // configuration files really do come last, not merely last among the
        // files that happen to sit beside them. Sorted, so a parent folder is
        // created (and numbered) before the folders inside it.
        dirs.sort();
        for (const QString& dir : dirs)
            getOrCreateFolder(dir);

        std::sort(ordered.begin(), ordered.end(),
                  [](const ProjectFile* a, const ProjectFile* b) {
            // Group by folder first: a file belongs beside its siblings, not
            // beside every other .spectable in the project.
            const QString dirA = QFileInfo(a->relativePath()).path();
            const QString dirB = QFileInfo(b->relativePath()).path();
            if (dirA != dirB) return dirA < dirB;

            const int rankA = listingRank(a->type());
            const int rankB = listingRank(b->type());
            if (rankA != rankB) return rankA < rankB;

            return a->relativePath().compare(b->relativePath(),
                                             Qt::CaseInsensitive) < 0;
        });

        for (auto* file : ordered) {
            QString dir = QFileInfo(file->relativePath()).path();
            Node* parentNode = getOrCreateFolder(dir);

            auto* fileNode = new Node();
            fileNode->type   = NodeType::File;
            fileNode->file   = file;
            fileNode->parent = parentNode;
            fileNode->row    = nextChildRow[parentNode]++;
            m_allNodes.append(fileNode);
        }
    }
}

Project* SolutionTreeModel::projectForIndex(const QModelIndex& idx) const
{
    Node* node = nodeFromIndex(idx);
    if (!node) return nullptr;
    // Solution and Root nodes have no project
    if (node->type == NodeType::Solution || node->type == NodeType::Root)
        return nullptr;
    return node->project;
}

SolutionTreeModel::Node* SolutionTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid()) return m_root;
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex SolutionTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    Node* parentNode = nodeFromIndex(parent);

    // Collect children of parentNode in m_allNodes insertion order
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
        case NodeType::Folder:   return node->name;
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
        if (node->type == NodeType::Project || node->type == NodeType::Folder)
            return node->project->rootPath();
        if (node->type == NodeType::File)
            return node->file->absolutePath().left(
                node->file->absolutePath().length() -
                node->file->relativePath().length() - 1);
        return {};

    case Qt::UserRole + 2:
        // Node type string — lets context menus distinguish Project from Folder/File
        switch (node->type) {
        case NodeType::Project: return QStringLiteral("project");
        case NodeType::Folder:  return QStringLiteral("folder");
        case NodeType::File:    return QStringLiteral("file");
        default:                return {};
        }

    default:
        return {};
    }
}
