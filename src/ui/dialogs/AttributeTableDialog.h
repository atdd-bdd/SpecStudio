#pragma once

#include <QDialog>
#include <QVector>
#include <QStringList>

class QTableWidget;

class AttributeTableDialog : public QDialog {
    Q_OBJECT
public:
    explicit AttributeTableDialog(const QString& name,
                                  const QVector<QStringList>& rows,
                                  QWidget* parent = nullptr);
private:
    QTableWidget* m_table;
};
