#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QTableWidget;
class SpecTableIndex;

class ExampleRunnerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExampleRunnerDialog(const QString& filePath, int cursorLine,
                                 const SpecTableIndex* index,
                                 QWidget* parent = nullptr);

private:
    enum class CellState { Valid, InvalidType, Missing, DNC, Output };

    struct FieldInfo {
        QString name;
        QString type;
        QString inOut;   // "In", "Out", "In-Out"
    };

    struct ValidationResult {
        QString keyword;
        QString blockName;
        QString attrSetName;
        QVector<FieldInfo>     fields;
        QVector<QStringList>   dataRows;
        QVector<QVector<CellState>> states;
        QString errorMsg;
    };

    static ValidationResult run(const QString& filePath, int cursorLine,
                                 const SpecTableIndex* index);
    static CellState validateCell(const QString& value, const FieldInfo& field);
    static bool isValidType(const QString& value, const QString& type);

    void buildTable(const ValidationResult& r);

    QLabel*       m_summary = nullptr;
    QTableWidget* m_table   = nullptr;
};
