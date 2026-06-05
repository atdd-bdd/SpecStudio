#include "LineNumberEdit.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QSet>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

// ---------------------------------------------------------------------------
// LineNumberArea — painted as the left gutter of LineNumberEdit
// ---------------------------------------------------------------------------
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(LineNumberEdit* editor)
        : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    LineNumberEdit* m_editor;
};

// ---------------------------------------------------------------------------
// LineNumberEdit
// ---------------------------------------------------------------------------

LineNumberEdit::LineNumberEdit(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &LineNumberEdit::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &LineNumberEdit::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &LineNumberEdit::highlightCurrentLine);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &LineNumberEdit::highlightMatchingBrackets);

    updateLineNumberAreaWidth();
}

int LineNumberEdit::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    return 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void LineNumberEdit::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(cr.left(), cr.top(),
                                  lineNumberAreaWidth(), cr.height());
}

void LineNumberEdit::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void LineNumberEdit::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void LineNumberEdit::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(245, 245, 245));

    QTextBlock block     = firstVisibleBlock();
    int blockNumber      = block.blockNumber();
    int top    = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(QColor(130, 130, 130));
            painter.drawText(0, top,
                             m_lineNumberArea->width() - 4,
                             fontMetrics().height(),
                             Qt::AlignRight,
                             QString::number(blockNumber + 1));
        }
        block = block.next();
        top   = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ---------------------------------------------------------------------------
// Bracket / quote matching
// ---------------------------------------------------------------------------

static void addBracketSelection(QList<QTextEdit::ExtraSelection>& list,
                                QTextDocument* doc, int pos, bool matched)
{
    QTextEdit::ExtraSelection sel;
    sel.format.setBackground(matched ? QColor(180, 238, 180) : QColor(255, 160, 160));
    sel.format.setForeground(Qt::black);
    sel.cursor = QTextCursor(doc);
    sel.cursor.setPosition(pos);
    sel.cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
    list.append(sel);
}

int LineNumberEdit::findMatchingBracket(QTextDocument* doc, int pos,
                                        QChar open, QChar close, bool forward)
{
    int depth = 1;
    int i = pos;
    const int count = doc->characterCount();
    while (true) {
        i += forward ? 1 : -1;
        if (i < 0 || i >= count) return -1;
        const QChar c = doc->characterAt(i);
        if (c == (forward ? open : close)) ++depth;
        else if (c == (forward ? close : open)) {
            if (--depth == 0) return i;
        }
    }
}

// ---------------------------------------------------------------------------
// Autocomplete
// ---------------------------------------------------------------------------

void LineNumberEdit::setBaseCompletionWords(const QStringList& words)
{
    m_baseWords = words;
    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_completer->setWidget(this);
        m_completer->setCompletionMode(QCompleter::PopupCompletion);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setModel(new QStringListModel(this));
        connect(m_completer, qOverload<const QString&>(&QCompleter::activated),
                this, &LineNumberEdit::insertCompletion);
    }
}

QString LineNumberEdit::currentLinePrefix() const
{
    QTextCursor tc = textCursor();
    const int col  = tc.positionInBlock();
    const QString blockText = tc.block().text();
    int start = 0;
    while (start < col && blockText[start].isSpace()) ++start;
    return blockText.mid(start, col - start);
}

void LineNumberEdit::updateCompleterWords()
{
    static const QStringList stepPrefixes = {
        "given ", "when ", "then ", "and ", "but "
    };

    QSet<QString> seen;
    QStringList all = m_baseWords;
    for (const QString& w : m_baseWords)
        seen.insert(w.toLower());

    QTextBlock b = document()->begin();
    while (b.isValid()) {
        const QString line  = b.text().trimmed();
        const QString lower = line.toLower();
        if (!line.isEmpty() && !seen.contains(lower)) {
            for (const QString& p : stepPrefixes) {
                if (lower.startsWith(p)) {
                    seen.insert(lower);
                    all.append(line);
                    break;
                }
            }
        }
        b = b.next();
    }

    qobject_cast<QStringListModel*>(m_completer->model())->setStringList(all);
}

void LineNumberEdit::insertCompletion(const QString& completion)
{
    QTextCursor tc = textCursor();
    const QString prefix = currentLinePrefix();
    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefix.length());
    tc.insertText(completion);
}

void LineNumberEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F12) {
        emit goToDefinitionRequested();
        return;
    }

    if (m_completer && m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    if (!m_completer) return;
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) return;

    const QString prefix = currentLinePrefix();
    if (prefix.length() < 2) {
        m_completer->popup()->hide();
        return;
    }

    updateCompleterWords();
    m_completer->setCompletionPrefix(prefix);

    if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        return;
    }

    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr);
}

// ---------------------------------------------------------------------------
// Bracket / quote matching
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Current line highlight + shared ExtraSelection merge
// ---------------------------------------------------------------------------

void LineNumberEdit::applyExtraSelections()
{
    setExtraSelections(m_currentLineSelections + m_bracketSelections + m_errorSelections);
}

void LineNumberEdit::clearErrorMarks()
{
    m_errorSelections.clear();
    applyExtraSelections();
}

void LineNumberEdit::setErrorMarks(const QList<QPair<int,int>>& lineColPairs)
{
    m_errorSelections.clear();
    for (const auto& [line, col] : lineColPairs) {
        const QTextBlock block = document()->findBlockByLineNumber(line - 1);
        if (!block.isValid()) continue;
        const int pos = block.position() + qMax(0, col - 1);
        QTextCursor c(document());
        c.setPosition(pos);
        c.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        if (!c.hasSelection())
            c.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection sel;
        sel.format.setUnderlineColor(Qt::red);
        sel.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        sel.cursor = c;
        m_errorSelections.append(sel);
    }
    applyExtraSelections();
}

void LineNumberEdit::highlightCurrentLine()
{
    m_currentLineSelections.clear();
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor(255, 255, 210));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        m_currentLineSelections.append(sel);
    }
    applyExtraSelections();
}

void LineNumberEdit::highlightMatchingBrackets()
{
    m_bracketSelections.clear();
    QList<QTextEdit::ExtraSelection>& extras = m_bracketSelections;

    static const QString opens  = "([{";
    static const QString closes = ")]}";

    const int pos = textCursor().position();

    auto tryPos = [&](int p) -> bool {
        if (p < 0 || p >= document()->characterCount() - 1) return false;
        const QChar ch = document()->characterAt(p);
        const int oi = opens.indexOf(ch);
        const int ci = closes.indexOf(ch);
        if (oi < 0 && ci < 0) return false;

        int matchPos;
        if (oi >= 0)
            matchPos = findMatchingBracket(document(), p, opens[oi], closes[oi], true);
        else
            matchPos = findMatchingBracket(document(), p, opens[ci], closes[ci], false);

        addBracketSelection(extras, document(), p, matchPos >= 0);
        if (matchPos >= 0)
            addBracketSelection(extras, document(), matchPos, true);
        return true;
    };

    // Prefer char to the left of cursor, fall back to char at cursor position
    if (!tryPos(pos - 1))
        tryPos(pos);

    applyExtraSelections();
}
