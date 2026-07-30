#include "SpecTableBlocks.h"

#include <QRegularExpression>
#include <QVector>

namespace {

// The keywords that open a block. Import and Insert are here even though they are
// single lines: either one ends the block above it, so treating them as headers is
// what keeps a trailing `Import` out of the Scenario before it.
const QRegularExpression& blockStartPattern()
{
    static const QRegularExpression re(
        R"(^\s*(Specification|Entity|Collection|DomainTerm|DataType|Attributes|)"
        R"(BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|)"
        R"(Background|Cleanup|Define)\b)",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

// Which lines sit *inside* a """ ... """ region, fences excluded.
//
// This is not defensive programming, it is the common case: a docstring holds
// arbitrary text, and include.spectable really does contain
//
//     Given a string include
//     """
//     Insert "string.txt"
//     """
//
// Without this, that Scenario would end at its own docstring's Insert line.
QVector<bool> docstringInterior(const QStringList& lines)
{
    QVector<bool> inside(lines.size(), false);
    bool open = false;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == QLatin1String("\"\"\"")) {
            open = !open;
            continue;               // the fence itself is not interior
        }
        inside[i] = open;
    }
    return inside;
}

} // namespace

namespace SpecTableBlocks {

bool isBlockHeader(const QStringList& lines, int index)
{
    if (index < 0 || index >= lines.size()) return false;
    if (docstringInterior(lines).at(index)) return false;
    return blockStartPattern().match(lines[index]).hasMatch();
}

bool blockKeywordRange(const QStringList& lines, int index, int* startCol, int* endCol)
{
    if (index < 0 || index >= lines.size() || !startCol || !endCol) return false;
    if (docstringInterior(lines).at(index)) return false;

    const auto m = blockStartPattern().match(lines[index]);
    if (!m.hasMatch()) return false;

    *startCol = m.capturedStart(1);
    *endCol   = m.capturedEnd(1);
    return true;
}

bool rangeForLine(const QStringList& lines, int index, int* firstLine, int* lastLine)
{
    if (index < 0 || index >= lines.size() || !firstLine || !lastLine)
        return false;

    const QVector<bool>          inDoc = docstringInterior(lines);
    const QRegularExpression&    re    = blockStartPattern();

    const auto header = [&](int i) {
        return !inDoc.at(i) && re.match(lines[i]).hasMatch();
    };

    // A click inside a docstring belongs to the block that owns the docstring, so
    // searching backwards from the clicked line handles it without a special case.
    int start = -1;
    for (int i = index; i >= 0; --i)
        if (header(i)) { start = i; break; }
    if (start < 0) return false;

    int end = start;
    for (int i = start + 1; i < lines.size(); ++i) {
        if (header(i)) break;
        end = i;
    }

    while (end > start && lines[end].trimmed().isEmpty())
        --end;

    *firstLine = start;
    *lastLine  = end;
    return true;
}

} // namespace SpecTableBlocks
