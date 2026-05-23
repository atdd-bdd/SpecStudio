#include "FeatureXEditor.h"
#include "syntax/FeatureXHighlighter.h"
#include <QPlainTextEdit>

FeatureXEditor::FeatureXEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new FeatureXHighlighter(textEdit()->document()));
}
