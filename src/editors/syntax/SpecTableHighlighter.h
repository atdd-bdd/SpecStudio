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

    // Underlines each misspelled word in the line, after the colouring, so the
    // underline sits on top of whatever colour the word already has.
    void checkSpelling(const QString& text);

    // A docstring spans lines, so it cannot be a rule: the rule list only ever
    // sees one line. The block state carries "inside a docstring" from one line
    // to the next, and this is the colour the whole of it gets.
    enum BlockState { Normal = 0, InDocString = 1 };
    QTextCharFormat m_docStringFmt;
};
