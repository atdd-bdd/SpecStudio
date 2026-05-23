#pragma once

#include "GherkinHighlighter.h"

class FeatureXHighlighter : public GherkinHighlighter
{
    Q_OBJECT

public:
    explicit FeatureXHighlighter(QTextDocument* parent = nullptr);
};
