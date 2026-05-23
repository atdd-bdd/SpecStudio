#include "SolutionExplorer.h"

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

    setWidget(m_tree);
}

void SolutionExplorer::setModel(QAbstractItemModel* model)
{
    m_tree->setModel(model);
}
