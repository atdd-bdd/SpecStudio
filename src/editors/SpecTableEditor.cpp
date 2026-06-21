#include "SpecTableEditor.h"
#include "syntax/SpecTableHighlighter.h"
#include "../analyzer/SpecTableIndex.h"
#include "../ui/dialogs/AttributeTableDialog.h"

#include <QApplication>
#include <QFileInfo>
#include <QHelpEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
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
    return PlainTextEditor::save();
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
    if (!m_index) return;

    QTextCursor tc     = textEdit()->textCursor();
    QTextBlock curBlk  = tc.block();
    QTextBlock prevBlk = curBlk.previous();

    if (!prevBlk.isValid()) return;
    if (!curBlk.text().trimmed().isEmpty()) return; // cursor not on a blank line
    // Don't insert if a table row already follows
    if (curBlk.next().isValid() && curBlk.next().text().trimmed().startsWith('|')) return;

    static QRegularExpression reStep(
        R"(^\s*(?:Given|When|Then|And|But)\b.+:\s*(\w+)(\s+Transposed)?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reApplying(
        R"(\bapplying\s+(?:BusinessRule|Calculation)\b)",
        QRegularExpression::CaseInsensitiveOption);

    const QString prevLine = prevBlk.text();
    auto m = reStep.match(prevLine);
    if (!m.hasMatch()) return;
    if (reApplying.match(prevLine).hasMatch()) return;

    const QString name       = m.captured(1);
    const bool    transposed = !m.captured(2).trimmed().isEmpty();
    const SpecTableSymbols& syms = m_index->projectSymbols();
    if (!syms.hasAttributeSet(name) && !syms.dataTypes.contains(name)) return;

    QString indent;
    for (const QChar ch : prevLine) { if (!ch.isSpace()) break; indent += ch; }

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
