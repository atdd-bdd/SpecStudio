#include "AttributeInspectorPanel.h"
#include "../analyzer/SpecTableIndex.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

AttributeInspectorPanel::AttributeInspectorPanel(QWidget* parent)
    : QDockWidget(tr("Attribute Inspector"), parent)
{
    auto* container = new QWidget(this);
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_nameLabel = new QLabel(tr("(no symbol selected)"), container);
    m_nameLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_nameLabel);

    m_typeLabel = new QLabel(container);
    m_typeLabel->setStyleSheet("color: gray;");
    layout->addWidget(m_typeLabel);

    m_table = new QTableWidget(container);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    setWidget(container);
    setFeatures(DockWidgetMovable | DockWidgetFloatable | DockWidgetClosable);
}

void AttributeInspectorPanel::showSymbol(const QString& name, SpecTableIndex* index)
{
    if (!index) { clear(); return; }

    const SpecTableSymbols& proj = index->projectSymbols();
    const SymbolLocation loc = proj.locationFor(name);

    QString typeName;
    if      (proj.attributes.contains(name))     typeName = tr("AttributeSet");
    else if (proj.entities.contains(name))        typeName = tr("Entity");
    else if (proj.dataTypes.contains(name))       typeName = tr("DataType");
    else if (proj.businessRules.contains(name))   typeName = tr("BusinessRule");
    else if (proj.calculations.contains(name))    typeName = tr("Calculation");
    else if (proj.scenarios.contains(name))       typeName = tr("Scenario");
    else if (proj.defines.contains(name))         typeName = tr("Define");
    else if (proj.domainTerms.contains(name))     typeName = tr("DomainTerm");

    m_nameLabel->setText(name);
    m_typeLabel->setText(typeName.isEmpty() ? QString()
        : QStringLiteral("%1  —  %2 : %3")
            .arg(typeName, QFileInfo(loc.filePath).fileName())
            .arg(loc.line));

    const QVector<QStringList> rows = index->attributeRows(name);
    if (rows.isEmpty()) {
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        return;
    }

    const QStringList& headers = rows.first();
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(rows.size() - 1);

    for (int r = 1; r < rows.size(); ++r) {
        const QStringList& cells = rows[r];
        for (int c = 0; c < cells.size() && c < headers.size(); ++c)
            m_table->setItem(r - 1, c, new QTableWidgetItem(cells[c]));
    }
    m_table->resizeColumnsToContents();
}

void AttributeInspectorPanel::clear()
{
    m_nameLabel->setText(tr("(no symbol selected)"));
    m_typeLabel->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(0);
}
