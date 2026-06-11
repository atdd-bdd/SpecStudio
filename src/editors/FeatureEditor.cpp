#include "FeatureEditor.h"
#include "syntax/GherkinHighlighter.h"

#include <QRegularExpression>

FeatureEditor::FeatureEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new GherkinHighlighter(textEdit()->document()));
    setCompletionWords({
        "Feature:", "Background:", "Scenario:", "Scenario Outline:",
        "Examples:", "Rule:",
        "Given ", "When ", "Then ", "And ", "But ",
    });
    lineNumberEdit()->setFoldPattern(
        QRegularExpression(R"(^\s*(Feature|Background|Scenario(?:\s+Outline)?|Examples|Rule)\s*:)",
                           QRegularExpression::CaseInsensitiveOption));
}
