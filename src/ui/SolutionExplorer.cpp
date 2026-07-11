#include "SolutionExplorer.h"
#include "SolutionTreeModel.h"
#include "../model/Project.h"

#include <QAction>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

SolutionExplorer::SolutionExplorer(QWidget* parent)
    : QDockWidget(tr("Solution Explorer"), parent)
{
    setObjectName("SolutionExplorer");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_tree = new QTreeView(this);
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        QString path = index.data(Qt::UserRole).toString();
        if (!path.isEmpty())
            emit fileDoubleClicked(path);
    });

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QModelIndex index = m_tree->indexAt(pos);
        const QString filePath    = index.data(Qt::UserRole).toString();
        const QString projectRoot = index.data(Qt::UserRole + 1).toString();
        const QString nodeType    = index.data(Qt::UserRole + 2).toString();
        const bool isFile    = (nodeType == "file");
        const bool isProject = (nodeType == "project");

        QMenu menu(this);
        auto* actNewFile = menu.addAction(tr("New File..."));
        connect(actNewFile, &QAction::triggered, this, [this, projectRoot] {
            emit newFileRequested(projectRoot);
        });

        if (isProject) {
            menu.addSeparator();
            auto* actRename = menu.addAction(tr("Rename Project..."));
            auto* actMove   = menu.addAction(tr("Move Project..."));
            connect(actRename, &QAction::triggered, this, [this, projectRoot] {
                emit projectRenameRequested(projectRoot);
            });
            connect(actMove, &QAction::triggered, this, [this, projectRoot] {
                emit projectMoveRequested(projectRoot);
            });
            menu.addSeparator();
            auto* actPaste = menu.addAction(tr("Paste..."));
            connect(actPaste, &QAction::triggered, this, [this, projectRoot] {
                emit filePasteRequested(projectRoot);
            });
        }

        if (isFile) {
            menu.addSeparator();
            auto* actCopy   = menu.addAction(tr("Copy"));
            auto* actPaste  = menu.addAction(tr("Paste..."));
            menu.addSeparator();
            auto* actRename = menu.addAction(tr("Rename..."));
            auto* actMove   = menu.addAction(tr("Move..."));
            auto* actDelete = menu.addAction(tr("Delete"));
            connect(actCopy, &QAction::triggered, this, [this, filePath] {
                emit fileCopyRequested(filePath);
            });
            connect(actPaste, &QAction::triggered, this, [this, projectRoot] {
                emit filePasteRequested(projectRoot);
            });
            connect(actRename, &QAction::triggered, this, [this, filePath] {
                emit fileRenameRequested(filePath);
            });
            connect(actMove, &QAction::triggered, this, [this, filePath] {
                emit fileMoveRequested(filePath);
            });
            connect(actDelete, &QAction::triggered, this, [this, filePath] {
                emit fileDeleteRequested(filePath);
            });
        }

        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    });

    setWidget(m_tree);
}

void SolutionExplorer::setModel(QAbstractItemModel* model)
{
    m_tree->setModel(model);
}

Project* SolutionExplorer::selectedProject(SolutionTreeModel* model) const
{
    if (!model) return nullptr;
    const QModelIndex idx = m_tree->currentIndex();
    return model->projectForIndex(idx);
}
