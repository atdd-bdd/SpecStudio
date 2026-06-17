#include "SpecTableEditor.h"
#include "syntax/SpecTableHighlighter.h"

#include <QRegularExpression>

SpecTableEditor::SpecTableEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new SpecTableHighlighter(textEdit()->document()));

    setCompletionWords({
        "Entity ", "DomainTerm ", "DataType ", "Attributes ", "BusinessRule ",
        "Calculation ", "Constraint ", "Import ", "Insert ",
        "Given ", "When ", "Then ", "And ", "But ",
        "applying BusinessRule ", "applying Calculation ",
    });

    lineNumberEdit()->setFoldPattern(
        QRegularExpression(
            R"(^\s*(Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Constraint)\b)",
            QRegularExpression::CaseInsensitiveOption));
}
