#include "CursorModel.h"

#include "SpectableParser.h"

#include <algorithm>

void CursorModel::update(const QString& text, const QString& filePath, int revision)
{
    if (revision == m_revision && filePath == m_filePath) return;
    m_file     = SpectableParser().parseText(text, filePath);
    m_filePath = filePath;
    m_revision = revision;
}

template <typename F>
void CursorModel::eachStep(F f) const
{
    for (const Scenario& s : m_file.scenarios)
        for (const Step& st : s.steps) f(st);
    for (const Step& st : m_file.backgroundSteps) f(st);
    for (const Step& st : m_file.cleanupSteps)    f(st);
}

// ---------------------------------------------------------------------------
// Steps
// ---------------------------------------------------------------------------

const Step* CursorModel::stepAt(int line) const
{
    const Step* found = nullptr;
    eachStep([&](const Step& st) { if (st.line == line) found = &st; });
    return found;
}

int CursorModel::tableFirstLine(const Step& step) const
{
    return step.table.rowLines.isEmpty() ? 0 : step.table.rowLines.first();
}

int CursorModel::tableLastLine(const Step& step) const
{
    return step.table.rowLines.isEmpty() ? 0 : step.table.rowLines.last();
}

const Step* CursorModel::stepOwning(int line) const
{
    // The nearest step at or above the line, if the line is within what the
    // step owns: its own line, its rows, its =Define line, its docstring.
    const Step* best = nullptr;
    eachStep([&](const Step& st) {
        if (st.line <= line && (!best || st.line > best->line)) best = &st;
    });
    if (!best) return nullptr;
    int last = best->line;
    last = qMax(last, tableLastLine(*best));
    last = qMax(last, best->defineRefLine);
    last = qMax(last, best->docStringEndLine);
    // A step's named comments and their rows are its too.
    for (const NamedComment& c : best->comments) {
        last = qMax(last, c.line + c.rows.size());
    }
    return line <= last ? best : nullptr;
}

// ---------------------------------------------------------------------------
// Blocks
// ---------------------------------------------------------------------------

const AttrSet* CursorModel::attrSetAt(int line) const
{
    for (const AttrSet& as : m_file.attrSets)
        if (!as.isContext && !as.imported && as.line == line) return &as;
    return nullptr;
}

const AttrSet* CursorModel::attrSetOwning(int line) const
{
    for (const AttrSet& as : m_file.attrSets) {
        if (as.isContext || as.imported || as.line > line) continue;
        int last = qMax(as.line, as.headerLine);
        for (const Field& f : as.fields) last = qMax(last, f.line);
        for (const NamedComment& c : as.comments) last = qMax(last, c.line + c.rows.size());
        if (line <= last) return &as;
    }
    return nullptr;
}

const Collection* CursorModel::collectionAt(int line) const
{
    for (const Collection& c : m_file.collections)
        if (!c.isContext && c.line == line) return &c;
    return nullptr;
}

const NamedBlock* CursorModel::namedBlockOwning(int line) const
{
    // The block is the last thing that starts at or above the line, whatever
    // kind of thing that is; only then is the line inside it.
    int lastStart = 0;
    for (const AttrSet& x : m_file.attrSets)      if (!x.isContext && !x.imported && x.line <= line) lastStart = qMax(lastStart, x.line);
    for (const Collection& x : m_file.collections) if (!x.isContext && x.line <= line) lastStart = qMax(lastStart, x.line);
    for (const Define& x : m_file.defines)         if (!x.isContext && !x.imported && x.line <= line) lastStart = qMax(lastStart, x.line);
    for (const Scenario& x : m_file.scenarios)     if (x.line <= line) lastStart = qMax(lastStart, x.line);
    const NamedBlock* block = nullptr;
    for (const NamedBlock& nb : m_file.namedBlocks) {
        if (nb.isContext || nb.imported || nb.line > line) continue;
        if (nb.line >= lastStart) { lastStart = nb.line; block = &nb; }
    }
    return block;
}

bool CursorModel::isExamplesLine(int line, const NamedBlock** owner) const
{
    for (const NamedBlock& nb : m_file.namedBlocks) {
        if (nb.isContext || nb.imported) continue;
        if (nb.examples.line == line) {
            if (owner) *owner = &nb;
            return true;
        }
    }
    return false;
}

const Scenario* CursorModel::scenarioOwning(int line) const
{
    int lastStart = 0;
    for (const AttrSet& x : m_file.attrSets)      if (!x.isContext && !x.imported && x.line <= line) lastStart = qMax(lastStart, x.line);
    for (const Collection& x : m_file.collections) if (!x.isContext && x.line <= line) lastStart = qMax(lastStart, x.line);
    for (const Define& x : m_file.defines)         if (!x.isContext && !x.imported && x.line <= line) lastStart = qMax(lastStart, x.line);
    for (const NamedBlock& x : m_file.namedBlocks) if (!x.isContext && !x.imported && x.line <= line) lastStart = qMax(lastStart, x.line);
    const Scenario* found = nullptr;
    for (const Scenario& sc : m_file.scenarios) {
        if (sc.line > line) continue;
        if (sc.line >= lastStart) { lastStart = sc.line; found = &sc; }
    }
    return found;
}

// ---------------------------------------------------------------------------
// Lines of other kinds
// ---------------------------------------------------------------------------

bool CursorModel::isImportLine(int line) const
{
    return m_file.importLines.contains(line);
}

bool CursorModel::isInsertLine(int line) const
{
    return m_file.insertLines.contains(line);
}

const NamedComment* CursorModel::namedCommentAt(int line) const
{
    auto in = [&](const QVector<NamedComment>& v) -> const NamedComment* {
        for (const NamedComment& c : v) if (c.line == line) return &c;
        return nullptr;
    };
    if (auto* c = in(m_file.comments)) return c;
    for (const AttrSet& as : m_file.attrSets)      if (auto* c = in(as.comments)) return c;
    for (const NamedBlock& nb : m_file.namedBlocks) if (auto* c = in(nb.comments)) return c;
    for (const Scenario& sc : m_file.scenarios) {
        if (auto* c = in(sc.comments)) return c;
        for (const Step& st : sc.steps) if (auto* c = in(st.comments)) return c;
    }
    for (const Step& st : m_file.backgroundSteps) if (auto* c = in(st.comments)) return c;
    for (const Step& st : m_file.cleanupSteps)    if (auto* c = in(st.comments)) return c;
    return nullptr;
}

bool CursorModel::isTableRow(int line) const
{
    bool found = false;
    eachStep([&](const Step& st) { if (st.table.rowLines.contains(line)) found = true; });
    if (found) return true;
    for (const AttrSet& as : m_file.attrSets) {
        if (as.isContext || as.imported) continue;
        if (as.headerLine == line) return true;
        for (const Field& f : as.fields) if (f.line == line) return true;
    }
    for (const NamedBlock& nb : m_file.namedBlocks) {
        if (nb.isContext || nb.imported) continue;
        if (nb.examples.headerLine == line || nb.examples.rowLines.contains(line)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Block starts
// ---------------------------------------------------------------------------

QVector<int> CursorModel::blockStartLines() const
{
    QVector<int> starts;
    if (m_file.specLine) starts << m_file.specLine;
    for (const AttrSet& x : m_file.attrSets)       if (!x.isContext && !x.imported) starts << x.line;
    for (const Collection& x : m_file.collections)  if (!x.isContext) starts << x.line;
    for (const DomainTerm& x : m_file.domainTerms)  if (!x.isContext) starts << x.line;
    for (const Define& x : m_file.defines)
        if (!x.isContext && !x.imported && x.line) starts << (x.listLine ? x.listLine : x.line);
    for (const NamedBlock& x : m_file.namedBlocks)  if (!x.isContext && !x.imported) starts << x.line;
    for (const Scenario& x : m_file.scenarios)      starts << x.line;
    for (const ScenarioGroup& x : m_file.scenarioGroups) starts << x.line;
    starts << m_file.backgroundLines << m_file.cleanupLines;
    std::sort(starts.begin(), starts.end());
    return starts;
}

bool CursorModel::isBlockStart(int line) const
{
    return blockStartLines().contains(line);
}

int CursorModel::endOfEnclosingBlock(int line, const QStringList& lines) const
{
    int next = lines.size() + 1;
    for (int start : blockStartLines())
        if (start > line) { next = start; break; }
    int last = line;
    for (int l = line + 1; l < next && l <= lines.size(); ++l)
        if (!lines[l - 1].trimmed().isEmpty()) last = l;
    return last;
}
