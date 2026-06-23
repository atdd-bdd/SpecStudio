#include "ExampleRunnerDialog.h"
#include "../../analyzer/SpecTableIndex.h"

#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ExampleRunnerDialog::ExampleRunnerDialog(const QString& filePath, int cursorLine,
                                         const SpecTableIndex* index,
                                         QWidget* parent)
    : QDialog(parent, Qt::Tool | Qt::WindowCloseButtonHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    resize(760, 420);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_summary);
    layout->addWidget(m_table);

    const ValidationResult r = run(filePath, cursorLine, index);

    if (!r.errorMsg.isEmpty()) {
        setWindowTitle(tr("Example Runner"));
        m_summary->setText(r.errorMsg);
        return;
    }

    setWindowTitle(tr("Example Runner — %1: %2").arg(r.keyword, r.blockName));
    buildTable(r);
}

// ---------------------------------------------------------------------------
// Main parse + validate
// ---------------------------------------------------------------------------

ExampleRunnerDialog::ValidationResult ExampleRunnerDialog::run(
    const QString& filePath, int cursorLine, const SpecTableIndex* index)
{
    ValidationResult res;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        res.errorMsg = tr("Cannot open file: %1").arg(filePath);
        return res;
    }
    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines << in.readLine();

    static QRegularExpression reDecl(
        R"(^\s*(BusinessRule|Calculation)\s+(\w[\w\s]*\w|\w+))",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reTopLevel(
        R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reExamples(R"(^\s*Examples:\s*(\w+))",
                                          QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow(R"(^\s*\|)");

    // Find the enclosing BusinessRule/Calculation block (search backward from cursor)
    int blockStart = -1;
    for (int i = qMin(cursorLine - 1, lines.size() - 1); i >= 0; --i) {
        auto m = reDecl.match(lines[i]);
        if (m.hasMatch()) {
            res.keyword   = m.captured(1);
            res.blockName = m.captured(2).trimmed();
            blockStart    = i;
            break;
        }
        // Hit a different top-level keyword before finding our block
        auto tm = reTopLevel.match(lines[i]);
        if (tm.hasMatch() && !reDecl.match(lines[i]).hasMatch())
            break;
    }

    if (blockStart < 0) {
        res.errorMsg = tr("Cursor is not inside a BusinessRule or Calculation block.");
        return res;
    }

    // Find the Examples: line inside the block
    int examplesLine = -1;
    for (int i = blockStart + 1; i < lines.size(); ++i) {
        if (i != blockStart && reTopLevel.match(lines[i]).hasMatch()) break;
        auto em = reExamples.match(lines[i]);
        if (em.hasMatch()) {
            res.attrSetName = em.captured(1);
            examplesLine    = i;
            break;
        }
    }

    if (examplesLine < 0) {
        res.errorMsg = tr("No Examples: section found in %1 '%2'.")
                           .arg(res.keyword, res.blockName);
        return res;
    }

    // Look up the AttributeSet definition for field types
    const QVector<QStringList> attrDef = index->attributeRows(res.attrSetName);
    if (attrDef.size() < 2) {
        res.errorMsg = tr("AttributeSet '%1' not found or has no fields.").arg(res.attrSetName);
        return res;
    }

    // attrDef[0] = header row: Attribute, Type, Default, Notes, In-Out, ...
    const QStringList& hdr = attrDef[0];
    int typeCol  = hdr.indexOf("Type",   Qt::CaseInsensitive);
    int inOutCol = hdr.indexOf("In-Out", Qt::CaseInsensitive);
    if (inOutCol < 0) inOutCol = hdr.indexOf("In/Out", Qt::CaseInsensitive);

    for (int r = 1; r < attrDef.size(); ++r) {
        const QStringList& row = attrDef[r];
        if (row.isEmpty()) continue;
        FieldInfo fi;
        fi.name  = row[0];
        fi.type  = (typeCol >= 0 && typeCol < row.size()) ? row[typeCol] : "";
        fi.inOut = (inOutCol >= 0 && inOutCol < row.size()) ? row[inOutCol] : "In";
        if (fi.inOut.isEmpty()) fi.inOut = "In";
        res.fields << fi;
    }

    // Collect Examples table rows
    bool inTable = false;
    QStringList exHeaders;
    for (int i = examplesLine + 1; i < lines.size(); ++i) {
        if (lines[i].trimmed().isEmpty()) { if (inTable) break; continue; }
        if (!inTable && reTopLevel.match(lines[i]).hasMatch()) break;
        if (!reRow.match(lines[i]).hasMatch()) { if (inTable) break; continue; }

        const QStringList parts = lines[i].split('|');
        QStringList cells;
        for (int p = 1; p < parts.size() - 1; ++p)
            cells << parts[p].trimmed();

        if (!inTable) {
            // First row = column headers
            exHeaders = cells;
            inTable = true;
        } else {
            res.dataRows << cells;
        }
    }

    if (res.dataRows.isEmpty()) {
        res.errorMsg = tr("No data rows found in the Examples table for %1 '%2'.")
                           .arg(res.keyword, res.blockName);
        return res;
    }

    // Reorder fields to match the example table's column order
    QVector<FieldInfo> orderedFields;
    for (const QString& col : exHeaders) {
        bool found = false;
        for (const FieldInfo& fi : res.fields) {
            if (fi.name.compare(col, Qt::CaseInsensitive) == 0) {
                orderedFields << fi;
                found = true;
                break;
            }
        }
        if (!found) {
            FieldInfo unknown;
            unknown.name  = col;
            unknown.inOut = "In";
            orderedFields << unknown;
        }
    }
    res.fields = orderedFields;

    // Validate each data row
    for (const QStringList& row : res.dataRows) {
        QVector<CellState> rowStates;
        for (int c = 0; c < res.fields.size(); ++c) {
            const QString val = (c < row.size()) ? row[c] : "";
            rowStates << validateCell(val, res.fields[c]);
        }
        res.states << rowStates;
    }

    return res;
}

// ---------------------------------------------------------------------------
// Validate one cell
// ---------------------------------------------------------------------------

ExampleRunnerDialog::CellState ExampleRunnerDialog::validateCell(
    const QString& value, const FieldInfo& field)
{
    if (value == "?DNC?" || value.compare("DNC", Qt::CaseInsensitive) == 0)
        return CellState::DNC;

    const bool isOutput = (field.inOut.compare("Out", Qt::CaseInsensitive) == 0);

    if (value.isEmpty() || value == "~")
        return isOutput ? CellState::Output : CellState::Missing;

    if (value.startsWith('='))
        return CellState::Valid;   // Define reference — trust it

    if (isOutput)
        return CellState::Output;   // Don't type-validate outputs

    if (!field.type.isEmpty() && !isValidType(QString(value).replace('~', ' '), field.type))
        return CellState::InvalidType;

    return CellState::Valid;
}

bool ExampleRunnerDialog::isValidType(const QString& value, const QString& type)
{
    const QString ltype = type.toLower();
    static QRegularExpression reInt (R"(^-?\d+$)");
    static QRegularExpression reFlt (R"(^-?\d+(\.\d+)?([eE][+-]?\d+)?$)");
    static QRegularExpression reDate(R"(^\d{4}-\d{2}-\d{2}$)");
    static QRegularExpression reTime(R"(^\d{2}:\d{2}(:\d{2})?$)");
    static QRegularExpression reDT  (R"(^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2})");

    if (ltype == "integer")  return reInt.match(value).hasMatch();
    if (ltype == "float")    return reFlt.match(value).hasMatch();
    if (ltype == "boolean")  { auto l = value.toLower(); return l=="true"||l=="false"; }
    if (ltype == "yesno")    { auto l = value.toLower(); return l=="y"||l=="n"||l=="yes"||l=="no"||l=="t"||l=="f"||l=="true"||l=="false"; }
    if (ltype == "date")     return reDate.match(value).hasMatch();
    if (ltype == "time")     return reTime.match(value).hasMatch();
    if (ltype == "datetime") return reDT.match(value).hasMatch();
    return true;  // String, Text, etc. — always valid
}

// ---------------------------------------------------------------------------
// Build the result QTableWidget
// ---------------------------------------------------------------------------

void ExampleRunnerDialog::buildTable(const ValidationResult& r)
{
    const int cols = r.fields.size();
    const int rows = r.dataRows.size();

    m_table->setColumnCount(cols);
    m_table->setRowCount(rows);

    // Column headers: "FieldName (Type/In-Out)"
    QStringList headerLabels;
    for (const FieldInfo& fi : r.fields) {
        QString lbl = fi.name;
        if (!fi.type.isEmpty())  lbl += "\n" + fi.type;
        if (!fi.inOut.isEmpty()) lbl += " [" + fi.inOut + "]";
        headerLabels << lbl;
    }
    m_table->setHorizontalHeaderLabels(headerLabels);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);

    // Color palette
    static const QColor colValid   ("#1E4620");  // dark green
    static const QColor colInvalid ("#5A1A1A");  // dark red
    static const QColor colMissing ("#5A4A00");  // dark yellow
    static const QColor colDNC     ("#2A2A2A");  // grey
    static const QColor colOutput  ("#1A2A3A");  // dark blue-grey

    int validCount = 0, invalidCount = 0, missingCount = 0;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const QString val    = (col < r.dataRows[row].size()) ? r.dataRows[row][col] : "";
            const CellState state = (row < r.states.size() && col < r.states[row].size())
                                     ? r.states[row][col] : CellState::Valid;

            auto* item = new QTableWidgetItem(val);
            item->setTextAlignment(Qt::AlignCenter);

            QColor bg;
            switch (state) {
            case CellState::Valid:        bg = colValid;   ++validCount;   break;
            case CellState::InvalidType:  bg = colInvalid; ++invalidCount; break;
            case CellState::Missing:      bg = colMissing; ++missingCount; break;
            case CellState::DNC:          bg = colDNC;                     break;
            case CellState::Output:       bg = colOutput;                  break;
            }
            item->setBackground(bg);
            item->setForeground(QColor("#DCDCDC"));
            if (state == CellState::InvalidType)
                item->setToolTip(tr("Invalid value for type '%1'").arg(r.fields[col].type));
            else if (state == CellState::Missing)
                item->setToolTip(tr("Missing required input value"));

            m_table->setItem(row, col, item);
        }
    }

    const QString summary = tr("%1 rows — %2 valid, %3 invalid type, %4 missing value")
                                .arg(rows).arg(validCount).arg(invalidCount).arg(missingCount);
    m_summary->setText(tr("<b>%1: %2</b> &nbsp; Examples: %3 &nbsp;&nbsp; %4")
                           .arg(r.keyword, r.blockName, r.attrSetName, summary));
}
