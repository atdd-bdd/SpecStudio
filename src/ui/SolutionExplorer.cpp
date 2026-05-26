#include "SolutionExplorer.h"

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
        // UserRole+1 carries the project root path (set for project and file nodes)
        QString projectRoot = index.data(Qt::UserRole + 1).toString();

        QMenu menu(this);
        auto* actNewFile = menu.addAction(tr("New File..."));
        connect(actNewFile, &QAction::triggered, this, [this, projectRoot] {
            emit newFileRequested(projectRoot);
        });
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    });

    setWidget(m_tree);
}

void SolutionExplorer::setModel(QAbstractItemModel* model)
{
    m_tree->setModel(model);
}
