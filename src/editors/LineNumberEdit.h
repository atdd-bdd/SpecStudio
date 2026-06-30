#pragma once

#include <QPlainTextEdit>
#include <QRegularExpression>
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
    QSet<int>          m_foldedBlocks;

    QList<QTextEdit::ExtraSelection> m_currentLineSelections;
    QList<QTextEdit::ExtraSelection> m_bracketSelections;
    QList<QTextEdit::ExtraSelection> m_errorSelections;
};
