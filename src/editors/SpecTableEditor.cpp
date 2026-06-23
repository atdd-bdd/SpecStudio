#include "SpecTableEditor.h"
#include "syntax/SpecTableHighlighter.h"
#include "../analyzer/SpecTableIndex.h"
#include "../ui/dialogs/AttributeTableDialog.h"
#include "../ui/dialogs/BackgroundCleanupDialog.h"
#include "../ui/dialogs/ExampleRunnerDialog.h"
#include "../ui/dialogs/ScenarioSimulatorDialog.h"

#include <QApplication>
#include <QDialog>
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
        "Specification ", "Entity ", "DomainTerm ", "DataType ", "Attributes ",
        "BusinessRule ", "Calculation ", "Import ", "Insert ", "Define ",
        "Scenario ", "ScenarioGroup ", "Background ", "Cleanup ",
        "Description ", "Details ", "Constraint ",
        "Examples: EnumerationValues", "Examples: ValidValues", "Examples: ",
        "Transposed",
        "Given ", "When ", "Then ", "And ", "But ",
        "applying BusinessRule ", "applying Calculation ",
        // Built-in DataTypes
        "Character", "String", "Text", "Integer", "Float",
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
            R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
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

        if (!type.isEmpty())
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
            "| Value1 | Value2 |",
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

    // ── Case 2: Step line with : AttrSetName ──────────────────────────────────
    {
        static QRegularExpression reStep(
            R"(^\s*(?:Given|When|Then|And|But)\b.+:\s*(\w+)(\s+Transposed)?\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        static QRegularExpression reApplying(
            R"(\bapplying\s+(?:BusinessRule|Calculation)\b)",
            QRegularExpression::CaseInsensitiveOption);

        auto m = reStep.match(prevLine);
        if (!m.hasMatch() || reApplying.match(prevLine).hasMatch()) return;

        const QString name       = m.captured(1);
        const bool    transposed = !m.captured(2).trimmed().isEmpty();

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
                                           + R"((\s+Transposed)?\s*$)",
                                           QRegularExpression::CaseInsensitiveOption),
                        ": " + picked + (transposed ? " Transposed" : ""));
                    prev.insertText(newLine);
                    // Then fall through to insert the header for the picked set
                    QTimer::singleShot(0, this, [this, picked, transposed, indent]() {
                        if (!m_index) return;
                        const QVector<QStringList> attrDef = m_index->attributeRows(picked);
                        if (attrDef.size() < 2) return;
                        QStringList attrNames;
                        for (int r = 1; r < attrDef.size(); ++r)
                            if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
                        if (attrNames.isEmpty()) return;
                        QString tableText;
                        if (transposed) {
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
            const QVector<QStringList> attrDef = m_index->attributeRows(name);
            if (attrDef.size() < 2) return;

            QStringList attrNames;
            for (int r = 1; r < attrDef.size(); ++r)
                if (!attrDef[r].isEmpty()) attrNames << attrDef[r][0];
            if (attrNames.isEmpty()) return;

            if (transposed) {
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
// Edit multi-line comment — join continuation lines into a single string,
// let the user edit it, then reflow at the current viewport width.
// ---------------------------------------------------------------------------

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

    if (!m_index) return;

    QTextCursor tc = textEdit()->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    const QString word = tc.selectedText().trimmed();
    if (word.isEmpty()) return;

    const SpecTableSymbols& syms = m_index->projectSymbols();
    const SymbolLocation    loc  = syms.locationFor(word);
    const bool isKnown   = !loc.filePath.isEmpty();
    const bool isAttrSet = syms.hasAttributeSet(word);

    if (!isKnown && !isAttrSet) return;

    menu->addSeparator();

    if (isAttrSet) {
        auto* act = menu->addAction(tr("Show Attributes: %1").arg(word));
        connect(act, &QAction::triggered, this, [this, word] {
            const QVector<QStringList> rows = m_index->attributeRows(word);
            auto* dlg = new AttributeTableDialog(word, rows, this);
            dlg->show();
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

    // Find Step Usages (shown when cursor is on a Given/When/Then/And/But line)
    {
        static QRegularExpression reStep(
            R"(^\s*(Given|When|Then|And|But)\s+(.+?)(?:\s*:.*)?$)",
            QRegularExpression::CaseInsensitiveOption);
        const QString lineText = textEdit()->textCursor().block().text();
        auto sm = reStep.match(lineText);
        if (sm.hasMatch()) {
            QString kw   = sm.captured(1);
            QString text = sm.captured(2).trimmed();
            // Normalise And/But to the last real keyword (best effort: just keep as-is for search)
            menu->addSeparator();
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
