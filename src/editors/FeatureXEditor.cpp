#include "FeatureXEditor.h"
#include "LineNumberEdit.h"
#include "syntax/FeatureXHighlighter.h"

#include <QFileInfo>
#include <QRegularExpression>

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
    lineNumberEdit()->setFoldPattern(
        QRegularExpression(R"(^\s*(Feature|Background|Scenario(?:\s+Outline)?|Examples|Rule|Data)\s*[:\s])",
                           QRegularExpression::CaseInsensitiveOption));
    connect(lineNumberEdit(), &LineNumberEdit::goToDefinitionRequested,
            this,             &FeatureXEditor::goToDefinition);
}

void FeatureXEditor::goToDefinition()
{
    const QString line = textEdit()->textCursor().block().text();
    static const QRegularExpression importRe("^\\s*import\\s+\"([^\"]+)\"");
    const auto match = importRe.match(line);
    if (!match.hasMatch()) return;

    const QString imported = match.captured(1);
    const QString dir      = QFileInfo(filePath()).absolutePath();
    const QString resolved = QFileInfo(dir + "/" + imported).absoluteFilePath();
    emit fileOpenRequested(resolved);
}
