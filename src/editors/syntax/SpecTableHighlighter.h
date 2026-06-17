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
};
