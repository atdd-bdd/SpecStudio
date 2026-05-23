#include "FeatureEditor.h"
#include "syntax/GherkinHighlighter.h"
#include <QPlainTextEdit>

FeatureEditor::FeatureEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new GherkinHighlighter(textEdit()->document()));
}
