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

    QPlainTextEdit* textEdit() const { return m_edit; }

protected:
    void setHighlighter(QSyntaxHighlighter* highlighter);

private:
    QPlainTextEdit*    m_edit        = nullptr;
    QSyntaxHighlighter* m_highlighter = nullptr;
};
