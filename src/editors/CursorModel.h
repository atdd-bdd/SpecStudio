#pragma once

#include "SpectableModel.h"

#include <QString>

// What is at a line of the text being edited, asked of the parse tree.
//
// The editor used to decide with regular expressions what the line under the
// cursor was -- a step, a table row, an Attributes header -- one pattern per
// question, seventy-odd of them, each a small private reading of the language
// that drifted from the parser's: its step pattern knew Vertical and not
// CompareOnly or EveryCell, so a step carrying either was not a step to the
// context menu. Now the current text is parsed (once per document revision)
// and the questions are asked of the tree, whose elements all carry the line
// they came from.
//
// Lines are 1-based, as everywhere in the model.
class CursorModel
{
public:
    // Re-parse when the text changed. `revision` is the document's revision
    // counter; the same value means the same text and the parse is kept.
    void update(const QString& text, const QString& filePath, int revision);

    const SpectableFile& file() const { return m_file; }

    // The step declared on this line.
    const Step* stepAt(int line) const;
    // The step whose table, =Define line or docstring this line belongs to,
    // or the step itself. Null between steps.
    const Step* stepOwning(int line) const;
    // The Attributes/Entity block declared on this line, or whose field
    // table this line is in.
    const AttrSet* attrSetAt(int line) const;
    const AttrSet* attrSetOwning(int line) const;
    // The Collection declared on this line.
    const Collection* collectionAt(int line) const;
    // The BusinessRule/Calculation/DataType whose block this line is in --
    // from its heading to the last row of its Examples table.
    const NamedBlock* namedBlockOwning(int line) const;
    // True when this line is a block's "Examples:" line.
    bool isExamplesLine(int line, const NamedBlock** owner = nullptr) const;
    // The Scenario whose block this line is in.
    const Scenario* scenarioOwning(int line) const;

    bool isImportLine(int line) const;
    bool isInsertLine(int line) const;
    // The named comment on this line (Description, Details, Notes, Constraint,
    // Uses), wherever it belongs.
    const NamedComment* namedCommentAt(int line) const;

    // True when this line is a row of any table the parser read.
    bool isTableRow(int line) const;

    // The lines every top-level block starts on -- Specification, Attributes,
    // Entity, Collection, DomainTerm, DataType, BusinessRule, Calculation,
    // Scenario, ScenarioGroup, Background, Cleanup, Define -- sorted.
    QVector<int> blockStartLines() const;
    bool isBlockStart(int line) const;
    // The last non-blank line before the next block starts after `line`,
    // given the lines of the text; `line` itself when nothing follows.
    int  endOfEnclosingBlock(int line, const QStringList& lines) const;

    // The first line of the table a step's rows form, and the last, or 0.
    int tableFirstLine(const Step& step) const;
    int tableLastLine(const Step& step) const;

private:
    template <typename F> void eachStep(F f) const;

    SpectableFile m_file;
    QString       m_filePath;
    int           m_revision = -1;
};
