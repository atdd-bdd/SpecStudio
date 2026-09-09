#pragma once

#include "GherkinHighlighter.h"

class SpecTableHighlighter : public GherkinHighlighter
{
    Q_OBJECT

public:
    explicit SpecTableHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    void buildRules();

    // A docstring spans lines, so it cannot be a rule: the rule list only ever
    // sees one line. The block state carries "inside a docstring" from one line
    // to the next, and this is the colour the whole of it gets.
    enum BlockState { Normal = 0, InDocString = 1 };
    QTextCharFormat m_docStringFmt;
};
