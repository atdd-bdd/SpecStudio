#include "EntityTreePanel.h"
#include "../analyzer/SpecTableIndex.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QMap>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

static void populateGroup(QTreeWidget* tree,
                          const QString& groupName,
                          const QMap<QString, SymbolLocation>& symbols)
{
    if (symbols.isEmpty()) return;

    auto* group = new QTreeWidgetItem(tree, {groupName});
    group->setFlags(Qt::ItemIsEnabled);
    QFont f = group->font(0);
    f.setBold(true);
    group->setFont(0, f);

    for (auto it = symbols.cbegin(); it != symbols.cend(); ++it) {
        auto* item = new QTreeWidgetItem(group);
        item->setText(0, it.key());
        item->setText(1, QFileInfo(it.value().filePath).fileName());
        item->setData(0, Qt::UserRole, it.value().filePath);
        item->setData(0, Qt::UserRole + 1, it.value().line);
        item->setToolTip(0, it.value().filePath + QStringLiteral(" : ") + QString::number(it.value().line));
    }
    group->setExpanded(true);
}

EntityTreePanel::EntityTreePanel(QWidget* parent)
    : QDockWidget(tr("Symbol Tree"), parent)
{
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Symbol"), tr("File")});
    m_tree->setColumnWidth(0, 180);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setAlternatingRowColors(true);

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
        const QString fp  = item->data(0, Qt::UserRole).toString();
        const int     ln  = item->data(0, Qt::UserRole + 1).toInt();
        if (!fp.isEmpty()) emit navigateToRequested(fp, ln);
    });

    setWidget(m_tree);
    setFeatures(DockWidgetMovable | DockWidgetFloatable | DockWidgetClosable);
}

void EntityTreePanel::refresh(SpecTableIndex* index)
{
    m_tree->clear();
    if (!index) return;

    const SpecTableSymbols& sym = index->projectSymbols();
    populateGroup(m_tree, tr("Entities"),       sym.entities);
    populateGroup(m_tree, tr("Attributes"),      sym.attributes);
    populateGroup(m_tree, tr("DataTypes"),       sym.dataTypes);
    populateGroup(m_tree, tr("BusinessRules"),   sym.businessRules);
    populateGroup(m_tree, tr("Calculations"),    sym.calculations);
    populateGroup(m_tree, tr("Scenarios"),       sym.scenarios);
    populateGroup(m_tree, tr("Specifications"),  sym.specifications);
    populateGroup(m_tree, tr("Defines"),         sym.defines);
    populateGroup(m_tree, tr("DomainTerms"),     sym.domainTerms);
}

void EntityTreePanel::clear()
{
    m_tree->clear();
}
