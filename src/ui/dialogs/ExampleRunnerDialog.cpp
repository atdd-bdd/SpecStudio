#include "ExampleRunnerDialog.h"
#include "../../analyzer/SpecTableIndex.h"
#include "SpectableParser.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
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
// The block under the cursor, from the parse tree
// ---------------------------------------------------------------------------
//
// Until 2026-09-18 this dialog read the file itself: found the block by
// pattern, walked to its Examples: line, split the rows on pipes, and judged
// each cell with its own copies of the Integer/Date rules -- a second
// validateValue, drifting from the first. Now the block, its set and its rows
// come from the tree the converter reads, merged with the project's other
// files, and a cell is judged by the one rule the build applies.

ExampleRunnerDialog::ValidationResult ExampleRunnerDialog::run(
    const QString& filePath, int cursorLine, const SpecTableIndex* index)
{
    ValidationResult res;

    SpectableFile file;
    if (index) {
        file = index->fileWithContext(filePath);
    } else {
        file = SpectableParser().parse(filePath);
        resolveDomainTermTypes(file);
        resolveDefineReferences(file);
    }

    // The block the cursor is in: the last thing that starts at or above the
    // cursor, whatever kind of thing it is. Only a BusinessRule or Calculation
    // has examples to run.
    int lastStart = 0;
    for (const AttrSet& x : file.attrSets)      if (!x.isContext && x.line <= cursorLine) lastStart = qMax(lastStart, x.line);
    for (const Collection& x : file.collections) if (!x.isContext && x.line <= cursorLine) lastStart = qMax(lastStart, x.line);
    for (const Define& x : file.defines)         if (!x.isContext && x.line <= cursorLine) lastStart = qMax(lastStart, x.line);
    for (const Scenario& x : file.scenarios)     if (x.line <= cursorLine)                 lastStart = qMax(lastStart, x.line);
    const NamedBlock* block = nullptr;
    for (const NamedBlock& nb : file.namedBlocks) {
        if (nb.isContext || nb.line > cursorLine) continue;
        if (nb.line >= lastStart) { lastStart = nb.line; block = &nb; }
    }
    if (!block || (block->kind.compare("BusinessRule", Qt::CaseInsensitive) != 0
                   && block->kind.compare("Calculation", Qt::CaseInsensitive) != 0)) {
        res.errorMsg = tr("Cursor is not inside a BusinessRule or Calculation block.");
        return res;
    }
    res.keyword   = block->kind;
    res.blockName = block->name;

    const ExamplesBlock& ex = block->examples;
    if (ex.line == 0) {
        res.errorMsg = tr("No Examples: section found in %1 '%2'.")
                           .arg(res.keyword, res.blockName);
        return res;
    }
    res.attrSetName = ex.attrSetName;

    // The set's fields, the file's own declaration first if it has one.
    const AttrSet* as = nullptr;
    for (const AttrSet& x : file.attrSets)
        if (x.name.compare(res.attrSetName, Qt::CaseInsensitive) == 0
                && (!as || (as->isContext && !x.isContext))) as = &x;
    if (!as || as->fields.isEmpty()) {
        res.errorMsg = tr("AttributeSet '%1' not found or has no fields.").arg(res.attrSetName);
        return res;
    }
    for (const Field& f : as->fields) {
        FieldInfo fi;
        fi.name  = f.name;
        fi.type  = f.type.trimmed();
        fi.inOut = f.inOut.isEmpty() ? QStringLiteral("In") : f.inOut;
        res.fields << fi;
    }

    res.dataRows = ex.rows;
    if (res.dataRows.isEmpty()) {
        res.errorMsg = tr("No data rows found in the Examples table for %1 '%2'.")
                           .arg(res.keyword, res.blockName);
        return res;
    }

    // Columns in the table's own order; a column naming no field is shown as
    // it is, and the build's own finding about it is reported below.
    QVector<FieldInfo> orderedFields;
    for (const QString& col : ex.header) {
        bool found = false;
        for (const FieldInfo& fi : res.fields)
            if (fi.name.compare(col.trimmed(), Qt::CaseInsensitive) == 0) { orderedFields << fi; found = true; break; }
        if (!found) {
            FieldInfo unknown;
            unknown.name  = col.trimmed();
            unknown.inOut = "In";
            orderedFields << unknown;
        }
    }
    res.fields = orderedFields;

    for (const QStringList& row : res.dataRows) {
        QVector<CellState> rowStates;
        for (int c = 0; c < res.fields.size(); ++c)
            rowStates << validateCell(c < row.size() ? row[c].trimmed() : QString(), res.fields[c]);
        res.states << rowStates;
    }

    // What the build would say about this table -- a column naming no field,
    // a field with no column and no default -- from the same validator.
    const int lastLine = ex.rowLines.isEmpty() ? ex.line : ex.rowLines.last();
    for (const ParseMessage& m : validateExamplesTables(file))
        if (m.line >= ex.line && m.line <= lastLine && !m.text.contains("is not a valid"))
            res.findings << m.text;

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
        return CellState::Valid;   // Define reference that did not resolve -- reported elsewhere

    if (isOutput)
        return CellState::Output;   // Don't type-validate outputs

    // The one rule the build applies, not a copy of it.
    if (!field.type.isEmpty() && !isValidValueForType(QString(value).replace('~', ' '), field.type))
        return CellState::InvalidType;

    return CellState::Valid;
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
    QString text = tr("<b>%1: %2</b> &nbsp; Examples: %3 &nbsp;&nbsp; %4")
                       .arg(r.keyword, r.blockName, r.attrSetName, summary);
    for (const QString& finding : r.findings)
        text += "<br/><span style='color:#C0392B;'>" + finding.toHtmlEscaped() + "</span>";
    m_summary->setText(text);
}
