#pragma once

#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>

#include <functional>
#include <QSet>
#include <QStringList>
#include <QTextEdit>

class QCompleter;

// QPlainTextEdit with a painted line-number gutter, bracket matching,
// and optional keyword/step autocomplete.
class LineNumberEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LineNumberEdit(QWidget* parent = nullptr);

    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    void lineNumberAreaMousePress(const QPoint& pos);

    // Call once to enable autocomplete with a base keyword list.
    // Document step-lines are added dynamically when the popup opens.
    void setBaseCompletionWords(const QStringList& words);
    void setTagCompletionWords(const QStringList& tags);
    void setAttrSetCompletionWords(const QStringList& words);
    void setTypeCompletionWords(const QStringList& words);
    void setFoldPattern(const QRegularExpression& re);
    // Or decide fold starts by asking: the SpecTable editor asks its parse
    // tree, so a fold begins exactly where the parser says a block does.
    void setFoldStartPredicate(std::function<bool(const QTextBlock&)> isFoldStart);

    void setErrorMarks(const QList<QPair<int,int>>& lineColPairs);
    void clearErrorMarks();

signals:
    void goToDefinitionRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();
    void highlightMatchingBrackets();
    void insertCompletion(const QString& completion);

private:
    static int findMatchingBracket(QTextDocument* doc, int pos,
                                   QChar open, QChar close, bool forward);

    QString currentLinePrefix() const;
    void    updateCompleterWords();
    void    applyExtraSelections();

    static bool isStepColonContext(const QString& blockText, int col);
    bool isTypeColumnContext() const;
    int  tableHeaderTypeColumn() const;
    static int cursorColumnInPipeRow(const QString& lineText, int col);

    void       toggleFold(int blockNumber);
    QTextBlock foldEnd(const QTextBlock& header) const;

    QWidget*    m_lineNumberArea;
    QCompleter* m_completer  = nullptr;
    QStringList m_baseWords;
    QStringList m_tagWords;
    QStringList m_attrSetWords;
    QStringList m_typeWords;

    QRegularExpression m_foldPattern;
    std::function<bool(const QTextBlock&)> m_foldStart;
    bool foldsEnabled() const { return m_foldPattern.isValid() || bool(m_foldStart); }
    bool isFoldStart(const QTextBlock& block) const;
    QSet<int>          m_foldedBlocks;

    QList<QTextEdit::ExtraSelection> m_currentLineSelections;
    QList<QTextEdit::ExtraSelection> m_bracketSelections;
    QList<QTextEdit::ExtraSelection> m_errorSelections;
};
