#pragma once

#include <QPlainTextEdit>
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

    // Call once to enable autocomplete with a base keyword list.
    // Document step-lines are added dynamically when the popup opens.
    void setBaseCompletionWords(const QStringList& words);

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

    void applyExtraSelections();

    QWidget*    m_lineNumberArea;
    QCompleter* m_completer  = nullptr;
    QStringList m_baseWords;

    QList<QTextEdit::ExtraSelection> m_currentLineSelections;
    QList<QTextEdit::ExtraSelection> m_bracketSelections;
};
