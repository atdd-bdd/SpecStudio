#include "PlainTextEditor.h"
#include "LineNumberEdit.h"
#include "../ui/dialogs/GridDialog.h"
#include "../ui/dialogs/StringDialog.h"

#include <QContextMenuEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVector>

PlainTextEditor::PlainTextEditor(const QString& filePath, QWidget* parent)
    : BaseEditor(filePath, parent)
{
    m_edit = new LineNumberEdit(this);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);

    QFont font("Courier New", 10);
    font.setFixedPitch(true);
    m_edit->setFont(font);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_edit);
    setLayout(layout);

    connect(m_edit, &QPlainTextEdit::modificationChanged,
            this,   [this](bool modified) { setDirty(modified); });

    m_edit->installEventFilter(this);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &PlainTextEditor::onFileChangedOnDisk);

    load(filePath);
}

void PlainTextEditor::load(const QString& path)
{
    // Update watcher to track the new path
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (QFile::exists(path))
        m_watcher->addPath(path);

    setFilePath(path);

    m_edit->clearErrorMarks();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_edit->setPlainText(QString());
        setDirty(false);
        return;
    }

    QTextStream in(&file);
    m_edit->setPlainText(in.readAll());
    m_edit->document()->setModified(false);
    setDirty(false);
}

void PlainTextEditor::onFileChangedOnDisk(const QString& path)
{
    // Re-add the path — some editors replace the file on save (Qt removes it from watcher)
    if (QFile::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);

    auto btn = QMessageBox::question(this, tr("File Changed"),
        tr("'%1' was modified outside the editor. Reload?")
            .arg(QFileInfo(path).fileName()),
        QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes)
        load(path);
}

bool PlainTextEditor::save()
{
    const QString path = filePath();

    // Unwatch before writing so our own save doesn't trigger the external-change prompt
    m_watcher->removePath(path);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_watcher->addPath(path);
        QMessageBox::critical(this, tr("Save Failed"),
            tr("Cannot write '%1': %2").arg(path, file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out << m_edit->toPlainText();
    file.close();

    m_watcher->addPath(path);
    m_edit->document()->setModified(false);
    setDirty(false);
    return true;
}

// ---------------------------------------------------------------------------
// Context menu — intercept right-click on the text area
// ---------------------------------------------------------------------------

bool PlainTextEditor::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_edit && event->type() == QEvent::ContextMenu) {
        auto* ce = static_cast<QContextMenuEvent*>(event);
        showEditorContextMenu(ce->globalPos());
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void PlainTextEditor::showEditorContextMenu(const QPoint& globalPos)
{
    QMenu* menu = m_edit->createStandardContextMenu();
    menu->addSeparator();

    const bool inTable  = isInTableContext();
    const bool inString = isInStringContext();

    auto* tableAct  = menu->addAction(inTable  ? tr("Edit Table...")  : tr("Add Table..."));
    auto* stringAct = menu->addAction(inString ? tr("Edit String...") : tr("Add String..."));

    connect(tableAct,  &QAction::triggered, this, &PlainTextEditor::editTable);
    connect(stringAct, &QAction::triggered, this, &PlainTextEditor::editString);

    menu->exec(globalPos);
    delete menu;
}

bool PlainTextEditor::isInTableContext() const
{
    static QRegularExpression reRow(R"(^\s*\|)");
    QTextDocument* doc = m_edit->document();
    const int curBlock = m_edit->textCursor().blockNumber();

    if (reRow.match(doc->findBlockByNumber(curBlock).text()).hasMatch())
        return true;

    // Check the next few non-blank lines
    for (int i = curBlock + 1; i < qMin(curBlock + 4, doc->blockCount()); ++i) {
        const QString text = doc->findBlockByNumber(i).text().trimmed();
        if (text.isEmpty()) continue;
        return text.startsWith('|');
    }
    return false;
}

bool PlainTextEditor::isInStringContext() const
{
    static QRegularExpression reTriple(R"(^\s*\"\"\")");
    QTextDocument* doc = m_edit->document();
    const int curBlock = m_edit->textCursor().blockNumber();

    int openLine = -1;
    for (int i = curBlock; i >= 0; --i) {
        if (reTriple.match(doc->findBlockByNumber(i).text()).hasMatch()) {
            openLine = i;
            break;
        }
    }
    if (openLine < 0) return false;

    for (int i = openLine + 1; i < doc->blockCount(); ++i) {
        if (reTriple.match(doc->findBlockByNumber(i).text()).hasMatch())
            return (curBlock >= openLine && curBlock <= i);
    }
    return false;
}

// ---------------------------------------------------------------------------
// Edit Table — opens GridDialog pre-populated from the pipe table at cursor
// ---------------------------------------------------------------------------

void PlainTextEditor::editTable()
{
    QTextDocument* doc      = m_edit->document();
    const int      curBlock = m_edit->textCursor().blockNumber();

    static QRegularExpression reRow(R"(^\s*\|)");

    const bool onRow = reRow.match(doc->findBlockByNumber(curBlock).text()).hasMatch();

    int startBlock = curBlock;
    int endBlock   = curBlock;

    if (onRow) {
        while (startBlock > 0 &&
               reRow.match(doc->findBlockByNumber(startBlock - 1).text()).hasMatch())
            --startBlock;
        while (endBlock < doc->blockCount() - 1 &&
               reRow.match(doc->findBlockByNumber(endBlock + 1).text()).hasMatch())
            ++endBlock;
    } else {
        // Not on a row — find next contiguous table block (within a few lines)
        for (int i = curBlock + 1; i < qMin(curBlock + 4, doc->blockCount()); ++i) {
            const QString text = doc->findBlockByNumber(i).text().trimmed();
            if (text.isEmpty()) continue;
            if (text.startsWith('|')) {
                startBlock = i;
                endBlock   = i;
                while (endBlock < doc->blockCount() - 1 &&
                       reRow.match(doc->findBlockByNumber(endBlock + 1).text()).hasMatch())
                    ++endBlock;
            }
            break;
        }
    }

    // Parse rows
    QVector<QStringList> tableData;
    if (startBlock <= endBlock &&
        reRow.match(doc->findBlockByNumber(startBlock).text()).hasMatch())
    {
        for (int i = startBlock; i <= endBlock; ++i) {
            const QStringList parts = doc->findBlockByNumber(i).text().split('|');
            QStringList cells;
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

    // Compute column widths
    int numCols = 0;
    for (const auto& row : result) numCols = qMax(numCols, row.size());
    QVector<int> widths(numCols, 0);
    for (const auto& row : result)
        for (int c = 0; c < row.size(); ++c)
            widths[c] = qMax(widths[c], row[c].length());

    // Detect indent from reference row
    const QString refText = doc->findBlockByNumber(startBlock).text();
    QString indent;
    for (const QChar ch : refText) {
        if (ch == ' ' || ch == '\t') indent += ch;
        else break;
    }

    QStringList newLines;
    for (const auto& row : result) {
        QString line = indent + "|";
        for (int c = 0; c < numCols; ++c) {
            const QString cell = (c < row.size()) ? row[c] : "";
            line += " " + cell.leftJustified(widths[c]) + " |";
        }
        newLines << line;
    }

    QTextCursor tc(doc);
    const bool hasExistingTable =
        reRow.match(doc->findBlockByNumber(startBlock).text()).hasMatch();

    if (hasExistingTable) {
        QTextBlock startB = doc->findBlockByNumber(startBlock);
        QTextBlock endB   = doc->findBlockByNumber(endBlock);
        tc.setPosition(startB.position());
        tc.setPosition(endB.position() + endB.length() - 1, QTextCursor::KeepAnchor);
        tc.insertText(newLines.join("\n"));
    } else {
        // Insert after current line
        QTextBlock b = doc->findBlockByNumber(curBlock);
        tc.setPosition(b.position() + b.length() - 1);
        tc.insertText("\n" + newLines.join("\n"));
    }
}

// ---------------------------------------------------------------------------
// Edit String — opens StringDialog pre-populated from the """ block at cursor
// ---------------------------------------------------------------------------

void PlainTextEditor::editString()
{
    QTextDocument* doc      = m_edit->document();
    const int      curBlock = m_edit->textCursor().blockNumber();

    static QRegularExpression reTriple(R"(^\s*\"\"\")");

    int openLine = -1;
    for (int i = curBlock; i >= 0; --i) {
        if (reTriple.match(doc->findBlockByNumber(i).text()).hasMatch()) {
            openLine = i;
            break;
        }
    }

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

void PlainTextEditor::cut()       { m_edit->cut(); }
void PlainTextEditor::copy()      { m_edit->copy(); }
void PlainTextEditor::paste()     { m_edit->paste(); }
void PlainTextEditor::undo()      { m_edit->undo(); }
void PlainTextEditor::redo()      { m_edit->redo(); }
void PlainTextEditor::selectAll() { m_edit->selectAll(); }

void PlainTextEditor::goToLine(int n)
{
    const QTextBlock block = m_edit->document()->findBlockByLineNumber(qMax(0, n - 1));
    if (!block.isValid()) return;
    QTextCursor c(block);
    m_edit->setTextCursor(c);
    m_edit->ensureCursorVisible();
}

bool PlainTextEditor::findNext(const QString& text, bool caseSensitive, bool wrapAround, bool useRegex)
{
    auto doFind = [&](QTextDocument::FindFlags flags) -> bool {
        if (useRegex) {
            QRegularExpression re(text, caseSensitive
                ? QRegularExpression::NoPatternOption
                : QRegularExpression::CaseInsensitiveOption);
            return re.isValid() && m_edit->find(re, flags);
        }
        if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
        return m_edit->find(text, flags);
    };

    bool found = doFind({});
    if (!found && wrapAround) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(QTextCursor::Start);
        m_edit->setTextCursor(c);
        found = doFind({});
    }
    return found;
}

bool PlainTextEditor::findPrev(const QString& text, bool caseSensitive, bool wrapAround, bool useRegex)
{
    auto doFind = [&](QTextDocument::FindFlags flags) -> bool {
        flags |= QTextDocument::FindBackward;
        if (useRegex) {
            QRegularExpression re(text, caseSensitive
                ? QRegularExpression::NoPatternOption
                : QRegularExpression::CaseInsensitiveOption);
            return re.isValid() && m_edit->find(re, flags);
        }
        if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
        return m_edit->find(text, flags);
    };

    bool found = doFind({});
    if (!found && wrapAround) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(QTextCursor::End);
        m_edit->setTextCursor(c);
        found = doFind({});
    }
    return found;
}

bool PlainTextEditor::replaceCurrent(const QString& replacement)
{
    QTextCursor c = m_edit->textCursor();
    if (!c.hasSelection()) return false;
    c.insertText(replacement);
    return true;
}

int PlainTextEditor::replaceAll(const QString& findText, const QString& replacement, bool caseSensitive, bool useRegex)
{
    QList<QTextCursor> hits;
    QTextCursor c(m_edit->document());

    if (useRegex) {
        QRegularExpression re(findText, caseSensitive
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) return 0;
        while (true) {
            c = m_edit->document()->find(re, c);
            if (c.isNull()) break;
            hits.append(c);
        }
    } else {
        QTextDocument::FindFlags flags;
        if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
        while (true) {
            c = m_edit->document()->find(findText, c, flags);
            if (c.isNull()) break;
            hits.append(c);
        }
    }

    if (hits.isEmpty()) return 0;

    QTextCursor undo = hits.first();
    undo.beginEditBlock();
    for (int i = hits.size() - 1; i >= 0; --i)
        hits[i].insertText(replacement);
    undo.endEditBlock();
    return hits.size();
}

void PlainTextEditor::formatTable()
{
    QTextCursor c = m_edit->textCursor();
    QTextBlock cur = c.block();

    auto isTableRow = [](const QTextBlock& b) {
        return b.isValid() && b.text().trimmed().startsWith('|');
    };
    if (!isTableRow(cur)) return;

    // Expand to the full contiguous table block
    QTextBlock first = cur, last = cur;
    while (isTableRow(first.previous())) first = first.previous();
    while (isTableRow(last.next()))      last  = last.next();

    // Parse each row into trimmed cells
    auto parseRow = [](const QString& line) -> QStringList {
        QString t = line.trimmed();
        if (t.startsWith('|')) t = t.mid(1);
        if (t.endsWith('|'))   t.chop(1);
        QStringList cells = t.split('|');
        for (auto& cell : cells) cell = cell.trimmed();
        return cells;
    };

    QList<QStringList> rows;
    for (QTextBlock b = first; ; b = b.next()) {
        rows << parseRow(b.text());
        if (b == last) break;
    }

    // Compute per-column max width
    int cols = 0;
    for (const auto& row : rows) cols = qMax(cols, row.size());
    QVector<int> widths(cols, 0);
    for (const auto& row : rows)
        for (int i = 0; i < row.size(); ++i)
            widths[i] = qMax(widths[i], row[i].length());

    // Preserve leading indent of the first row
    QString indent;
    for (QChar ch : first.text()) {
        if (!ch.isSpace()) break;
        indent += ch;
    }

    // Build formatted lines
    QStringList formatted;
    for (const auto& row : rows) {
        QString line = indent + "|";
        for (int i = 0; i < cols; ++i) {
            const QString cell = (i < row.size()) ? row[i] : QString();
            line += " " + cell.leftJustified(widths[i]) + " |";
        }
        formatted << line;
    }

    // Replace the table range in one undo step
    c.setPosition(first.position());
    c.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    c.insertText(formatted.join('\n'));
}

void PlainTextEditor::setErrorMarks(const QList<QPair<int,int>>& marks)
{
    m_edit->setErrorMarks(marks);
}

void PlainTextEditor::setTagCompletionWords(const QStringList& tags)
{
    m_edit->setTagCompletionWords(tags);
}

void PlainTextEditor::setHighlighter(QSyntaxHighlighter* highlighter)
{
    delete m_highlighter;
    m_highlighter = highlighter;
    if (m_highlighter)
        m_highlighter->setDocument(m_edit->document());
}

void PlainTextEditor::setCompletionWords(const QStringList& words)
{
    m_edit->setBaseCompletionWords(words);
}
