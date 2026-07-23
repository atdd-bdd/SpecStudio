#include "SpecTableEditor.h"
#include "syntax/SpecTableHighlighter.h"
#include "../analyzer/SpecTableIndex.h"
#include "../ui/dialogs/AttributeTableDialog.h"
#include "../ui/dialogs/BackgroundCleanupDialog.h"
#include "../ui/dialogs/ExampleRunnerDialog.h"
#include "../ui/dialogs/ScenarioSimulatorDialog.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QMenu>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolTip>

SpecTableEditor::SpecTableEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new SpecTableHighlighter(textEdit()->document()));

    m_staticKeywords = {
        "Specification ", "Entity ", "Collection ", "DomainTerm ", "DataType ", "Attributes ",
        "BusinessRule ", "Calculation ", "Import ", "Insert ", "Define ",
        "Scenario ", "ScenarioGroup ", "Background ", "Cleanup ",
        "Description ", "Details ", "Constraint ",
        "Examples: EnumerationValues", "Examples: ValidValues", "Examples: ",
        "Vertical",
        "Given ", "When ", "Then ", "And ", "WhenThen ",
        "applying BusinessRule ", "applying Calculation ",
        // Built-in DataTypes
        "Character", "String", "Text", "Integer", "Scientific",
        "Boolean", "Date", "Time", "DateTime", "Duration", "YesNo",
        // Built-in AttributeSets
        "EnumerationValues", "ValidValues",
    };
    setCompletionWords(m_staticKeywords);

    textEdit()->installEventFilter(this);

    connect(textEdit(), &QPlainTextEdit::cursorPositionChanged, this, [this] {
        if (!m_index) return;
        const QTextCursor cur = textEdit()->textCursor();
        QTextCursor wc = cur;
        wc.select(QTextCursor::WordUnderCursor);
        const QString word = wc.selectedText().trimmed();
        if (!word.isEmpty() && !m_index->projectSymbols().locationFor(word).filePath.isEmpty())
            emit symbolAtCursor(word);
        else
            emit symbolAtCursor({});
    });

    lineNumberEdit()->setFoldPattern(
        QRegularExpression(
            R"(^\s*(Specification|Entity|Collection|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
            QRegularExpression::CaseInsensitiveOption));
}

// ---------------------------------------------------------------------------
// Save — auto-format all pipe tables before writing to disk
// ---------------------------------------------------------------------------

bool SpecTableEditor::save()
{
    formatAllTables();
    fixTrailingContinuations();
    return PlainTextEditor::save();
}

// ---------------------------------------------------------------------------
// Strip the trailing ' \' from the last line of each continuation block.
// Opener lines like "Details \" keep their backslash; only indented
// continuation lines that end a block are cleaned.
// ---------------------------------------------------------------------------

void SpecTableEditor::fixTrailingContinuations()
{
    QTextDocument* doc = textEdit()->document();
    QTextCursor tc(doc);
    tc.beginEditBlock();

    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const QString text = b.text();
        // Only indented lines ending with ' \' are candidates
        if (text.isEmpty() || !text.at(0).isSpace()) continue;
        if (!text.endsWith(QStringLiteral(" \\"))) continue;

        // Check whether the next line is also a continuation (indented)
        const QTextBlock next = b.next();
        const bool nextIsContinuation = next.isValid()
                                        && !next.text().isEmpty()
                                        && next.text().at(0).isSpace();
        if (nextIsContinuation) continue;

        // Last continuation line — remove the trailing ' \'
        tc.setPosition(b.position() + text.length() - 2);
        tc.setPosition(b.position() + text.length(), QTextCursor::KeepAnchor);
        tc.removeSelectedText();
    }

    tc.endEditBlock();
}

// ---------------------------------------------------------------------------
// Event filter — intercept Tab key for table cell navigation
// ---------------------------------------------------------------------------

bool SpecTableEditor::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == textEdit() && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Tab && !(ke->modifiers() & Qt::ShiftModifier)
            && !QApplication::activePopupWidget())
        {
            if (handleTableTabKey())
                return true;
            if (tryExpandSnippet())
                return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && ke->modifiers() == Qt::NoModifier)
        {
            QTimer::singleShot(0, this, &SpecTableEditor::autoInsertTableHeader);
            QTimer::singleShot(0, this, &SpecTableEditor::checkAdHocTableAttributeSet);
        }
        if (ke->key() == Qt::Key_Slash && ke->modifiers() == Qt::ControlModifier) {
            toggleLineComment();
            return true;
        }
    }
    if (obj == textEdit()->viewport() && event->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(event);
        showHoverPreview(he->pos(), he->globalPos());
        return true;
    }
    return PlainTextEditor::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// Hover preview — show popup with symbol info when mouse rests on a word
// ---------------------------------------------------------------------------

void SpecTableEditor::showHoverPreview(const QPoint& viewportPos, const QPoint& globalPos)
{
    if (!m_index) { QToolTip::hideText(); return; }

    QTextCursor tc = textEdit()->cursorForPosition(viewportPos);
    tc.select(QTextCursor::WordUnderCursor);
    const QString word = tc.selectedText().trimmed();
    if (word.isEmpty()) { QToolTip::hideText(); return; }

    const SpecTableSymbols& syms = m_index->projectSymbols();
    QString tipText;

    if (syms.hasAttributeSet(word)) {
        const QVector<QStringList> rows = m_index->attributeRows(word);
        if (!rows.isEmpty()) {
            tipText = QStringLiteral("<b>%1</b><table border='1' cellpadding='3' style='margin-top:4px'>")
                          .arg(word.toHtmlEscaped());
            for (int r = 0; r < rows.size(); ++r) {
                tipText += (r == 0) ? "<tr style='background:#e0e0e0'>" : "<tr>";
                for (const QString& cell : rows[r])
                    tipText += (r == 0)
                        ? QStringLiteral("<th>%1</th>").arg(cell.toHtmlEscaped())
                        : QStringLiteral("<td>%1</td>").arg(cell.toHtmlEscaped());
                tipText += "</tr>";
            }
            tipText += "</table>";
        } else if (word.compare("ValidValues", Qt::CaseInsensitive) == 0) {
            tipText = QStringLiteral(
                "<b>ValidValues</b> (built-in)<br>"
                "<table border='1' cellpadding='3' style='margin-top:4px'>"
                "<tr style='background:#e0e0e0'><th>Value</th><th>IsValid</th><th>Notes</th></tr>"
                "<tr><td>example</td><td>true/false</td><td>optional</td></tr>"
                "</table>");
        } else if (word.compare("EnumerationValues", Qt::CaseInsensitive) == 0) {
            tipText = QStringLiteral(
                "<b>EnumerationValues</b> (built-in)<br>"
                "<table border='1' cellpadding='3' style='margin-top:4px'>"
                "<tr style='background:#e0e0e0'><th>Value</th><th>Notes</th></tr>"
                "<tr><td>CONSTANT_NAME</td><td>optional</td></tr>"
                "</table>");
        } else {
            tipText = QStringLiteral("<b>AttributeSet:</b> %1 (built-in)").arg(word.toHtmlEscaped());
        }
    } else {
        const SymbolLocation loc = syms.locationFor(word);
        if (loc.filePath.isEmpty()) { QToolTip::hideText(); return; }

        QString type;
        if      (syms.dataTypes.contains(word))      type = "DataType";
        else if (syms.businessRules.contains(word))  type = "BusinessRule";
        else if (syms.calculations.contains(word))   type = "Calculation";
        else if (syms.domainTerms.contains(word))    type = "DomainTerm";
        else if (syms.defines.contains(word))         type = "Define";
        else if (syms.scenarios.contains(word))       type = "Scenario";
        else if (syms.specifications.contains(word))  type = "Specification";

        if (type == "Define") {
            // If cursor is on a =DefineName reference, show the define value
            QTextCursor tcCheck = tc;
            tcCheck.setPosition(tc.selectionStart());
            tcCheck.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
            if (tcCheck.selectedText() == "=" && m_index) {
                const auto info = m_index->defineInfo(word);
                if (!info.first.isEmpty()) {
                    const QString val = info.first.toHtmlEscaped().replace("\n", "<br>");
                    tipText = QStringLiteral("<b>=%1</b> = %2<br><small>%3 &mdash; line %4</small>")
                                  .arg(word.toHtmlEscaped(), val,
                                       QFileInfo(loc.filePath).fileName().toHtmlEscaped(),
                                       QString::number(loc.line));
                } else if (!info.second.isEmpty()) {
                    tipText = QStringLiteral("<b>=%1</b><table border='1' cellpadding='3' style='margin-top:4px'>")
                                  .arg(word.toHtmlEscaped());
                    for (int r = 0; r < info.second.size(); ++r) {
                        tipText += (r == 0) ? "<tr style='background:#e0e0e0'>" : "<tr>";
                        for (const QString& cell : info.second[r])
                            tipText += (r == 0)
                                ? QStringLiteral("<th>%1</th>").arg(cell.toHtmlEscaped())
                                : QStringLiteral("<td>%1</td>").arg(cell.toHtmlEscaped());
                        tipText += "</tr>";
                    }
                    tipText += QStringLiteral("</table><br><small>%1 &mdash; line %2</small>")
                                   .arg(QFileInfo(loc.filePath).fileName().toHtmlEscaped())
                                   .arg(loc.line);
                }
            }
        }

        if (!type.isEmpty() && tipText.isEmpty())
            tipText = QStringLiteral("<b>%1:</b> %2<br><small>%3 &mdash; line %4</small>")
                          .arg(type, word.toHtmlEscaped(),
                               QFileInfo(loc.filePath).fileName().toHtmlEscaped(),
                               QString::number(loc.line));
    }

    if (!tipText.isEmpty())
        QToolTip::showText(globalPos, tipText, textEdit()->viewport());
    else
        QToolTip::hideText();
}

// ---------------------------------------------------------------------------
// Tab navigation between pipe-table cells
// ---------------------------------------------------------------------------
// Snippet expansion — trigger word + Tab → multi-line block template
// ---------------------------------------------------------------------------

bool SpecTableEditor::tryExpandSnippet()
{
    QTextCursor tc = textEdit()->textCursor();
    const QString lineText = tc.block().text();

    // Only expand when the entire trimmed line equals a trigger (no partial lines)
    const QString trigger = lineText.trimmed().toLower();
    if (trigger.isEmpty()) return false;

    // Map: trigger → { template text, offset within first line to select "Name" }
    struct Snippet {
        QString text;
        QString placeholder; // text to select after insertion (first occurrence)
    };

    static const QMap<QString, Snippet> snippets = {
        { "brule", {
            "BusinessRule Name\n"
            "Details \n"
            "Examples: AttrSetName\n"
            "| Attribute | Expected |\n"
            "|           |          |",
            "Name" } },
        { "calc", {
            "Calculation Name\n"
            "Details \n"
            "Examples: AttrSetName\n"
            "| Input | Expected |\n"
            "|       |          |",
            "Name" } },
        { "scenario", {
            "Scenario Name\n"
            "  Given initial condition: AttrSetName\n"
            "  When action is taken: AttrSetName\n"
            "  Then result is verified: AttrSetName",
            "Name" } },
        { "sgroup", {
            "ScenarioGroup Name\n"
            "  Given initial condition: AttrSetName\n"
            "  When action is taken: AttrSetName\n"
            "  Then result is verified: AttrSetName",
            "Name" } },
        { "bgd", {
            "Background\n"
            "  Given initial condition: AttrSetName",
            "AttrSetName" } },
        { "attrs", {
            "Attributes Name\n"
            "| Attribute | Type | Default | Notes | In-Out |\n"
            "|           |      |         |       | In     |",
            "Name" } },
        { "entity", {
            "Entity Name\n"
            "| Attribute | Type | Default | Notes | In-Out |\n"
            "|           |      |         |       | In     |",
            "Name" } },
        { "define", {
            "Define Name = ",
            "Name" } },
        { "spec", {
            "Specification Name\n"
            "Description ",
            "Name" } },
        { "dterm", {
            "DomainTerm Name\n"
            "Description ",
            "Name" } },
        { "dtype", {
            "DataType Name\n"
            "Details \n"
            "Examples: EnumerationValues\n"
            "| Value  |\n"
            "| Value1 |\n"
            "| Value2 |",
            "Name" } },
        { "import", {
            "Import \"filename.spectable\"",
            "filename.spectable" } },
    };

    if (!snippets.contains(trigger)) return false;

    const Snippet& sn = snippets[trigger];

    // Replace the entire current line with the expanded template
    tc.select(QTextCursor::LineUnderCursor);
    tc.removeSelectedText();
    const int insertPos = tc.position();
    tc.insertText(sn.text);

    // Select the first occurrence of the placeholder so the user can type the name
    if (!sn.placeholder.isEmpty()) {
        QTextCursor search = textEdit()->document()->find(
            sn.placeholder,
            insertPos,
            QTextDocument::FindCaseSensitively);
        if (!search.isNull())
            textEdit()->setTextCursor(search);
    }

    return true;
}

// ---------------------------------------------------------------------------

bool SpecTableEditor::handleTableTabKey()
{
    QTextCursor tc = textEdit()->textCursor();
    const QString line = tc.block().text();
    if (!line.trimmed().startsWith('|')) return false;

    const int curPos = tc.positionInBlock();

    // Collect pipe positions in the current line
    QList<int> pipes;
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '|') pipes << i;
    if (pipes.size() < 2) return false;

    // Determine which inter-pipe region the cursor is in
    int region = -1;
    for (int i = 0; i < pipes.size() - 1; ++i) {
        if (curPos >= pipes[i] && curPos <= pipes[i + 1]) { region = i; break; }
    }

    // ── "Create DataType?" prompt when leaving an unrecognised Type cell ─────
    if (m_index && region >= 0) {
        // Find the table header row
        QTextBlock hdrBlock = tc.block();
        while (hdrBlock.previous().isValid()
               && hdrBlock.previous().text().trimmed().startsWith('|'))
            hdrBlock = hdrBlock.previous();

        if (hdrBlock != tc.block()) {  // not on the header row itself
            auto parseHeader = [](const QString& rowLine) -> QStringList {
                QString t = rowLine.trimmed();
                if (t.startsWith('|')) t = t.mid(1);
                if (t.endsWith('|'))   t.chop(1);
                QStringList cells = t.split('|');
                for (auto& c : cells) c = c.trimmed();
                return cells;
            };
            const QStringList hdr = parseHeader(hdrBlock.text());

            int typeColIdx = -1;
            for (int i = 0; i < hdr.size(); ++i)
                if (hdr[i].compare("Type", Qt::CaseInsensitive) == 0) { typeColIdx = i; break; }

            if (typeColIdx >= 0 && region == typeColIdx) {
                const QString cellValue =
                    line.mid(pipes[region] + 1, pipes[region + 1] - pipes[region] - 1).trimmed();

                if (!cellValue.isEmpty()) {
                    static const QStringList builtInTypes = {
                        "Boolean", "Character", "Date", "DateTime", "Duration",
                        "Float", "Integer", "Scientific", "String", "Text", "Time", "YesNo"
                    };
                    const SpecTableSymbols& syms = m_index->projectSymbols();
                    const bool known =
                        builtInTypes.contains(cellValue, Qt::CaseInsensitive)
                        || syms.dataTypes.contains(cellValue);

                    if (!known) {
                        const auto ans = QMessageBox::question(
                            textEdit(),
                            tr("Unknown Type"),
                            tr("'%1' is not a known built-in type or DataType.\n"
                               "Create a DataType block for it?").arg(cellValue),
                            QMessageBox::Yes | QMessageBox::No);
                        if (ans == QMessageBox::Yes) {
                            QTextCursor end = textEdit()->textCursor();
                            end.movePosition(QTextCursor::End);
                            end.insertText(
                                QString("\n\nDataType %1\nDetails \n"
                                        "Examples: EnumerationValues\n"
                                        "| Value  |\n"
                                        "| Value1 |\n"
                                        "| Value2 |").arg(cellValue));
                            // Navigate cursor to the newly created DataType line
                            QTextCursor found = textEdit()->document()->find(
                                QStringLiteral("DataType ") + cellValue,
                                end.position(),
                                QTextDocument::FindBackward);
                            if (!found.isNull()) {
                                found.movePosition(QTextCursor::StartOfBlock);
                                textEdit()->setTextCursor(found);
                                textEdit()->ensureCursorVisible();
                            }
                        }
                    }
                }
            }
        }
    }

    const int nextRegion = region + 1;
    if (nextRegion < pipes.size() - 1) {
        // Select content of the next cell (trim whitespace)
        int start = pipes[nextRegion] + 1;
        int end   = pipes[nextRegion + 1];
        while (start < end && line[start].isSpace())   ++start;
        while (end > start && line[end - 1].isSpace()) --end;
        const int base = tc.block().position();
        tc.setPosition(base + start);
        tc.setPosition(base + end, QTextCursor::KeepAnchor);
        textEdit()->setTextCursor(tc);
    } else {
        // Last cell of this row: move to first cell of the next table row
        const QTextBlock nextBlock = tc.block().next();
        if (nextBlock.isValid() && nextBlock.text().trimmed().startsWith('|')) {
            const QString next = nextBlock.text();
            QList<int> np;
            for (int i = 0; i < next.size(); ++i) if (next[i] == '|') np << i;
            if (np.size() >= 2) {
                int s = np[0] + 1, e = np[1];
                while (s < e && next[s].isSpace())   ++s;
                while (e > s && next[e - 1].isSpace()) --e;
                QTextCursor nc(nextBlock);
                nc.setPosition(nextBlock.position() + s);
                nc.setPosition(nextBlock.position() + e, QTextCursor::KeepAnchor);
                textEdit()->setTextCursor(nc);
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Auto-format all pipe tables in the document
// ---------------------------------------------------------------------------

void SpecTableEditor::formatAllTables()
{
    QTextDocument* doc = textEdit()->document();
    static QRegularExpression reRow(R"(^\s*\|)");

    QList<int> starts;
    bool prevWasRow = false;
    for (int i = 0; i < doc->blockCount(); ++i) {
        const bool isRow = reRow.match(doc->findBlockByNumber(i).text()).hasMatch();
        if (isRow && !prevWasRow) starts.append(i);
        prevWasRow = isRow;
    }
    if (starts.isEmpty()) return;

    const QTextCursor saved = textEdit()->textCursor();
    for (int blockNum : starts) {
        QTextBlock b = doc->findBlockByNumber(blockNum);
        if (!b.isValid()) continue;
        QTextCursor tc(b);
        tc.movePosition(QTextCursor::StartOfBlock);
        textEdit()->setTextCursor(tc);
        formatTable();
    }
    textEdit()->setTextCursor(saved);
}

// ---------------------------------------------------------------------------
// Insert a blank row below the current table row
// ---------------------------------------------------------------------------

void SpecTableEditor::insertTableRow()
{
    QTextCursor tc = textEdit()->textCursor();
    const QString line = tc.block().text();
    if (!line.trimmed().startsWith('|')) return;

    const int cols = line.split('|').size() - 2;
    if (cols < 1) return;

    QString indent;
    for (const QChar ch : line) { if (!ch.isSpace()) break; indent += ch; }

    QString newRow = indent + "|";
    for (int i = 0; i < cols; ++i) newRow += "   |";

    tc.movePosition(QTextCursor::EndOfBlock);
    tc.insertText("\n" + newRow);
    textEdit()->setTextCursor(tc);
}

// ---------------------------------------------------------------------------
// Delete the current table row
// ---------------------------------------------------------------------------

void SpecTableEditor::deleteTableRow()
{
    QTextCursor tc = textEdit()->textCursor();
    if (!tc.block().text().trimmed().startsWith('|')) return;

    tc.beginEditBlock();
    tc.movePosition(QTextCursor::StartOfBlock);
    tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
    if (!tc.atEnd())
        tc.deleteChar();
    else if (!tc.atStart())
        tc.deletePreviousChar();
    tc.endEditBlock();
    textEdit()->setTextCursor(tc);
}

// ---------------------------------------------------------------------------
// Transpose the current pipe table (rows ↔ columns)
// ---------------------------------------------------------------------------

void SpecTableEditor::transposeTable()
{
    QTextDocument* doc = textEdit()->document();
    QTextCursor tc = textEdit()->textCursor();
    static QRegularExpression reRow(R"(^\s*\|)");

    QTextBlock cur = tc.block();
    if (!reRow.match(cur.text()).hasMatch()) return;

    QTextBlock first = cur, last = cur;
    while (reRow.match(first.previous().text()).hasMatch()) first = first.previous();
    while (reRow.match(last.next().text()).hasMatch())      last  = last.next();

    auto parseRow = [](const QString& line) -> QStringList {
        QString t = line.trimmed();
        if (t.startsWith('|')) t = t.mid(1);
        if (t.endsWith('|'))   t.chop(1);
        QStringList cells = t.split('|');
        for (auto& c : cells) c = c.trimmed();
        return cells;
    };

    QList<QStringList> rows;
    for (QTextBlock b = first; ; b = b.next()) { rows << parseRow(b.text()); if (b == last) break; }

    if (rows.isEmpty()) return;
    int maxCols = 0;
    for (const auto& r : rows) maxCols = qMax(maxCols, r.size());

    // Transpose
    QList<QStringList> t;
    for (int c = 0; c < maxCols; ++c) {
        QStringList nr;
        for (const auto& r : rows) nr << (c < r.size() ? r[c] : QString());
        t << nr;
    }

    const int numCols = rows.size();
    QVector<int> widths(numCols, 0);
    for (const auto& r : t)
        for (int i = 0; i < r.size(); ++i) widths[i] = qMax(widths[i], r[i].length());

    QString indent;
    for (const QChar ch : first.text()) { if (!ch.isSpace()) break; indent += ch; }

    QStringList newLines;
    for (const auto& r : t) {
        QString line = indent + "|";
        for (int i = 0; i < numCols; ++i)
            line += " " + (i < r.size() ? r[i] : QString()).leftJustified(widths[i]) + " |";
        newLines << line;
    }

    tc.setPosition(first.position());
    tc.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    tc.insertText(newLines.join("\n"));
}

// Finds the last non-empty line of the top-level block (Scenario/ScenarioGroup/
// Background/Cleanup/BusinessRule/Calculation/etc.) containing `from`, so a
// new declaration can be inserted right after that block ends instead of
// splicing into its middle.
static QTextBlock endOfEnclosingBlock(const QTextBlock& from)
{
    static QRegularExpression reTopLevel(
        R"(^\s*(Specification|Entity|Collection|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
        QRegularExpression::CaseInsensitiveOption);
    QTextBlock last = from;
    QTextBlock b = from.next();
    while (b.isValid()) {
        if (reTopLevel.match(b.text()).hasMatch())
            break;
        if (!b.text().trimmed().isEmpty())
            last = b;
        b = b.next();
    }
    return last;
}

// ---------------------------------------------------------------------------
// Auto-insert table header when Enter is pressed at the end of a step line
// ---------------------------------------------------------------------------

void SpecTableEditor::autoInsertTableHeader()
{
    QTextCursor tc     = textEdit()->textCursor();
    QTextBlock curBlk  = tc.block();
    QTextBlock prevBlk = curBlk.previous();

    if (!prevBlk.isValid()) return;
    if (!curBlk.text().trimmed().isEmpty()) return;
    if (curBlk.next().isValid() && curBlk.next().text().trimmed().startsWith('|')) return;

    const QString prevLine = prevBlk.text();
    const QString prevTrimmed = prevLine.trimmed();

    QString indent;
    for (const QChar ch : prevLine) { if (!ch.isSpace()) break; indent += ch; }

    // ── Case 1: Attributes/Entity line → insert standard header ──────────────
    {
        static QRegularExpression reAttrDecl(
            R"(^\s*(Attributes|Entity)\s+\S+)",
            QRegularExpression::CaseInsensitiveOption);
        if (reAttrDecl.match(prevLine).hasMatch()) {
            const QString hdr = indent + "| Attribute | Type | Default | Notes |";
            tc.movePosition(QTextCursor::StartOfBlock);
            tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            tc.insertText(hdr);
            return;
        }
    }

    // ── Case 1b: Collection line → insert its declaration header ─────────────
    {
        static QRegularExpression reCollDecl(
            R"(^\s*Collection\s+\S+)",
            QRegularExpression::CaseInsensitiveOption);
        if (reCollDecl.match(prevLine).hasMatch()) {
            const QString hdr  = indent + "| DataType | Minimum | Maximum | Notes |";
            const QString data = indent + "|          |         |         |       |";
            tc.movePosition(QTextCursor::StartOfBlock);
            tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            tc.insertText(hdr + "\n" + data);
            return;
        }
    }

    // ── Case 2: Examples: Name — ValidValues/EnumerationValues built-ins, a
    //    known AttrSet, or an unknown one (prompt to create/pick) ────────────
    {
        static QRegularExpression reExamples(
            R"(^\s*Examples:\s*(\w+)\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reExamples.match(prevLine);
        if (m.hasMatch()) {
            const QString name      = m.captured(1);
            const QString nameLower = name.toLower();

            if (nameLower == "validvalues" || nameLower == "enumerationvalues") {
                const QString hdr = (nameLower == "validvalues")
                    ? indent + "| Value | IsValid | Notes |"
                    : indent + "| Value | Notes |";
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                tc.insertText(hdr);
                return;
            }

            if (!m_index) return;
            const SpecTableSymbols& syms = m_index->projectSymbols();

            if (syms.hasAttributeSet(name)) {
                const QVector<QStringList> attrDef = m_index->attributeRows(name);
                if (attrDef.size() < 2) return;
                QStringList attrNames;
                for (int r = 1; r < attrDef.size(); ++r)
                    if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
                if (attrNames.isEmpty()) return;

                QString hdr = indent + "|", data = indent + "|";
                for (const QString& a : attrNames) {
                    hdr  += " " + a + " |";
                    data += " " + QString(a.length(), ' ') + " |";
                }
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                tc.insertText(hdr + "\n" + data);
                return;
            }

            if (syms.dataTypes.contains(name)) return; // handled elsewhere; not this table shape

            // Unknown AttributeSet referenced by Examples: — same offer as an
            // unknown step AttrSet, anchored to this Examples: line instead.
            QStringList known;
            for (auto it = syms.attributes.begin(); it != syms.attributes.end(); ++it)
                known << it.key();
            for (auto it = syms.entities.begin(); it != syms.entities.end(); ++it)
                if (!known.contains(it.key())) known << it.key();
            known.sort(Qt::CaseInsensitive);

            QMessageBox box(QMessageBox::Question,
                tr("Unknown AttributeSet"),
                tr("AttributeSet '%1' is not defined in this project.\n\n"
                   "What would you like to do?").arg(name),
                QMessageBox::NoButton, textEdit());
            QPushButton* createBtn = box.addButton(tr("Create '%1'").arg(name), QMessageBox::AcceptRole);
            QPushButton* pickBtn   = nullptr;
            if (!known.isEmpty())
                pickBtn = box.addButton(tr("Pick Existing..."), QMessageBox::ActionRole);
            box.addButton(QMessageBox::Cancel);
            box.exec();

            QAbstractButton* clicked = box.clickedButton();
            if (clicked == static_cast<QAbstractButton*>(createBtn)) {
                QTextBlock afterBlock = endOfEnclosingBlock(prevBlk);
                QTextCursor end(afterBlock);
                end.movePosition(QTextCursor::EndOfBlock);
                end.insertText(QString("\n\nAttributes %1\n| Attribute | Type | Default | Notes |\n|           |      |         |       |")
                               .arg(name));

                const QString hdr = indent + "| " + name + " |  |";
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                tc.insertText(hdr);
            } else if (pickBtn && clicked == static_cast<QAbstractButton*>(pickBtn)) {
                bool ok = false;
                const QString picked = QInputDialog::getItem(
                    textEdit(), tr("Pick AttributeSet"),
                    tr("Replace '%1' with:").arg(name),
                    known, 0, false, &ok);
                if (ok && !picked.isEmpty()) {
                    QTextCursor prev(prevBlk);
                    prev.movePosition(QTextCursor::StartOfBlock);
                    prev.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                    prev.insertText(indent + "Examples: " + picked);

                    QTimer::singleShot(0, this, [this, picked, indent]() {
                        if (!m_index) return;
                        const QVector<QStringList> attrDef = m_index->attributeRows(picked);
                        if (attrDef.size() < 2) return;
                        QStringList attrNames;
                        for (int r = 1; r < attrDef.size(); ++r)
                            if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
                        if (attrNames.isEmpty()) return;
                        QString hdr2 = indent + "|", data2 = indent + "|";
                        for (const QString& a : attrNames) {
                            hdr2  += " " + a + " |";
                            data2 += " " + QString(a.length(), ' ') + " |";
                        }
                        QTextCursor tc2 = textEdit()->textCursor();
                        tc2.movePosition(QTextCursor::StartOfBlock);
                        tc2.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                        tc2.insertText(hdr2 + "\n" + data2);
                    });
                }
            }
            return;
        }
    }

    // ── Case 3: Step line with : AttrSetName ──────────────────────────────────
    {
        static QRegularExpression reStep(
            R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(\s+Vertical)?\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        static QRegularExpression reApplying(
            R"(\bapplying\s+(?:BusinessRule|Calculation)\b)",
            QRegularExpression::CaseInsensitiveOption);

        auto m = reStep.match(prevLine);
        if (!m.hasMatch() || reApplying.match(prevLine).hasMatch()) return;

        const QString name       = m.captured(1);
        const bool    vertical = !m.captured(2).trimmed().isEmpty();

        if (!m_index) return;
        const SpecTableSymbols& syms = m_index->projectSymbols();

        // ── Unknown AttributeSet: prompt to create or pick ────────────────────
        if (!syms.hasAttributeSet(name) && !syms.dataTypes.contains(name)) {
            // Collect known attribute set names for the "pick" option
            QStringList known;
            for (auto it = syms.attributes.begin(); it != syms.attributes.end(); ++it)
                known << it.key();
            for (auto it = syms.entities.begin(); it != syms.entities.end(); ++it)
                if (!known.contains(it.key())) known << it.key();
            known.sort(Qt::CaseInsensitive);

            QMessageBox box(QMessageBox::Question,
                tr("Unknown AttributeSet"),
                tr("AttributeSet '%1' is not defined in this project.\n\n"
                   "What would you like to do?").arg(name),
                QMessageBox::NoButton, textEdit());
            QPushButton* createBtn = box.addButton(tr("Create '%1'").arg(name), QMessageBox::AcceptRole);
            QPushButton* pickBtn   = nullptr;
            if (!known.isEmpty())
                pickBtn = box.addButton(tr("Pick Existing..."), QMessageBox::ActionRole);
            box.addButton(QMessageBox::Cancel);
            box.exec();

            QAbstractButton* clicked = box.clickedButton();
            if (clicked == static_cast<QAbstractButton*>(createBtn)) {
                // Insert "Attributes <name>" block at end of file with header row
                QTextCursor end = textEdit()->textCursor();
                end.movePosition(QTextCursor::End);
                end.insertText(QString("\n\nAttributes %1\n| Attribute | Type | Default | Notes |\n|           |      |         |       |")
                               .arg(name));
                // Now insert column header stub where the step is
                const QString hdr = indent + "| " + name + " |  |";
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                tc.insertText(hdr);
            } else if (pickBtn && clicked == static_cast<QAbstractButton*>(pickBtn)) {
                bool ok = false;
                const QString picked = QInputDialog::getItem(
                    textEdit(), tr("Pick AttributeSet"),
                    tr("Replace '%1' with:").arg(name),
                    known, 0, false, &ok);
                if (ok && !picked.isEmpty()) {
                    // Rewrite the previous line replacing the name
                    QTextCursor prev = textEdit()->textCursor();
                    prev.movePosition(QTextCursor::PreviousBlock);
                    prev.movePosition(QTextCursor::StartOfBlock);
                    prev.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                    QString newLine = prev.selectedText();
                    newLine.replace(
                        QRegularExpression(R"(:\s*)" + QRegularExpression::escape(name)
                                           + R"((\s+Vertical)?\s*$)",
                                           QRegularExpression::CaseInsensitiveOption),
                        ": " + picked + (vertical ? " Vertical" : ""));
                    prev.insertText(newLine);
                    // Then fall through to insert the header for the picked set
                    QTimer::singleShot(0, this, [this, picked, vertical, indent]() {
                        if (!m_index) return;
                        const QVector<QStringList> attrDef = m_index->attributeRows(picked);
                        if (attrDef.size() < 2) return;
                        QStringList attrNames;
                        for (int r = 1; r < attrDef.size(); ++r)
                            if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
                        if (attrNames.isEmpty()) return;
                        QString tableText;
                        if (vertical) {
                            QStringList ls;
                            for (const QString& a : attrNames) ls << (indent + "| " + a + " |  |");
                            tableText = ls.join("\n");
                        } else {
                            QString hdr = indent + "|", data = indent + "|";
                            for (const QString& a : attrNames) {
                                hdr  += " " + a + " |";
                                data += " " + QString(a.length(), ' ') + " |";
                            }
                            tableText = hdr + "\n" + data;
                        }
                        QTextCursor tc2 = textEdit()->textCursor();
                        tc2.movePosition(QTextCursor::StartOfBlock);
                        tc2.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                        tc2.insertText(tableText);
                    });
                }
            }
            return;
        }

        // ── Known AttributeSet: insert its column headers ─────────────────────
        QString tableText;

        if (syms.dataTypes.contains(name) && !syms.hasAttributeSet(name)) {
            tableText = indent + "|   |   |   |\n" + indent + "|   |   |   |";
        } else {
            // A Collection has no fields of its own — its step-table header should
            // reflect the entity it contains, not the Collection's own
            // DataType/Minimum/Maximum/Notes declaration columns.
            QString lookupName = name;
            if (syms.hasCollection(name)) {
                const QString elemType = m_index->collectionElementType(name);
                if (!elemType.isEmpty()) lookupName = elemType;
            }
            const QVector<QStringList> attrDef = m_index->attributeRows(lookupName);
            if (attrDef.size() < 2) return;

            QStringList attrNames;
            for (int r = 1; r < attrDef.size(); ++r)
                if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
            if (attrNames.isEmpty()) return;

            if (vertical) {
                QStringList lines;
                for (const QString& a : attrNames)
                    lines << (indent + "| " + a + " |  |");
                tableText = lines.join("\n");
            } else {
                QString hdr = indent + "|", data = indent + "|";
                for (const QString& a : attrNames) {
                    hdr  += " " + a + " |";
                    data += " " + QString(a.length(), ' ') + " |";
                }
                tableText = hdr + "\n" + data;
            }
        }

        if (tableText.isEmpty()) return;

        tc.movePosition(QTextCursor::StartOfBlock);
        tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        tc.insertText(tableText);
    }
}

// ---------------------------------------------------------------------------
// Proactive prompt: the user just finished typing an ad-hoc table under a
// step that either has no ": AttrSetName" at all, or names one that isn't
// actually defined — offer to create a new AttributeSet from the table's
// columns (using the step's own name directly when it already has one), or
// link the table to an existing AttributeSet, instead of requiring the
// manual "Extract as AttributeSet..." action.
// ---------------------------------------------------------------------------

void SpecTableEditor::checkAdHocTableAttributeSet()
{
    QTextCursor tc     = textEdit()->textCursor();
    QTextBlock curBlk  = tc.block();
    QTextBlock prevBlk = curBlk.previous();

    if (!prevBlk.isValid()) return;
    if (!curBlk.text().trimmed().isEmpty()) return;
    if (curBlk.next().isValid() && curBlk.next().text().trimmed().startsWith('|')) return;
    if (!prevBlk.text().trimmed().startsWith('|')) return;  // only fire when leaving a table

    // Walk back to the first row of this table
    QTextBlock firstRow = prevBlk;
    while (firstRow.previous().isValid() && firstRow.previous().text().trimmed().startsWith('|'))
        firstRow = firstRow.previous();

    // The line directly above the table should be the owning step or
    // "Examples: Name" line (inside a BusinessRule/Calculation/DataType).
    QTextBlock ownerBlk = firstRow.previous();
    if (!ownerBlk.isValid()) return;
    const QString ownerLine = ownerBlk.text();

    static QRegularExpression reBareStep(
        R"(^\s*(Given|When|Then|And|WhenThen)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reExamplesOwner(
        R"(^\s*Examples:\s*(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    const bool isStepOwner = reBareStep.match(ownerLine).hasMatch();
    auto exOwnerMatch = reExamplesOwner.match(ownerLine);
    const bool isExamplesOwner = exOwnerMatch.hasMatch();
    if (!isStepOwner && !isExamplesOwner) return;

    if (!m_index) return;
    const SpecTableSymbols& syms = m_index->projectSymbols();

    // If the owner already names a real, known AttrSet/DataType it's fully
    // linked — nothing to do. A name that's NOT yet defined is treated the
    // same as a bare step, just using that name directly instead of
    // prompting for one.
    QString existingName;
    if (isExamplesOwner) {
        existingName = exOwnerMatch.captured(1);
        const QString nameLower = existingName.toLower();
        if (nameLower == "validvalues" || nameLower == "enumerationvalues") return;
    } else {
        static QRegularExpression reColonName(
            R"(:\s*(\w+)(?:\s+(?:Vertical|CompareOnly))?\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        auto cm = reColonName.match(ownerLine);
        if (cm.hasMatch()) existingName = cm.captured(1);
    }
    if (!existingName.isEmpty()
        && (syms.hasAttributeSet(existingName) || syms.dataTypes.contains(existingName)))
        return; // already linked to a real, known AttrSet/DataType

    QString hdrLine = firstRow.text().trimmed();
    if (hdrLine.startsWith('|')) hdrLine = hdrLine.mid(1);
    if (hdrLine.endsWith('|'))   hdrLine.chop(1);
    QStringList headers = hdrLine.split('|');
    for (auto& h : headers) h = h.trimmed();
    headers.removeAll({});
    if (headers.isEmpty()) return;

    offerCreateAttributeSetFromTable(ownerBlk, existingName, headers, isStepOwner);
}

// ---------------------------------------------------------------------------
// Shared by the step and "Examples: Name" variants above (and by the
// context-menu action on an existing Examples table): given a name that
// isn't a known AttrSet/DataType and a set of column headers from an
// already-typed table, offer to create a new Attributes declaration from
// those headers, or link the table to an existing AttributeSet instead.
// ---------------------------------------------------------------------------

void SpecTableEditor::offerCreateAttributeSetFromTable(
    QTextBlock ownerLineBlock, const QString& existingName,
    const QStringList& headers, bool ownerIsStepLine)
{
    if (!m_index) return;
    const SpecTableSymbols& syms = m_index->projectSymbols();

    QStringList known;
    for (auto it = syms.attributes.begin(); it != syms.attributes.end(); ++it)
        known << it.key();
    for (auto it = syms.entities.begin(); it != syms.entities.end(); ++it)
        if (!known.contains(it.key())) known << it.key();
    known.sort(Qt::CaseInsensitive);

    const QString msg = existingName.isEmpty()
        ? tr("This table isn't linked to an Attribute/Entity/DataType.\n\n"
             "What would you like to do?")
        : tr("AttributeSet '%1' referenced here is not defined.\n\n"
             "What would you like to do?").arg(existingName);
    const QString createLabel = existingName.isEmpty()
        ? tr("Create AttributeSet...") : tr("Create '%1'").arg(existingName);

    QMessageBox box(QMessageBox::Question, tr("Ad-hoc Table"), msg, QMessageBox::NoButton, textEdit());
    QPushButton* createBtn = box.addButton(createLabel, QMessageBox::AcceptRole);
    QPushButton* pickBtn   = known.isEmpty() ? nullptr
                           : box.addButton(tr("Use Existing..."), QMessageBox::ActionRole);
    box.addButton(tr("Not Now"), QMessageBox::RejectRole);
    box.exec();

    QAbstractButton* clicked = box.clickedButton();
    if (clicked == static_cast<QAbstractButton*>(createBtn)) {
        QString name = existingName;
        if (name.isEmpty()) {
            bool ok = false;
            name = QInputDialog::getText(textEdit(), tr("Create AttributeSet"),
                tr("AttributeSet name:"), QLineEdit::Normal, {}, &ok);
            if (!ok || name.trimmed().isEmpty()) return;
        }
        name = name.trimmed();

        // New declarations are top-level, so they land after the enclosing
        // Scenario/Background/Cleanup/BusinessRule/Calculation block ends,
        // not indented inside it.
        QStringList lines;
        lines << QString();
        lines << ("Attributes " + name);
        lines << "| Attribute | DataType |";
        for (const QString& h : headers)
            lines << ("| " + h + " | String |");

        QTextBlock afterBlock = endOfEnclosingBlock(ownerLineBlock);
        QTextCursor insertPos(afterBlock);
        insertPos.movePosition(QTextCursor::EndOfBlock);
        insertPos.insertText("\n" + lines.join("\n"));

        if (existingName.isEmpty() && ownerIsStepLine) {
            QTextCursor ownerCur(ownerLineBlock);
            ownerCur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            ownerCur.insertText(ownerLineBlock.text() + " : " + name);
        }
        // else: the owner line already names <name> (or is an Examples:
        // line, which always does) — nothing to rewrite there.
    } else if (pickBtn && clicked == static_cast<QAbstractButton*>(pickBtn)) {
        bool ok = false;
        const QString picked = QInputDialog::getItem(textEdit(), tr("Use Existing AttributeSet"),
            tr("Link this table to:"), known, 0, false, &ok);
        if (!ok || picked.isEmpty()) return;

        QTextCursor ownerCur(ownerLineBlock);
        ownerCur.movePosition(QTextCursor::StartOfBlock);
        ownerCur.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString newOwnerLine = ownerLineBlock.text();
        if (!ownerIsStepLine) {
            newOwnerLine.replace(
                QRegularExpression(R"(^(\s*Examples:\s*).*$)",
                                   QRegularExpression::CaseInsensitiveOption),
                R"(\1)" + picked);
        } else if (existingName.isEmpty()) {
            newOwnerLine += " : " + picked;
        } else {
            newOwnerLine.replace(
                QRegularExpression(R"(:\s*)" + QRegularExpression::escape(existingName)
                                   + R"((\s+(?:Vertical|CompareOnly))?\s*$)",
                                   QRegularExpression::CaseInsensitiveOption),
                ": " + picked);
        }
        ownerCur.insertText(newOwnerLine);
    }
}

// ---------------------------------------------------------------------------
// Context-menu action: cursor is on an "Examples: Name" line (inside a
// BusinessRule/Calculation/DataType) where Name isn't a known AttrSet, and a
// table already exists below it — create the AttributeSet from that table's
// real headers. Unlike checkAdHocTableAttributeSet(), this doesn't depend on
// catching an Enter keypress at the right moment; it works on an already-
// open file, on demand.
// ---------------------------------------------------------------------------

void SpecTableEditor::createAttributeSetFromExamplesTable()
{
    QTextBlock ownerBlk = textEdit()->textCursor().block();

    static QRegularExpression reExamplesOwner(
        R"(^\s*Examples:\s*(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = reExamplesOwner.match(ownerBlk.text());
    if (!m.hasMatch()) return;

    const QString name = m.captured(1);
    const QString nameLower = name.toLower();
    if (nameLower == "validvalues" || nameLower == "enumerationvalues") return;

    if (!m_index) return;
    const SpecTableSymbols& syms = m_index->projectSymbols();
    if (syms.hasAttributeSet(name) || syms.dataTypes.contains(name)) return;

    QTextBlock firstRow = ownerBlk.next();
    if (!firstRow.isValid() || !firstRow.text().trimmed().startsWith('|')) return;

    QString hdrLine = firstRow.text().trimmed();
    if (hdrLine.startsWith('|')) hdrLine = hdrLine.mid(1);
    if (hdrLine.endsWith('|'))   hdrLine.chop(1);
    QStringList headers = hdrLine.split('|');
    for (auto& h : headers) h = h.trimmed();
    headers.removeAll({});
    if (headers.isEmpty()) return;

    offerCreateAttributeSetFromTable(ownerBlk, name, headers, /*ownerIsStepLine=*/false);
}

// ---------------------------------------------------------------------------
// Insert header row for the step the cursor is currently on
// (context-menu action when the step has : AttrSetName but no table below)
// ---------------------------------------------------------------------------

void SpecTableEditor::insertTableHeaderForCurrentStep()
{
    QTextCursor tc = textEdit()->textCursor();
    const QString stepLine = tc.block().text();

    static QRegularExpression reStep(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(\s+Vertical)?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reApplying(
        R"(\bapplying\s+(?:BusinessRule|Calculation)\b)",
        QRegularExpression::CaseInsensitiveOption);

    auto m = reStep.match(stepLine);
    if (!m.hasMatch() || reApplying.match(stepLine).hasMatch()) return;

    const QString name      = m.captured(1);
    const bool    vertical = !m.captured(2).trimmed().isEmpty();

    QString indent;
    for (const QChar ch : stepLine) { if (!ch.isSpace()) break; indent += ch; }

    if (!m_index) return;
    const SpecTableSymbols& syms = m_index->projectSymbols();
    if (!syms.hasAttributeSet(name)) return;

    QString tableText;
    if (syms.dataTypes.contains(name) && !syms.attributes.contains(name)
                                      && !syms.entities.contains(name)) {
        tableText = indent + "|   |   |   |\n" + indent + "|   |   |   |";
    } else {
        const QVector<QStringList> attrDef = m_index->attributeRows(name);
        if (attrDef.size() < 2) return;
        QStringList attrNames;
        for (int r = 1; r < attrDef.size(); ++r)
            if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
        if (attrNames.isEmpty()) return;

        if (vertical) {
            QStringList lines;
            for (const QString& a : attrNames)
                lines << (indent + "| " + a + " |  |");
            tableText = lines.join("\n");
        } else {
            QString hdr = indent + "|", data = indent + "|";
            for (const QString& a : attrNames) {
                hdr  += " " + a + " |";
                data += " " + QString(a.length(), ' ') + " |";
            }
            tableText = hdr + "\n" + data;
        }
    }

    if (tableText.isEmpty()) return;

    tc.movePosition(QTextCursor::EndOfBlock);
    tc.insertText("\n" + tableText);
    textEdit()->setTextCursor(tc);
}

// ---------------------------------------------------------------------------
// Edit multi-line comment — join continuation lines into a single string,
// let the user edit it, then reflow at the current viewport width.
// ---------------------------------------------------------------------------

// Toggle # comment on the current line or every line in the selection.
// If all affected lines already start with #, the # is removed; otherwise
// # is prepended to every line (mixed state → all get commented).
void SpecTableEditor::toggleLineComment()
{
    QPlainTextEdit* te  = textEdit();
    QTextCursor     cur = te->textCursor();
    QTextDocument*  doc = te->document();

    const int posStart = cur.selectionStart();
    const int posEnd   = cur.selectionEnd();

    QTextBlock firstBlock = doc->findBlock(posStart);
    QTextBlock lastBlock  = doc->findBlock(posEnd);

    // If the selection ends exactly at a block boundary don't include that block.
    if (posStart != posEnd && lastBlock.position() == posEnd)
        lastBlock = lastBlock.previous();

    // Decide: remove comments only if every line already starts with #
    bool allCommented = true;
    for (QTextBlock b = firstBlock; b.isValid() && b != lastBlock.next(); b = b.next()) {
        if (!b.text().startsWith('#')) { allCommented = false; break; }
    }

    cur.beginEditBlock();
    for (QTextBlock b = firstBlock; b.isValid() && b != lastBlock.next(); b = b.next()) {
        QTextCursor bc(b);
        bc.movePosition(QTextCursor::StartOfBlock);
        if (allCommented) {
            // Remove "# " if present, otherwise just "#"
            const QString text = b.text();
            bc.deleteChar();                          // remove '#'
            if (bc.block().text().startsWith(' '))
                bc.deleteChar();                      // remove the space
        } else {
            bc.insertText("# ");
        }
    }
    cur.endEditBlock();
}

void SpecTableEditor::editMultilineComment()
{
    QTextDocument* doc = textEdit()->document();

    static QRegularExpression reField(
        R"(^\s*(Description|Details|Constraint|Notes)\s*(.*))",
        QRegularExpression::CaseInsensitiveOption);

    // Walk back from cursor to find the field opener line
    int openerNum = textEdit()->textCursor().blockNumber();
    for (int i = openerNum; i >= 0; --i) {
        const QString t = doc->findBlockByNumber(i).text();
        if (reField.match(t).hasMatch()) { openerNum = i; break; }
        if (i < textEdit()->textCursor().blockNumber()
            && (t.isEmpty() || !t.at(0).isSpace())) return;
    }

    QTextBlock opener = doc->findBlockByNumber(openerNum);
    auto om = reField.match(opener.text());
    if (!om.hasMatch()) return;

    const QString keyword = om.captured(1);
    // Detect the opener's leading indent
    QString openerIndent;
    for (const QChar ch : opener.text()) { if (!ch.isSpace()) break; openerIndent += ch; }
    const QString contIndent = openerIndent + "  ";

    // Collect the inline text (after the keyword) and continuation lines
    QString inlineRaw = om.captured(2).trimmed();
    const bool isMultilineOpener = (inlineRaw == "\\" || inlineRaw.isEmpty());
    if (inlineRaw.endsWith(" \\")) inlineRaw.chop(2);
    else if (inlineRaw.endsWith("\\")) inlineRaw.chop(1);
    inlineRaw = inlineRaw.trimmed();

    QStringList parts;
    if (!inlineRaw.isEmpty()) parts << inlineRaw;

    int lastContNum = openerNum;
    if (isMultilineOpener) {
        for (int i = openerNum + 1; i < doc->blockCount(); ++i) {
            const QString t = doc->findBlockByNumber(i).text();
            if (t.isEmpty() || !t.at(0).isSpace()) break;
            lastContNum = i;
            QString stripped = t.trimmed();
            if (stripped.endsWith(" \\")) stripped.chop(2);
            else if (stripped.endsWith("\\")) stripped.chop(1);
            parts << stripped.trimmed();
        }
    }

    const QString fullText = parts.join(" ").simplified();

    // Dialog
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Edit %1").arg(keyword));
    dlg.resize(620, 160);
    auto* edit = new QPlainTextEdit(fullText, &dlg);
    edit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(edit);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString newText = edit->toPlainText().simplified();
    if (newText.isEmpty()) return;

    // Compute wrap width from viewport
    const int charW = qMax(1, textEdit()->fontMetrics().horizontalAdvance(QChar('M')));
    const int viewW = textEdit()->viewport()->width();
    const int maxContentCols = qMax(30, viewW / charW - int(contIndent.length()) - 2);

    // Word-wrap
    QStringList words = newText.split(' ', Qt::SkipEmptyParts);
    QStringList wrapped;
    QString line;
    for (const QString& word : words) {
        if (!line.isEmpty() && line.length() + 1 + word.length() > maxContentCols) {
            wrapped << line;
            line = word;
        } else {
            if (!line.isEmpty()) line += ' ';
            line += word;
        }
    }
    if (!line.isEmpty()) wrapped << line;

    // Build replacement lines
    QStringList newLines;
    if (wrapped.size() == 1) {
        newLines << openerIndent + keyword + " " + wrapped.first();
    } else {
        newLines << openerIndent + keyword + " \\";
        for (int i = 0; i < wrapped.size() - 1; ++i)
            newLines << contIndent + wrapped[i] + " \\";
        newLines << contIndent + wrapped.last();
    }

    QTextCursor tc(doc);
    QTextBlock endBlock = doc->findBlockByNumber(lastContNum);
    tc.setPosition(opener.position());
    tc.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
    tc.insertText(newLines.join('\n'));
}

// ---------------------------------------------------------------------------
// Phase 9 — Extract ad-hoc table as a new Attributes or Define declaration
// ---------------------------------------------------------------------------

static QStringList tableHeadersAtCursor(QPlainTextEdit* edit)
{
    static QRegularExpression reRow(R"(^\s*\|)");
    QTextBlock b = edit->textCursor().block();
    while (b.previous().isValid() && reRow.match(b.previous().text()).hasMatch())
        b = b.previous();

    QString trimmed = b.text().trimmed();
    if (trimmed.startsWith('|')) trimmed = trimmed.mid(1);
    if (trimmed.endsWith('|'))   trimmed.chop(1);
    QStringList headers = trimmed.split('|');
    for (auto& h : headers) h = h.trimmed();
    headers.removeAll({});
    return headers;
}

static QTextBlock firstTableBlock(QPlainTextEdit* edit)
{
    static QRegularExpression reRow(R"(^\s*\|)");
    QTextBlock b = edit->textCursor().block();
    while (b.previous().isValid() && reRow.match(b.previous().text()).hasMatch())
        b = b.previous();
    return b;
}

void SpecTableEditor::extractAsAttributeSet()
{
    const QStringList headers = tableHeadersAtCursor(textEdit());
    if (headers.isEmpty()) return;

    bool ok;
    const QString name = QInputDialog::getText(this, tr("Extract as AttributeSet"),
        tr("AttributeSet name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QTextBlock firstRow = firstTableBlock(textEdit());
    QString indent;
    for (const QChar ch : firstRow.text()) { if (!ch.isSpace()) break; indent += ch; }

    QStringList lines;
    lines << (indent + "Attributes " + name.trimmed());
    lines << (indent + "| Attribute | DataType |");
    for (const QString& h : headers)
        lines << (indent + "| " + h + " | String |");
    lines << QString();

    QTextCursor tc(firstRow);
    tc.movePosition(QTextCursor::StartOfBlock);
    tc.insertText(lines.join("\n") + "\n");
}

void SpecTableEditor::extractAsDefine()
{
    const QTextBlock firstRow = firstTableBlock(textEdit());
    QString indent;
    for (const QChar ch : firstRow.text()) { if (!ch.isSpace()) break; indent += ch; }

    bool ok;
    const QString name = QInputDialog::getText(this, tr("Extract as Define"),
        tr("Define name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QTextCursor tc(firstRow);
    tc.movePosition(QTextCursor::StartOfBlock);
    tc.insertText(indent + "Define " + name.trimmed() + "\n");
}

// ---------------------------------------------------------------------------
// CSV import — parse file, validate against AttributeSet, insert as pipe table
// ---------------------------------------------------------------------------

QVector<QStringList> SpecTableEditor::parseCsvFile(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    QVector<QStringList> result;

    while (!in.atEnd()) {
        const QString rawLine = in.readLine();
        QStringList row;
        QString field;
        bool inQuotes = false;

        for (int i = 0; i < rawLine.size(); ++i) {
            const QChar ch = rawLine[i];
            if (inQuotes) {
                if (ch == '"') {
                    if (i + 1 < rawLine.size() && rawLine[i + 1] == '"') {
                        field += '"'; ++i;  // escaped quote
                    } else {
                        inQuotes = false;
                    }
                } else {
                    field += ch;
                }
            } else {
                if (ch == '"') {
                    inQuotes = true;
                } else if (ch == ',') {
                    row << field.trimmed();
                    field.clear();
                } else {
                    field += ch;
                }
            }
        }
        row << field.trimmed();
        if (!(row.size() == 1 && row[0].isEmpty()))
            result << row;
    }
    return result;
}

void SpecTableEditor::importCsv()
{
    QTextCursor tc = textEdit()->textCursor();
    const QString lineText = tc.block().text();

    static QRegularExpression reStepLine(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b)",
        QRegularExpression::CaseInsensitiveOption);
    if (!reStepLine.match(lineText).hasMatch()) return;

    static QRegularExpression reStepAttr(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(\s+Vertical)?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = reStepAttr.match(lineText);
    const QString attrSetName = m.hasMatch() ? m.captured(1) : QString();

    const QString csvPath = QFileDialog::getOpenFileName(
        this, tr("Import CSV"), {}, tr("CSV Files (*.csv);;All Files (*)"));
    if (csvPath.isEmpty()) return;

    const QVector<QStringList> rows = parseCsvFile(csvPath);
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Import CSV"), tr("The CSV file is empty."));
        return;
    }

    const QStringList csvHeaders = rows.first();
    const QVector<QStringList> dataRows = rows.mid(1);

    // Case 1 — known AttributeSet: validate headers match its fields
    if (!attrSetName.isEmpty() && m_index
            && m_index->projectSymbols().hasAttributeSet(attrSetName)) {
        const QVector<QStringList> attrDef = m_index->attributeRows(attrSetName);
        QStringList expectedFields;
        for (int r = 1; r < attrDef.size(); ++r)
            if (!attrDef[r].isEmpty()) expectedFields << attrDef[r][0];

        QStringList missing, extra;
        for (const QString& h : csvHeaders)
            if (!expectedFields.contains(h, Qt::CaseInsensitive)) extra << h;
        for (const QString& f : expectedFields)
            if (!csvHeaders.contains(f, Qt::CaseInsensitive)) missing << f;

        if (!missing.isEmpty() || !extra.isEmpty()) {
            QString msg = tr("CSV headers do not match AttributeSet '%1':").arg(attrSetName);
            if (!missing.isEmpty())
                msg += tr("\n  Missing: %1").arg(missing.join(", "));
            if (!extra.isEmpty())
                msg += tr("\n  Extra: %1").arg(extra.join(", "));
            msg += tr("\n\nProceed anyway?");
            if (QMessageBox::question(this, tr("Column Mismatch"), msg,
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                return;
        }
    }

    // Case 2 — no AttributeSet: offer to create one from the CSV headers
    if (attrSetName.isEmpty() && !csvHeaders.isEmpty()) {
        const auto ans = QMessageBox::question(
            this, tr("Create AttributeSet?"),
            tr("The step has no AttributeSet. Create one from the CSV headers?\n\nHeaders: %1")
                .arg(csvHeaders.join(", ")),
            QMessageBox::Yes | QMessageBox::No);
        if (ans == QMessageBox::Yes) {
            bool ok;
            const QString name = QInputDialog::getText(
                this, tr("New AttributeSet"),
                tr("Name for the new AttributeSet:"), QLineEdit::Normal, {}, &ok);
            if (ok && !name.trimmed().isEmpty()) {
                const QString trimmedName = name.trimmed();

                // Append Attributes block at end of file
                QString block = QString("\n\nAttributes %1\n").arg(trimmedName);
                block += "| Attribute | Type | Default | Notes |\n";
                for (const QString& h : csvHeaders)
                    block += QString("| %1 | String |  |  |\n").arg(h);
                block.chop(1);
                QTextCursor endCur = textEdit()->textCursor();
                endCur.movePosition(QTextCursor::End);
                endCur.insertText(block);

                // Append ": AttrSetName" to the step line
                QTextCursor stepCur(tc.block());
                stepCur.movePosition(QTextCursor::EndOfBlock);
                stepCur.insertText(": " + trimmedName);
            }
        }
    }

    // Insert CSV as pipe table — after any existing table rows below the step
    QTextBlock stepBlock = tc.block();
    QTextBlock insertBlock = stepBlock;
    for (QTextBlock nb = stepBlock.next(); nb.isValid(); nb = nb.next()) {
        if (!nb.text().trimmed().startsWith('|')) break;
        insertBlock = nb;
    }

    // Derive indent from step line
    QString indent;
    for (const QChar ch : stepBlock.text()) { if (!ch.isSpace()) break; indent += ch; }

    // Compute column widths for neat formatting
    QVector<int> widths(csvHeaders.size(), 0);
    for (int i = 0; i < csvHeaders.size(); ++i)
        widths[i] = csvHeaders[i].length();
    for (const QStringList& row : dataRows)
        for (int i = 0; i < qMin(row.size(), csvHeaders.size()); ++i)
            widths[i] = qMax(widths[i], row[i].length());

    auto makeRow = [&](const QStringList& cells) -> QString {
        QString rowLine = indent + "|";
        for (int i = 0; i < csvHeaders.size(); ++i) {
            const QString val = (i < cells.size()) ? cells[i] : QString();
            rowLine += " " + val.leftJustified(widths[i]) + " |";
        }
        return rowLine;
    };

    QStringList tableLines;
    tableLines << makeRow(csvHeaders);
    for (const QStringList& row : dataRows)
        tableLines << makeRow(row);

    QTextCursor insertCur(insertBlock);
    insertCur.movePosition(QTextCursor::EndOfBlock);
    insertCur.insertText("\n" + tableLines.join("\n"));
    textEdit()->setTextCursor(insertCur);
}

void SpecTableEditor::refreshDynamicCompletions()
{
    if (!m_index) return;

    const SpecTableSymbols& syms = m_index->projectSymbols();
    QStringList words = m_staticKeywords;

    for (const QString& n : syms.entities.keys())      words << n;
    for (const QString& n : syms.attributes.keys())    words << n;
    for (const QString& n : syms.dataTypes.keys())     words << n;
    for (const QString& n : syms.businessRules.keys()) words << n;
    for (const QString& n : syms.calculations.keys())  words << n;
    for (const QString& n : syms.domainTerms.keys())   words << n;
    for (const QString& n : syms.defines.keys())       words << ("=" + n);

    words.removeDuplicates();
    setCompletionWords(words);

    // Dedicated list for the step-colon dropdown (Given/When/Then/Examples: <name>)
    // DataTypes are excluded — they are for the Type column, not step parameters.
    QStringList attrSets;
    for (const QString& n : syms.entities.keys())   attrSets << n;
    for (const QString& n : syms.attributes.keys()) attrSets << n;
    attrSets.sort(Qt::CaseInsensitive);
    attrSets.removeDuplicates();
    lineNumberEdit()->setAttrSetCompletionWords(attrSets);

    // Dedicated list for the Type column dropdown in Attributes/Entity tables
    static const QStringList builtInTypes = {
        "Boolean", "Character", "Date", "DateTime", "Duration",
        "Integer", "Scientific", "String", "Text", "Time", "YesNo"
    };
    QStringList typeWords = builtInTypes;
    for (const QString& n : syms.dataTypes.keys()) typeWords << n;
    typeWords.sort(Qt::CaseInsensitive);
    typeWords.removeDuplicates();
    lineNumberEdit()->setTypeCompletionWords(typeWords);
}

void SpecTableEditor::populateContextMenu(QMenu* menu)
{
    // Background / Cleanup display — always available for .spectable files
    {
        const QString fp = filePath();
        auto* bgAct = menu->addAction(tr("Display Background..."));
        connect(bgAct, &QAction::triggered, this, [fp, this] {
            auto* dlg = new BackgroundCleanupDialog(
                fp, BackgroundCleanupDialog::Mode::Background, window());
            dlg->show();
        });
        auto* clAct = menu->addAction(tr("Display Cleanup..."));
        connect(clAct, &QAction::triggered, this, [fp, this] {
            auto* dlg = new BackgroundCleanupDialog(
                fp, BackgroundCleanupDialog::Mode::Cleanup, window());
            dlg->show();
        });

        const int line = textEdit()->textCursor().block().blockNumber() + 1;
        auto* simAct = menu->addAction(tr("Simulate Scenario..."));
        connect(simAct, &QAction::triggered, this, [fp, line, this] {
            auto* dlg = new ScenarioSimulatorDialog(fp, line, window());
            dlg->show();
        });
        menu->addSeparator();
    }

    // Browse Import file (shown when cursor is on an Import "..." line)
    {
        static QRegularExpression reImport(
            "^\\s*Import\\s+\"([^\"]*)\"",
            QRegularExpression::CaseInsensitiveOption);
        const QString lineText = textEdit()->textCursor().block().text();
        auto im = reImport.match(lineText);
        if (im.hasMatch()) {
            auto* browseAct = menu->addAction(tr("Browse Import File..."));
            connect(browseAct, &QAction::triggered, this, [this] {
                // Paths in Import are resolved relative to this file's own
                // directory, so that's what the relative path must be built
                // against -- but open the dialog at the project's base
                // directory, which is what the user actually wants to browse from.
                const QString fileDir  = QFileInfo(filePath()).absolutePath();
                const QString startDir = m_projectRoot.isEmpty() ? fileDir : m_projectRoot;
                const QString picked = QFileDialog::getOpenFileName(
                    this, tr("Select Import File"), startDir,
                    tr("SpecTable Files (*.spectable);;All Files (*)"));
                if (picked.isEmpty()) return;
                const QString rel = QDir(fileDir).relativeFilePath(picked);
                QTextCursor tc = textEdit()->textCursor();
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                tc.insertText(QStringLiteral("Import \"%1\"").arg(rel));
            });
            menu->addSeparator();
        }
    }

    // Browse Insert file (shown when cursor is on an Insert "..."/'...'/<...> line)
    {
        static QRegularExpression reInsert(
            R"re(^\s*Insert\s+(?:"([^"]*)"|'([^']*)'|<([^>]*)>))re",
            QRegularExpression::CaseInsensitiveOption);
        const QString lineText = textEdit()->textCursor().block().text();
        auto insM = reInsert.match(lineText);
        if (insM.hasMatch()) {
            // Preserve whichever quote style the line already used. A
            // participating (even empty) capture has a valid start offset;
            // a non-participating alternative's is -1.
            QChar openQuote = '"', closeQuote = '"';
            if (insM.capturedStart(2) != -1)      { openQuote = closeQuote = '\''; }
            else if (insM.capturedStart(3) != -1) { openQuote = '<'; closeQuote = '>'; }

            auto* browseInsAct = menu->addAction(tr("Browse Insert File..."));
            connect(browseInsAct, &QAction::triggered, this, [this, openQuote, closeQuote] {
                const QString fileDir  = QFileInfo(filePath()).absolutePath();
                const QString startDir = m_projectRoot.isEmpty() ? fileDir : m_projectRoot;
                const QString picked = QFileDialog::getOpenFileName(
                    this, tr("Select Insert File"), startDir,
                    tr("All Files (*);;CSV Files (*.csv *.tsv);;Text Files (*.txt);;SpecTable Files (*.spectable)"));
                if (picked.isEmpty()) return;
                const QString rel = QDir(fileDir).relativeFilePath(picked);
                QTextCursor tc = textEdit()->textCursor();
                tc.movePosition(QTextCursor::StartOfBlock);
                tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                tc.insertText(QStringLiteral("Insert %1%2%3").arg(openQuote).arg(rel).arg(closeQuote));
            });
            menu->addSeparator();
        }
    }

    // Run Examples (shown when cursor is inside a BusinessRule or Calculation block)
    if (m_index) {
        static QRegularExpression reTopLevel(
            R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
            QRegularExpression::CaseInsensitiveOption);
        static QRegularExpression reBRCalc(
            R"(^\s*(BusinessRule|Calculation)\b)",
            QRegularExpression::CaseInsensitiveOption);

        const QTextBlock cur = textEdit()->textCursor().block();
        bool inBRCalc = false;
        for (QTextBlock b = cur; b.isValid(); b = b.previous()) {
            if (reBRCalc.match(b.text()).hasMatch())    { inBRCalc = true; break; }
            if (reTopLevel.match(b.text()).hasMatch())  break;
        }

        if (inBRCalc) {
            const QString fp   = filePath();
            const int     line = cur.blockNumber() + 1;
            const SpecTableIndex* idx = m_index;
            auto* runAct = menu->addAction(tr("Run Examples..."));
            connect(runAct, &QAction::triggered, this, [fp, line, idx, this] {
                auto* dlg = new ExampleRunnerDialog(fp, line, idx, window());
                dlg->show();
            });
            menu->addSeparator();
        }
    }

    // Create AttributeSet from Examples table (shown when cursor is on
    // "Examples: Name" and Name isn't a known AttrSet/DataType)
    {
        static QRegularExpression reExamplesOwner(
            R"(^\s*Examples:\s*(\w+)\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        const QTextBlock curBlock = textEdit()->textCursor().block();
        auto em = reExamplesOwner.match(curBlock.text());
        if (em.hasMatch() && m_index) {
            const QString exName = em.captured(1);
            const QString exNameLower = exName.toLower();
            const SpecTableSymbols& syms = m_index->projectSymbols();
            const bool isBuiltin = (exNameLower == "validvalues" || exNameLower == "enumerationvalues");
            const bool hasTable = curBlock.next().isValid()
                                  && curBlock.next().text().trimmed().startsWith('|');
            if (!isBuiltin && hasTable
                && !syms.hasAttributeSet(exName) && !syms.dataTypes.contains(exName)) {
                auto* createAct = menu->addAction(tr("Create AttributeSet '%1' from this table...").arg(exName));
                connect(createAct, &QAction::triggered,
                        this, &SpecTableEditor::createAttributeSetFromExamplesTable);
                menu->addSeparator();
            }
        }
    }

    if (!m_index) return;

    QTextCursor tc = textEdit()->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    const QString word = tc.selectedText().trimmed();
    if (word.isEmpty()) return;

    const SpecTableSymbols& syms = m_index->projectSymbols();
    const SymbolLocation    loc  = syms.locationFor(word);
    const bool isKnown   = !loc.filePath.isEmpty();
    const bool isAttrSet = syms.hasAttributeSet(word);

    if (!isKnown && !isAttrSet) {
        // Suggest creating an AttributeSet when the word is used as one in the current step
        static QRegularExpression reStepAttr(
            R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(\s+Vertical)?\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        const QString lineText = textEdit()->textCursor().block().text();
        auto sm = reStepAttr.match(lineText);
        if (sm.hasMatch() && sm.captured(1).compare(word, Qt::CaseInsensitive) == 0) {
            menu->addSeparator();
            auto* createAct = menu->addAction(tr("Create Attributes '%1'").arg(word));
            connect(createAct, &QAction::triggered, this, [this, word] {
                // Collect column headers from the table immediately below the step line
                QStringList attrNames;
                {
                    static QRegularExpression reRow(R"(^\s*\|)");
                    const QTextBlock stepBlock = textEdit()->textCursor().block();
                    const bool vertical = stepBlock.text().contains(
                        QRegularExpression(R"(\bVertical\b)", QRegularExpression::CaseInsensitiveOption));
                    bool foundFirstRow = false;
                    for (QTextBlock b = stepBlock.next(); b.isValid(); b = b.next()) {
                        if (b.text().trimmed().isEmpty()) continue;
                        if (!reRow.match(b.text()).hasMatch()) break;
                        QStringList parts = b.text().split('|');
                        QStringList cells;
                        for (int i = 1; i < parts.size() - 1; ++i) {
                            const QString c = parts[i].trimmed();
                            if (!c.isEmpty()) cells << c;
                        }
                        if (vertical) {
                            // Each row: | AttrName | Value | — take col 0
                            if (!cells.isEmpty()) attrNames << cells[0];
                        } else {
                            // First row = column headers
                            attrNames = cells;
                            foundFirstRow = true;
                        }
                        if (!vertical && foundFirstRow) break;
                    }
                }

                // Build block
                QString block = QString("\n\nAttributes %1\n").arg(word);
                block += "| Attribute | Type | Default | Notes |\n";
                if (attrNames.isEmpty()) {
                    block += "|           |      |         |       |";
                } else {
                    for (const QString& attr : attrNames)
                        block += QString("| %1 |      |         |       |\n").arg(attr);
                    block.chop(1);
                }

                QTextCursor end = textEdit()->textCursor();
                end.movePosition(QTextCursor::End);
                const int insertPos = end.position() + 3; // after "\n\nAttributes "
                end.insertText(block);

                // Move cursor to the first attribute data cell
                QTextCursor nav = textEdit()->textCursor();
                nav.movePosition(QTextCursor::End);
                // Walk back to the "Attributes <word>" line we just inserted
                QTextDocument* doc = textEdit()->document();
                QTextCursor found = doc->find(
                    QRegularExpression(R"(^\s*Attributes\s+)" + QRegularExpression::escape(word) + R"(\s*$)",
                                       QRegularExpression::CaseInsensitiveOption),
                    insertPos - 3,
                    QTextDocument::FindBackward);
                if (!found.isNull()) {
                    // Move to the line after the header row (first data row)
                    QTextBlock attrBlock = found.block().next().next(); // skip header row
                    if (attrBlock.isValid()) {
                        QTextCursor tc(attrBlock);
                        tc.movePosition(QTextCursor::StartOfBlock);
                        // Position inside the first cell (after "| ")
                        tc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
                        textEdit()->setTextCursor(tc);
                        textEdit()->ensureCursorVisible();
                    }
                }
            });
        }
        return;
    }

    menu->addSeparator();

    if (isAttrSet) {
        auto* act = menu->addAction(tr("Show Attributes: %1").arg(word));
        connect(act, &QAction::triggered, this, [this, word] {
            const QVector<QStringList> rows = m_index->attributeRows(word);
            auto* dlg = new AttributeTableDialog(word, rows, this);
            dlg->show();
        });
    }

    if (isKnown && syms.defines.contains(word)) {
        auto* showAct = menu->addAction(tr("Show Define: %1").arg(word));
        connect(showAct, &QAction::triggered, this, [this, word] {
            const auto info = m_index->defineInfo(word);
            if (!info.first.isEmpty()) {
                // Scalar define — show in a two-row table
                QVector<QStringList> rows;
                rows << QStringList{"Name", "Value"};
                rows << QStringList{word, info.first};
                auto* dlg = new AttributeTableDialog(word, rows, this);
                dlg->show();
            } else if (!info.second.isEmpty()) {
                // Table define
                auto* dlg = new AttributeTableDialog(word, info.second, this);
                dlg->show();
            }
        });
    }

    if (isKnown) {
        auto* defAct = menu->addAction(tr("Go to Definition: %1").arg(word));
        connect(defAct, &QAction::triggered, this, [this, loc] {
            emit goToDefinitionRequested(loc.filePath, loc.line);
        });

        auto* refAct = menu->addAction(tr("Find All References: %1").arg(word));
        connect(refAct, &QAction::triggered, this, [this, word] {
            emit findReferencesRequested(word);
        });

        auto* renameAct = menu->addAction(tr("Rename Symbol: %1...").arg(word));
        connect(renameAct, &QAction::triggered, this, [this, word] {
            emit renameSymbolRequested(word);
        });
    }

    // Import CSV / Find Step Usages / Insert Table Header (shown when cursor is on a step line)
    {
        static QRegularExpression reStep(
            R"(^\s*(Given|When|Then|And|WhenThen)\s+(.+?)(?:\s*:.*)?$)",
            QRegularExpression::CaseInsensitiveOption);
        static QRegularExpression reStepAttr(
            R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(\s+Vertical)?\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        const QTextBlock curBlock = textEdit()->textCursor().block();
        const QString lineText = curBlock.text();
        auto sm = reStep.match(lineText);
        if (sm.hasMatch()) {
            QString kw   = sm.captured(1);
            QString text = sm.captured(2).trimmed();
            menu->addSeparator();
            // Insert Table Header — shown when step has : AttrSet but no table below
            const bool hasAttr = reStepAttr.match(lineText).hasMatch();
            const bool hasTable = curBlock.next().isValid()
                                  && curBlock.next().text().trimmed().startsWith('|');
            if (hasAttr && !hasTable) {
                auto* insHdrAct = menu->addAction(tr("Insert Table Header"));
                connect(insHdrAct, &QAction::triggered,
                        this, &SpecTableEditor::insertTableHeaderForCurrentStep);
            }
            auto* csvAct = menu->addAction(tr("Import CSV..."));
            connect(csvAct, &QAction::triggered, this, &SpecTableEditor::importCsv);
            auto* stepAct = menu->addAction(tr("Find Step Usages: %1 %2...").arg(kw, text));
            connect(stepAct, &QAction::triggered, this, [this, kw, text] {
                emit findStepUsagesRequested(kw, text);
            });
        }
    }

    // Edit Comment (shown when cursor is on a Description/Details/Constraint line or continuation)
    {
        static QRegularExpression reField(
            R"(^\s*(Description|Details|Constraint|Notes)\b)",
            QRegularExpression::CaseInsensitiveOption);
        const QTextBlock cur = textEdit()->textCursor().block();
        const bool onField = reField.match(cur.text()).hasMatch();
        const bool onCont  = !cur.text().isEmpty() && cur.text().at(0).isSpace()
                             && cur.previous().isValid()
                             && (reField.match(cur.previous().text()).hasMatch()
                                 || (!cur.previous().text().isEmpty()
                                     && cur.previous().text().at(0).isSpace()));
        if (onField || onCont) {
            menu->addSeparator();
            auto* editCommentAct = menu->addAction(tr("Edit Comment..."));
            connect(editCommentAct, &QAction::triggered,
                    this, &SpecTableEditor::editMultilineComment);
        }
    }

    // Table row editing (shown when cursor is on a pipe row)
    if (textEdit()->textCursor().block().text().trimmed().startsWith('|')) {
        menu->addSeparator();
        auto* insRow = menu->addAction(tr("Insert Row Below"));
        connect(insRow, &QAction::triggered, this, &SpecTableEditor::insertTableRow);

        auto* delRow = menu->addAction(tr("Delete Row"));
        connect(delRow, &QAction::triggered, this, &SpecTableEditor::deleteTableRow);

        auto* transpose = menu->addAction(tr("Transpose Table"));
        connect(transpose, &QAction::triggered, this, &SpecTableEditor::transposeTable);

        menu->addSeparator();
        auto* extAttr = menu->addAction(tr("Extract as AttributeSet..."));
        connect(extAttr, &QAction::triggered, this, &SpecTableEditor::extractAsAttributeSet);

        auto* extDef = menu->addAction(tr("Extract as Define..."));
        connect(extDef, &QAction::triggered, this, &SpecTableEditor::extractAsDefine);
    }
}
