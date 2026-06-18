#include "SpecTableEditor.h"
#include "syntax/SpecTableHighlighter.h"
#include "../ui/dialogs/GridDialog.h"
#include "../ui/dialogs/StringDialog.h"

#include <QRegularExpression>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QVector>
#include <QStringList>

SpecTableEditor::SpecTableEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new SpecTableHighlighter(textEdit()->document()));

    setCompletionWords({
        "Entity ", "DomainTerm ", "DataType ", "Attributes ", "BusinessRule ",
        "Calculation ", "Constraint ", "Import ", "Insert ",
        "Given ", "When ", "Then ", "And ", "But ",
        "applying BusinessRule ", "applying Calculation ",
    });

    lineNumberEdit()->setFoldPattern(
        QRegularExpression(
            R"(^\s*(Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Constraint)\b)",
            QRegularExpression::CaseInsensitiveOption));
}

// ---------------------------------------------------------------------------
// Edit Table — opens GridDialog pre-populated from the pipe table at cursor
// ---------------------------------------------------------------------------

void SpecTableEditor::editTable()
{
    auto*          te       = textEdit();
    QTextDocument* doc      = te->document();
    const int      curBlock = te->textCursor().blockNumber();

    static QRegularExpression reRow(R"(^\s*\|)");

    // Check that cursor is on a table row
    const bool onRow = reRow.match(doc->findBlockByNumber(curBlock).text()).hasMatch();

    int startBlock = curBlock;
    int endBlock   = curBlock;

    if (onRow) {
        // Expand selection to cover all contiguous pipe rows
        while (startBlock > 0 &&
               reRow.match(doc->findBlockByNumber(startBlock - 1).text()).hasMatch())
            --startBlock;
        while (endBlock < doc->blockCount() - 1 &&
               reRow.match(doc->findBlockByNumber(endBlock + 1).text()).hasMatch())
            ++endBlock;
    }

    // Parse rows into grid data
    QVector<QStringList> tableData;
    if (onRow) {
        for (int i = startBlock; i <= endBlock; ++i) {
            const QString line  = doc->findBlockByNumber(i).text();
            const QStringList parts = line.split('|');
            QStringList cells;
            // Skip first and last (empty due to leading/trailing |)
            for (int p = 1; p < parts.size() - 1; ++p)
                cells << parts[p].trimmed();
            if (!cells.isEmpty())
                tableData << cells;
        }
    }

    GridDialog dlg(tableData, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QVector<QStringList> result = dlg.tableData();
    if (result.isEmpty()) return;

    // Compute column widths for aligned output
    int numCols = 0;
    for (const auto& row : result)
        numCols = qMax(numCols, row.size());

    QVector<int> widths(numCols, 0);
    for (const auto& row : result)
        for (int c = 0; c < row.size(); ++c)
            widths[c] = qMax(widths[c], row[c].length());

    // Detect indent from the first table row (or cursor line)
    const QString refText = doc->findBlockByNumber(startBlock).text();
    QString indent;
    for (const QChar ch : refText) {
        if (ch == ' ' || ch == '\t') indent += ch;
        else break;
    }

    // Build formatted pipe lines
    QStringList newLines;
    for (const auto& row : result) {
        QString line = indent + "|";
        for (int c = 0; c < numCols; ++c) {
            const QString cell = (c < row.size()) ? row[c] : "";
            line += " " + cell.leftJustified(widths[c]) + " |";
        }
        newLines << line;
    }

    // Replace or insert
    QTextCursor tc(doc);
    if (onRow) {
        QTextBlock startB = doc->findBlockByNumber(startBlock);
        QTextBlock endB   = doc->findBlockByNumber(endBlock);
        tc.setPosition(startB.position());
        tc.setPosition(endB.position() + endB.length() - 1, QTextCursor::KeepAnchor);
        tc.insertText(newLines.join("\n"));
    } else {
        // Not on a table row — insert after current line
        QTextBlock b = doc->findBlockByNumber(curBlock);
        tc.setPosition(b.position() + b.length() - 1);
        tc.insertText("\n" + newLines.join("\n"));
    }
}

// ---------------------------------------------------------------------------
// Edit String — opens StringDialog pre-populated from the """ block at cursor
// ---------------------------------------------------------------------------

void SpecTableEditor::editString()
{
    auto*          te       = textEdit();
    QTextDocument* doc      = te->document();
    const int      curBlock = te->textCursor().blockNumber();

    static QRegularExpression reTriple(R"(^\s*\"\"\")");

    // Search backward for opening """
    int openLine = -1;
    for (int i = curBlock; i >= 0; --i) {
        if (reTriple.match(doc->findBlockByNumber(i).text()).hasMatch()) {
            openLine = i;
            break;
        }
    }

    // Search forward for closing """ (starting after the opening line)
    int closeLine = -1;
    if (openLine >= 0) {
        for (int i = openLine + 1; i < doc->blockCount(); ++i) {
            if (reTriple.match(doc->findBlockByNumber(i).text()).hasMatch()) {
                closeLine = i;
                break;
            }
        }
    }

    const bool inDocstring = (openLine >= 0 && closeLine > openLine &&
                              curBlock >= openLine && curBlock <= closeLine);

    QString existing;
    if (inDocstring) {
        QStringList lines;
        for (int i = openLine + 1; i < closeLine; ++i)
            lines << doc->findBlockByNumber(i).text();
        existing = lines.join("\n");
    }

    StringDialog dlg(existing, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString newContent = dlg.content();

    QTextCursor tc(doc);
    if (inDocstring) {
        if (closeLine == openLine + 1) {
            // No content lines between markers — insert after opening """
            QTextBlock ob = doc->findBlockByNumber(openLine);
            tc.setPosition(ob.position() + ob.length() - 1);
            if (!newContent.isEmpty())
                tc.insertText("\n" + newContent);
        } else {
            QTextBlock startB = doc->findBlockByNumber(openLine + 1);
            QTextBlock endB   = doc->findBlockByNumber(closeLine - 1);
            tc.setPosition(startB.position());
            tc.setPosition(endB.position() + endB.length() - 1, QTextCursor::KeepAnchor);
            tc.insertText(newContent);
        }
    } else {
        // Not inside a docstring — insert new """ block after cursor line
        QTextBlock b = doc->findBlockByNumber(curBlock);
        QString indent;
        for (const QChar ch : b.text()) {
            if (ch == ' ' || ch == '\t') indent += ch;
            else break;
        }
        tc.setPosition(b.position() + b.length() - 1);
        tc.insertText("\n" + indent + "\"\"\"\n" + newContent + "\n" + indent + "\"\"\"");
    }
}
