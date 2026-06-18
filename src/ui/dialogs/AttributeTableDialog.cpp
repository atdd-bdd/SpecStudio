#include "AttributeTableDialog.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>

AttributeTableDialog::AttributeTableDialog(const QString& name,
                                           const QVector<QStringList>& rows,
                                           QWidget* parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Attributes: %1").arg(name));
    resize(520, 260);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);

    if (rows.size() >= 1) {
        const QStringList& headers = rows[0];
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);
        m_table->setRowCount(rows.size() - 1);

        for (int r = 1; r < rows.size(); ++r) {
            for (int c = 0; c < rows[r].size() && c < headers.size(); ++c)
                m_table->setItem(r - 1, c, new QTableWidgetItem(rows[r][c]));
        }
        m_table->resizeColumnsToContents();
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_table);
    setLayout(layout);
}
