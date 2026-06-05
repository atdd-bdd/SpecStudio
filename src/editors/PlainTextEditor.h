#pragma once

#include "BaseEditor.h"

class QPlainTextEdit;
class QSyntaxHighlighter;

class PlainTextEditor : public BaseEditor
{
    Q_OBJECT

public:
    explicit PlainTextEditor(const QString& filePath, QWidget* parent = nullptr);

    void load(const QString& path) override;
    bool save() override;

    void cut()   override;
    void copy()  override;
    void paste() override;

    bool findNext(const QString& text, bool caseSensitive, bool wrapAround);
    bool findPrev(const QString& text, bool caseSensitive, bool wrapAround);
    bool replaceCurrent(const QString& replacement);
    int  replaceAll(const QString& findText, const QString& replacement, bool caseSensitive);

    QPlainTextEdit* textEdit() const { return m_edit; }

protected:
    void setHighlighter(QSyntaxHighlighter* highlighter);

private:
    QPlainTextEdit*    m_edit        = nullptr;
    QSyntaxHighlighter* m_highlighter = nullptr;
};
