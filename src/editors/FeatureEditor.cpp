#include "FeatureEditor.h"
#include "syntax/GherkinHighlighter.h"

FeatureEditor::FeatureEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new GherkinHighlighter(textEdit()->document()));
    setCompletionWords({
        "Feature:", "Background:", "Scenario:", "Scenario Outline:",
        "Examples:", "Rule:",
        "Given ", "When ", "Then ", "And ", "But ",
    });
}
