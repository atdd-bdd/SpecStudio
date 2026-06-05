#pragma once

#include <QPlainTextEdit>

// QPlainTextEdit with a painted line-number gutter on the left margin.
class LineNumberEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LineNumberEdit(QWidget* parent = nullptr);

    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightMatchingBrackets();

private:
    static int findMatchingBracket(QTextDocument* doc, int pos,
                                   QChar open, QChar close, bool forward);

    QWidget* m_lineNumberArea;
};
