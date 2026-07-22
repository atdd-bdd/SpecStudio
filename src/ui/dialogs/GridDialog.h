#pragma once

#include <QDialog>
#include <QVector>
#include <QStringList>
#include <QEvent>

class QTableWidget;
class QTableWidgetItem;
class QAbstractButton;

class GridDialog : public QDialog {
    Q_OBJECT
public:
    explicit GridDialog(const QVector<QStringList>& data, QWidget* parent = nullptr);

    QVector<QStringList> tableData() const;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void showColumnContextMenu(const QPoint& pos);
    void showRowContextMenu(const QPoint& pos);

    void insertColumnBefore();
    void insertColumnAfter();
    void deleteColumn();
    void copyColumn();
    void pasteColumn();
    void extendColumn();

    void insertRowAbove();
    void insertRowBelow();
    void deleteRow();
    void copyRow();
    void pasteRow();

    void copyTable();
    void pasteTable();
    void transformTable();
    void undoTable();
    void expandTable();

private:
    QTableWidget*    m_table;
    QAbstractButton* m_cornerButton = nullptr;
    int m_contextCol = -1;
    int m_contextRow = -1;

    QVector<QVector<QStringList>> m_undoStack;
    QStringList m_copiedRow;
    QStringList m_copiedCol;

    void pushUndo();
    void appendRow();
    void populateTable(const QVector<QStringList>& data);
    QVector<QStringList> currentData() const;
    QVector<QStringList> parseClipboardTable() const;
    void setupCornerButton();
    void showCornerContextMenu(const QPoint& globalPos);
};
