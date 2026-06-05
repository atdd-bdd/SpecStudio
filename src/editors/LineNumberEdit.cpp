#include "LineNumberEdit.h"

#include <QPainter>
#include <QPaintEvent>
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

void LineNumberEdit::highlightMatchingBrackets()
{
    QList<QTextEdit::ExtraSelection> extras;

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

    setExtraSelections(extras);
}
