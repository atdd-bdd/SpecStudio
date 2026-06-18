#include "GridDialog.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QKeyEvent>

GridDialog::GridDialog(const QVector<QStringList>& data, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Table"));
    resize(600, 400);

    m_table = new QTableWidget(this);
    m_table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_table->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &GridDialog::showColumnContextMenu);
    connect(m_table->verticalHeader(), &QHeaderView::customContextMenuRequested,
            this, &GridDialog::showRowContextMenu);

    if (data.isEmpty()) {
        QVector<QStringList> empty;
        for (int r = 0; r < 2; ++r) {
            QStringList row;
            for (int c = 0; c < 3; ++c) row << "";
            empty << row;
        }
        populateTable(empty);
    } else {
        populateTable(data);
    }

    setupCornerButton();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addWidget(buttons);
    setLayout(layout);
}

void GridDialog::setupCornerButton()
{
    m_cornerButton = m_table->findChild<QAbstractButton*>("qt_corner_button");
    if (!m_cornerButton) {
        const auto btns = m_table->findChildren<QAbstractButton*>();
        if (!btns.isEmpty()) m_cornerButton = btns.first();
    }
    if (m_cornerButton)
        m_cornerButton->installEventFilter(this);

    m_table->installEventFilter(this);
}

bool GridDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_cornerButton && event->type() == QEvent::ContextMenu) {
        auto* e = static_cast<QContextMenuEvent*>(event);
        showCornerContextMenu(m_cornerButton->mapToGlobal(e->pos()));
        return true;
    }

    if (obj == m_table && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Tab ||
            ke->key() == Qt::Key_Return ||
            ke->key() == Qt::Key_Enter)
        {
            int lastRow = m_table->rowCount() - 1;
            int lastCol = m_table->columnCount() - 1;
            if (m_table->currentRow() == lastRow &&
                m_table->currentColumn() == lastCol)
            {
                appendRow();
                m_table->setCurrentCell(lastRow + 1, 0);
                return true;
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void GridDialog::appendRow()
{
    pushUndo();
    int newRow = m_table->rowCount();
    m_table->insertRow(newRow);
    for (int c = 0; c < m_table->columnCount(); ++c)
        m_table->setItem(newRow, c, new QTableWidgetItem(""));
}

void GridDialog::populateTable(const QVector<QStringList>& data)
{
    if (data.isEmpty()) return;

    int rows = data.size();
    int cols = 0;
    for (const auto& row : data)
        cols = qMax(cols, row.size());

    m_table->setRowCount(rows);
    m_table->setColumnCount(cols);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            QString val = (c < data[r].size()) ? data[r][c] : "";
            m_table->setItem(r, c, new QTableWidgetItem(val));
        }
    }

    m_table->resizeColumnsToContents();
}

QVector<QStringList> GridDialog::tableData() const
{
    return currentData();
}

QVector<QStringList> GridDialog::currentData() const
{
    QVector<QStringList> result;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            auto* item = m_table->item(r, c);
            row << (item ? item->text() : "");
        }
        result << row;
    }
    return result;
}

void GridDialog::pushUndo()
{
    m_undoStack.append(currentData());
    if (m_undoStack.size() > 50)
        m_undoStack.removeFirst();
}

// ─── Context Menus ─────────────────────────────────────────────────────────────

void GridDialog::showColumnContextMenu(const QPoint& pos)
{
    m_contextCol = m_table->horizontalHeader()->logicalIndexAt(pos);

    QMenu menu(this);
    menu.addAction(tr("Insert Column Before"), this, &GridDialog::insertColumnBefore);
    menu.addAction(tr("Insert Column After"),  this, &GridDialog::insertColumnAfter);
    menu.addSeparator();
    menu.addAction(tr("Delete Column"), this, &GridDialog::deleteColumn);
    menu.addSeparator();
    menu.addAction(tr("Copy Column"),   this, &GridDialog::copyColumn);
    menu.addAction(tr("Paste"),         this, &GridDialog::pasteColumn);
    menu.addSeparator();
    menu.addAction(tr("Extend Column"), this, &GridDialog::extendColumn);
    menu.exec(m_table->horizontalHeader()->mapToGlobal(pos));
}

void GridDialog::showRowContextMenu(const QPoint& pos)
{
    m_contextRow = m_table->verticalHeader()->logicalIndexAt(pos);

    QMenu menu(this);
    menu.addAction(tr("Insert Row Above"), this, &GridDialog::insertRowAbove);
    menu.addAction(tr("Insert Row Below"), this, &GridDialog::insertRowBelow);
    menu.addSeparator();
    menu.addAction(tr("Delete Row"), this, &GridDialog::deleteRow);
    menu.addSeparator();
    menu.addAction(tr("Copy Row"),  this, &GridDialog::copyRow);
    menu.addAction(tr("Paste"),     this, &GridDialog::pasteRow);
    menu.exec(m_table->verticalHeader()->mapToGlobal(pos));
}

void GridDialog::showCornerContextMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    menu.addAction(tr("Paste Table"), this, &GridDialog::pasteTable);
    menu.addAction(tr("Transform"),   this, &GridDialog::transformTable);
    menu.addAction(tr("Undo"),        this, &GridDialog::undoTable);
    menu.addAction(tr("Expand"),      this, &GridDialog::expandTable);
    menu.exec(globalPos);
}

// ─── Column Operations ─────────────────────────────────────────────────────────

void GridDialog::insertColumnBefore()
{
    pushUndo();
    int col = (m_contextCol < 0) ? 0 : m_contextCol;
    m_table->insertColumn(col);
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_table->setItem(r, col, new QTableWidgetItem(""));
}

void GridDialog::insertColumnAfter()
{
    pushUndo();
    int col = (m_contextCol < 0) ? m_table->columnCount() - 1 : m_contextCol;
    m_table->insertColumn(col + 1);
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_table->setItem(r, col + 1, new QTableWidgetItem(""));
}

void GridDialog::deleteColumn()
{
    if (m_contextCol < 0 || m_table->columnCount() <= 1) return;
    pushUndo();
    m_table->removeColumn(m_contextCol);
}

void GridDialog::copyColumn()
{
    if (m_contextCol < 0) return;
    m_copiedCol.clear();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* item = m_table->item(r, m_contextCol);
        m_copiedCol << (item ? item->text() : "");
    }
}

void GridDialog::extendColumn()
{
    if (m_contextCol < 0) return;
    int maxLen = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* item = m_table->item(r, m_contextCol);
        if (item) maxLen = qMax(maxLen, item->text().length());
    }
    QFontMetrics fm(m_table->font());
    int pixelWidth = fm.horizontalAdvance(QString(maxLen + 4, 'W'));
    m_table->setColumnWidth(m_contextCol, pixelWidth);
}

void GridDialog::pasteColumn()
{
    if (m_contextCol < 0 || m_copiedCol.isEmpty()) return;
    pushUndo();
    for (int r = 0; r < qMin(m_table->rowCount(), m_copiedCol.size()); ++r)
        m_table->setItem(r, m_contextCol, new QTableWidgetItem(m_copiedCol[r]));
}

// ─── Row Operations ───────────────────────────────────────────────────────────

void GridDialog::insertRowAbove()
{
    pushUndo();
    int row = (m_contextRow < 0) ? 0 : m_contextRow;
    m_table->insertRow(row);
    for (int c = 0; c < m_table->columnCount(); ++c)
        m_table->setItem(row, c, new QTableWidgetItem(""));
}

void GridDialog::insertRowBelow()
{
    pushUndo();
    int row = (m_contextRow < 0) ? m_table->rowCount() - 1 : m_contextRow;
    m_table->insertRow(row + 1);
    for (int c = 0; c < m_table->columnCount(); ++c)
        m_table->setItem(row + 1, c, new QTableWidgetItem(""));
}

void GridDialog::deleteRow()
{
    if (m_contextRow < 0 || m_table->rowCount() <= 1) return;
    pushUndo();
    m_table->removeRow(m_contextRow);
}

void GridDialog::copyRow()
{
    if (m_contextRow < 0) return;
    m_copiedRow.clear();
    for (int c = 0; c < m_table->columnCount(); ++c) {
        auto* item = m_table->item(m_contextRow, c);
        m_copiedRow << (item ? item->text() : "");
    }
}

void GridDialog::pasteRow()
{
    if (m_contextRow < 0 || m_copiedRow.isEmpty()) return;
    pushUndo();
    for (int c = 0; c < qMin(m_table->columnCount(), m_copiedRow.size()); ++c)
        m_table->setItem(m_contextRow, c, new QTableWidgetItem(m_copiedRow[c]));
}

// ─── Table Operations ─────────────────────────────────────────────────────────

QVector<QStringList> GridDialog::parseClipboardTable() const
{
    QString text = QApplication::clipboard()->text();
    text.replace("\r\n", "\n").replace('\r', '\n');

    QVector<QStringList> result;
    for (const QString& line : text.split('\n')) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;

        QStringList row;
        if (trimmedLine.startsWith('|')) {
            for (const QString& cell : trimmedLine.split('|')) {
                QString c = cell.trimmed();
                if (!c.isEmpty())
                    row << c;
            }
        } else {
            for (const QString& cell : line.split('\t'))
                row << cell.trimmed();
        }
        if (!row.isEmpty())
            result << row;
    }
    return result;
}

void GridDialog::pasteTable()
{
    QVector<QStringList> data = parseClipboardTable();
    if (data.isEmpty()) return;
    pushUndo();
    populateTable(data);
}

void GridDialog::transformTable()
{
    pushUndo();
    QVector<QStringList> data = currentData();
    if (data.isEmpty()) return;

    int rows = data.size();
    int cols = 0;
    for (const auto& r : data) cols = qMax(cols, r.size());

    QVector<QStringList> transposed;
    for (int c = 0; c < cols; ++c) {
        QStringList row;
        for (int r = 0; r < rows; ++r)
            row << (c < data[r].size() ? data[r][c] : "");
        transposed << row;
    }
    populateTable(transposed);
}

void GridDialog::undoTable()
{
    if (m_undoStack.isEmpty()) return;
    populateTable(m_undoStack.takeLast());
}

void GridDialog::expandTable()
{
    m_table->resizeColumnsToContents();
}
