#include "DiffView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Comparing
// ---------------------------------------------------------------------------

// Longest common subsequence of two lists, as the indices of the pairs that
// match, in order. A dynamic-programming table of (n+1)(m+1) ints; a
// specification is a few hundred lines, so that is nothing.
static QVector<QPair<int, int>> lcsPairs(const QStringList& a, const QStringList& b)
{
    const int n = a.size(), m = b.size();
    QVector<int> table((n + 1) * (m + 1), 0);
    auto at = [&](int i, int j) -> int& { return table[i * (m + 1) + j]; };
    for (int i = n - 1; i >= 0; --i)
        for (int j = m - 1; j >= 0; --j)
            at(i, j) = (a[i] == b[j]) ? at(i + 1, j + 1) + 1
                                      : qMax(at(i + 1, j), at(i, j + 1));
    QVector<QPair<int, int>> pairs;
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (a[i] == b[j])             { pairs.append({ i, j }); ++i; ++j; }
        else if (at(i + 1, j) >= at(i, j + 1)) ++i;
        else                          ++j;
    }
    return pairs;
}

QVector<DiffView::Row> DiffView::compare(const QString& oldText, const QString& newText)
{
    QStringList a = oldText.split('\n');
    QStringList b = newText.split('\n');
    for (QString& s : a) if (s.endsWith('\r')) s.chop(1);
    for (QString& s : b) if (s.endsWith('\r')) s.chop(1);
    // A trailing newline is not a line.
    if (!a.isEmpty() && a.last().isEmpty()) a.removeLast();
    if (!b.isEmpty() && b.last().isEmpty()) b.removeLast();

    QVector<Row> rows;
    // Between two matched pairs lie a run of removed lines and a run of added
    // lines. Lined up against each other they read as changed lines; whatever
    // is left over on the longer side stands alone.
    auto emitGap = [&](int a0, int a1, int b0, int b1) {
        const int removed = a1 - a0, added = b1 - b0;
        const int paired  = qMin(removed, added);
        for (int k = 0; k < paired; ++k)
            rows.append({ Row::Changed, a[a0 + k], b[b0 + k] });
        for (int k = paired; k < removed; ++k)
            rows.append({ Row::Removed, a[a0 + k], QString() });
        for (int k = paired; k < added; ++k)
            rows.append({ Row::Added, QString(), b[b0 + k] });
    };

    int ai = 0, bi = 0;
    for (const auto& p : lcsPairs(a, b)) {
        emitGap(ai, p.first, bi, p.second);
        rows.append({ Row::Same, a[p.first], b[p.second] });
        ai = p.first + 1;
        bi = p.second + 1;
    }
    emitGap(ai, a.size(), bi, b.size());
    return rows;
}

// The words of a line, keeping the separators as their own tokens so that a
// changed cell of a table is marked and its pipes are not.
static QStringList tokens(const QString& line)
{
    static const QRegularExpression re(R"(\s+|\||[^\s|]+)");
    QStringList out;
    auto it = re.globalMatch(line);
    while (it.hasNext()) out << it.next().captured();
    return out;
}

// ---------------------------------------------------------------------------
// Widget
// ---------------------------------------------------------------------------

DiffView::DiffView(QWidget* parent)
    : QWidget(parent)
{
    m_font = QFont("Courier New");

    auto* bar = new QHBoxLayout();
    bar->setContentsMargins(4, 0, 4, 0);
    m_inlineButton = new QToolButton(this);
    m_inlineButton->setText(tr("Inline"));
    m_inlineButton->setCheckable(true);
    m_inlineButton->setChecked(true);
    m_inlineButton->setToolTip(tr("One document, with what went away struck through and what "
                                  "arrived underlined -- as Word compares"));
    m_sideBySideButton = new QToolButton(this);
    m_sideBySideButton->setText(tr("Side by side"));
    m_sideBySideButton->setCheckable(true);
    m_sideBySideButton->setToolTip(tr("The earlier version on the left, the current one on the "
                                      "right, scrolled together"));
    bar->addWidget(m_inlineButton);
    bar->addWidget(m_sideBySideButton);
    bar->addStretch();

    m_stack = new QStackedWidget(this);

    m_inlineView = new QTextEdit(m_stack);
    m_inlineView->setReadOnly(true);
    m_inlineView->setLineWrapMode(QTextEdit::NoWrap);
    m_stack->addWidget(m_inlineView);

    m_sidePage = new QWidget(m_stack);
    auto* sideLayout = new QVBoxLayout(m_sidePage);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);
    auto* titles = new QHBoxLayout();
    m_leftTitle  = new QLabel(m_sidePage);
    m_rightTitle = new QLabel(m_sidePage);
    titles->addWidget(m_leftTitle, 1);
    titles->addWidget(m_rightTitle, 1);
    sideLayout->addLayout(titles);
    auto* splitter = new QSplitter(Qt::Horizontal, m_sidePage);
    m_leftView  = new QPlainTextEdit(splitter);
    m_rightView = new QPlainTextEdit(splitter);
    for (QPlainTextEdit* v : { m_leftView, m_rightView }) {
        v->setReadOnly(true);
        v->setLineWrapMode(QPlainTextEdit::NoWrap);
    }
    splitter->addWidget(m_leftView);
    splitter->addWidget(m_rightView);
    sideLayout->addWidget(splitter, 1);
    m_stack->addWidget(m_sidePage);

    // The two sides hold the same number of lines, so the same scroll value
    // shows the same rows on both.
    connect(m_leftView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_rightView->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_rightView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_leftView->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_leftView->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_rightView->horizontalScrollBar(), &QScrollBar::setValue);
    connect(m_rightView->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_leftView->horizontalScrollBar(), &QScrollBar::setValue);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addLayout(bar);
    layout->addWidget(m_stack, 1);

    connect(m_inlineButton,     &QToolButton::clicked, this, [this] { setMode(Mode::Inline); });
    connect(m_sideBySideButton, &QToolButton::clicked, this, [this] { setMode(Mode::SideBySide); });

    setDiffFont(m_font);
}

void DiffView::setDiffFont(const QFont& font)
{
    m_font = font;
    m_inlineView->setFont(font);
    m_leftView->setFont(font);
    m_rightView->setFont(font);
}

void DiffView::setMode(Mode mode)
{
    m_mode = mode;
    m_inlineButton->setChecked(mode == Mode::Inline);
    m_sideBySideButton->setChecked(mode == Mode::SideBySide);
    render();
}

void DiffView::setTexts(const QString& oldText, const QString& newText,
                        const QString& oldTitle, const QString& newTitle)
{
    m_rows           = compare(oldText, newText);
    m_oldTitle       = oldTitle;
    m_newTitle       = newTitle;
    m_showingMessage = false;
    render();
}

void DiffView::setMessage(const QString& text)
{
    m_rows.clear();
    m_showingMessage = true;
    m_inlineView->setPlainText(text);
    m_leftView->setPlainText(text);
    m_rightView->clear();
    m_leftTitle->clear();
    m_rightTitle->clear();
    m_stack->setCurrentWidget(m_mode == Mode::Inline ? static_cast<QWidget*>(m_inlineView)
                                                     : m_sidePage);
}

void DiffView::clear()
{
    setMessage(QString());
}

void DiffView::render()
{
    if (m_showingMessage) {
        m_stack->setCurrentWidget(m_mode == Mode::Inline ? static_cast<QWidget*>(m_inlineView)
                                                         : m_sidePage);
        return;
    }
    if (m_mode == Mode::Inline) renderInline();
    else                        renderSideBySide();
}

// ---------------------------------------------------------------------------
// Inline: one document, marked the way Word marks a comparison
// ---------------------------------------------------------------------------

void DiffView::renderInline()
{
    m_inlineView->clear();
    QTextCursor c(m_inlineView->document());

    QTextCharFormat plain;
    plain.setFont(m_font);
    QTextCharFormat removed = plain;
    removed.setForeground(QColor(170, 0, 0));
    removed.setFontStrikeOut(true);
    removed.setBackground(QColor(255, 228, 228));
    QTextCharFormat added = plain;
    added.setForeground(QColor(0, 110, 0));
    added.setFontUnderline(true);
    added.setBackground(QColor(224, 245, 224));
    QTextCharFormat heading = plain;
    heading.setForeground(QColor(100, 100, 100));

    c.insertText(tr("%1  compared with  %2\n\n").arg(m_oldTitle, m_newTitle), heading);

    for (const Row& row : m_rows) {
        switch (row.kind) {
        case Row::Same:
            c.insertText(row.left + '\n', plain);
            break;
        case Row::Removed:
            c.insertText(row.left, removed);
            c.insertText("\n", plain);
            break;
        case Row::Added:
            c.insertText(row.right, added);
            c.insertText("\n", plain);
            break;
        case Row::Changed: {
            // Word by word: what the two lines share is plain, what only the
            // old one had is struck through, what only the new one has is
            // underlined -- in place, so the row keeps its shape.
            const QStringList a = tokens(row.left);
            const QStringList b = tokens(row.right);
            int ai = 0, bi = 0;
            for (const auto& p : lcsPairs(a, b)) {
                for (; ai < p.first; ++ai) c.insertText(a[ai], removed);
                for (; bi < p.second; ++bi) c.insertText(b[bi], added);
                c.insertText(a[p.first], plain);
                ai = p.first + 1;
                bi = p.second + 1;
            }
            for (; ai < a.size(); ++ai) c.insertText(a[ai], removed);
            for (; bi < b.size(); ++bi) c.insertText(b[bi], added);
            c.insertText("\n", plain);
            break;
        }
        }
    }
    m_inlineView->moveCursor(QTextCursor::Start);
    m_stack->setCurrentWidget(m_inlineView);
}

// ---------------------------------------------------------------------------
// Side by side: the two versions, lined up
// ---------------------------------------------------------------------------

void DiffView::renderSideBySide()
{
    QStringList left, right;
    for (const Row& row : m_rows) {
        left  << row.left;
        right << row.right;
    }
    m_leftView->setPlainText(left.join('\n'));
    m_rightView->setPlainText(right.join('\n'));
    m_leftTitle->setText("  " + m_oldTitle);
    m_rightTitle->setText("  " + m_newTitle);

    // A tint across the full width of every line that differs, and grey for a
    // row that is only there to keep the other side's line level.
    auto tint = [&](QPlainTextEdit* view, bool isLeft) {
        QList<QTextEdit::ExtraSelection> marks;
        QTextBlock block = view->document()->firstBlock();
        for (const Row& row : m_rows) {
            if (!block.isValid()) break;
            QColor colour;
            switch (row.kind) {
            case Row::Same:    break;
            case Row::Changed: colour = isLeft ? QColor(255, 228, 228) : QColor(224, 245, 224); break;
            case Row::Removed: colour = isLeft ? QColor(255, 210, 210) : QColor(235, 235, 235); break;
            case Row::Added:   colour = isLeft ? QColor(235, 235, 235) : QColor(205, 240, 205); break;
            }
            if (colour.isValid()) {
                QTextEdit::ExtraSelection sel;
                sel.cursor = QTextCursor(block);
                sel.format.setBackground(colour);
                sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                marks << sel;
            }
            block = block.next();
        }
        view->setExtraSelections(marks);
    };
    tint(m_leftView, true);
    tint(m_rightView, false);

    m_leftView->moveCursor(QTextCursor::Start);
    m_rightView->moveCursor(QTextCursor::Start);
    m_stack->setCurrentWidget(m_sidePage);
}
