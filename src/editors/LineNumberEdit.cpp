#include "LineNumberEdit.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>

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
