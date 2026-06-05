#include "FeatureXEditor.h"
#include "syntax/FeatureXHighlighter.h"

FeatureXEditor::FeatureXEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new FeatureXHighlighter(textEdit()->document()));
    setCompletionWords({
        "Feature:", "Background:", "Scenario:", "Scenario Outline:",
        "Examples:", "Rule:",
        "Given ", "When ", "Then ", "And ", "But ",
        "Data ", "import ",
    });
}
