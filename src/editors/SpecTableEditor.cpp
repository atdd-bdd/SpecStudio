#include "SpecTableEditor.h"
#include "syntax/SpecTableHighlighter.h"
#include "../analyzer/SpecTableIndex.h"
#include "../ui/dialogs/AttributeTableDialog.h"

#include <QMenu>
#include <QRegularExpression>
#include <QTextCursor>

SpecTableEditor::SpecTableEditor(const QString& filePath, QWidget* parent)
    : PlainTextEditor(filePath, parent)
{
    setHighlighter(new SpecTableHighlighter(textEdit()->document()));

    setCompletionWords({
        "Specification ", "Entity ", "DomainTerm ", "DataType ", "Attributes ",
        "BusinessRule ", "Calculation ", "Import ", "Insert ", "Define ",
        "Scenario ", "ScenarioGroup ", "Background ",
        "Description ", "Details ", "Constraint ",
        "Examples: EnumerationValues", "Examples: ValidValues", "Examples: ",
        "Transposed",
        "Given ", "When ", "Then ", "And ", "But ",
        "applying BusinessRule ", "applying Calculation ",
        // Built-in DataTypes
        "Character", "String", "Text", "Integer", "Float",
        "Boolean", "Date", "Time", "DateTime", "Duration", "YesNo",
        // Built-in AttributeSets
        "EnumerationValues", "ValidValues",
    });

    lineNumberEdit()->setFoldPattern(
        QRegularExpression(
            R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Scenario|ScenarioGroup|Background|Define)\b)",
            QRegularExpression::CaseInsensitiveOption));
}

void SpecTableEditor::populateContextMenu(QMenu* menu)
{
    if (!m_index) return;

    // Get the word under the cursor
    QTextCursor tc = textEdit()->textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    const QString word = tc.selectedText().trimmed();
    if (word.isEmpty()) return;

    const SpecTableSymbols& syms = m_index->projectSymbols();
    if (!syms.attributes.contains(word) && !syms.entities.contains(word)) return;

    menu->addSeparator();
    auto* act = menu->addAction(tr("Show Attributes: %1").arg(word));
    connect(act, &QAction::triggered, this, [this, word] {
        const QVector<QStringList> rows = m_index->attributeRows(word);
        auto* dlg = new AttributeTableDialog(word, rows, this);
        dlg->show();
    });
}
