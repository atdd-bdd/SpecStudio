#include "SpectableParser.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QStringList SpectableParser::splitPipeRow(const QString& line)
{
    QStringList parts = line.split('|');
    QStringList result;
    for (int i = 1; i < parts.size() - 1; ++i)
        result << parts[i].trimmed();
    return result;
}

bool SpectableParser::isPipeRow(const QString& trimmed)
{
    return trimmed.startsWith('|');
}

const QStringList& builtinDataTypeNames()
{
    static const QStringList names = {
        "Character", "String", "Text", "Integer", "Float", "Scientific", "Decimal",
        "Boolean", "Date", "Time", "DateTime", "Duration", "YesNo"
    };
    return names;
}

bool isBuiltinDataType(const QString& name)
{
    const QString t = name.trimmed().toLower();
    if (t == "int" || t == "long" || t == "bool") return true;
    for (const QString& n : builtinDataTypeNames())
        if (n.compare(t, Qt::CaseInsensitive) == 0) return true;
    return false;
}

QString SpectableParser::normalizeKeyword(const QString& kw, const QString& last)
{
    const QString kwLow = kw.toLower();
    if (kwLow == "and")
        return last.isEmpty() ? "Given" : last;
    if (kwLow == "whenthen")
        return QStringLiteral("WhenThen");
    return kw[0].toUpper() + kw.mid(1).toLower();
}

QString SpectableParser::toMethodName(const QString& stepText)
{
    QString s = stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s = s.trimmed().remove(QRegularExpression("^_+|_+$"));
    return s;
}

bool SpectableParser::isDefineLine(const QString& trimmed, QString& defineName)
{
    static QRegularExpression re(R"(^=(\w+)\s*$)");
    auto m = re.match(trimmed);
    if (!m.hasMatch()) return false;
    defineName = m.captured(1);
    return true;
}

bool SpectableParser::isStepLine(const QString& trimmed,
                                  QString& kw, QString& text,
                                  QString& attrSet, bool& vertical, bool& compareOnly,
                                  bool& everyCell, QString& badModifier)
{
    badModifier.clear();
    static QRegularExpression reStep(
        R"(^\s*(Given|When|Then|And|WhenThen)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    // Any number of modifiers, in any order. They answer different questions --
    // Vertical is how the table is laid out, CompareOnly is which of its columns
    // are compared -- so a step may want both, and did not used to be able to
    // say so.
    static QRegularExpression reAttr(
        R"(\s*:\s*(\w+)((?:\s+\w+)*)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    auto m = reStep.match(trimmed);
    if (!m.hasMatch()) return false;

    kw           = m.captured(1);
    QString rest = m.captured(2).trimmed();

    auto ma = reAttr.match(rest);
    if (ma.hasMatch()) {
        attrSet     = ma.captured(1);
        vertical    = false;
        compareOnly = false;
        everyCell   = false;

        // Whatever followed the type. A word that is not a modifier is named as
        // the mistake it is rather than swallowed into the step text, which is
        // what used to happen: the line then read as a step with no attribute
        // set at all and the misspelling was never mentioned.
        const QStringList mods = ma.captured(2).split(QRegularExpression(R"(\s+)"),
                                                      Qt::SkipEmptyParts);
        for (const QString& mod : mods) {
            if (mod.compare("Vertical", Qt::CaseInsensitive) == 0)
                vertical = true;
            else if (mod.compare("CompareOnly", Qt::CaseInsensitive) == 0)
                compareOnly = true;
            else if (mod.compare("EveryCell", Qt::CaseInsensitive) == 0)
                everyCell = true;
            else if (badModifier.isEmpty())
                badModifier = mod;
        }
        text = rest.left(ma.capturedStart()).trimmed();
    } else {
        attrSet     = {};
        vertical  = false;
        compareOnly = false;
        everyCell   = false;
        text        = rest;
    }
    return true;
}

// A continuation is marked at the START of the following line, not with a
// trailing backslash on this one. This used to test for a trailing one and was
// called from nowhere, so a trailing backslash did nothing at all; the real
// rule lives in parseText, beside the named comments it continues.
bool SpectableParser::isContinuation(const QString& line)
{
    return line.trimmed().startsWith('\\');
}

// A named comment: documentation attached to the element above it, carrying no
// behaviour. These are transparent to the parser state in the strong sense --
// one may sit between a step and its table without ending the table, which is
// where the syntax document puts Uses.
bool SpectableParser::isNamedComment(const QString& firstWord)
{
    static const QStringList words = {
        "Description", "Details", "Constraint", "Notes", "Uses"
    };
    for (const QString& k : words)
        if (firstWord.startsWith(k, Qt::CaseInsensitive))
            return true;
    return false;
}

// Lines that appear inside any block and should be silently skipped
// WITHOUT changing the current parser state.
//
// Insert stays here rather than with the named comments above: it is a
// directive, not documentation, and it is consumed earlier when it stands in
// for a step table. Reaching this point means it was not in that position.
bool SpectableParser::isSkipKeyword(const QString& firstWord)
{
    static const QStringList words = { "Insert" };
    for (const QString& k : words)
        if (firstWord.startsWith(k, Qt::CaseInsensitive))
            return true;
    return false;
}

// Block-start keywords that end any open Attributes/Define/Step table
// BusinessRule / Calculation / DataType are handled explicitly now — not here
static bool isBlockStartKeyword(const QString& firstWord)
{
    static const QStringList words = { "DomainTerm", "ScenarioGroup" };
    for (const QString& k : words)
        if (firstWord.startsWith(k, Qt::CaseInsensitive))
            return true;
    return false;
}

// Every first word the dispatch in parseLines can act on.
//
// It exists for the line that is none of them. Such a line used to be reported
// after an open BusinessRule had already been closed for it, so a sentence
// somebody wrapped ended the rule, the Examples: table below it belonged to no
// rule, and the rule generated no test -- three warnings, the last of which was
// the only one about the damage. Asking this first means the block stays open,
// and the one message is about the line nobody can read.
//
// This must stay in step with the branches below; the predicates above cover
// the keywords handled before a block would be closed.
bool SpectableParser::isKnownKeyword(const QString& firstWord)
{
    QString w = firstWord;
    if (w.endsWith(':')) w.chop(1);        // Scenario: Background: Cleanup:
    static const QStringList words = {
        "Specification", "BusinessRule", "Calculation", "DataType",
        "Attributes", "Entity", "Collection", "Define",
        "Background", "Cleanup", "Scenario",
        "Given", "When", "Then", "And", "WhenThen"
    };
    for (const QString& k : words)
        if (w.compare(k, Qt::CaseInsensitive) == 0) return true;

    return isNamedComment(firstWord)
        || isSkipKeyword(firstWord)
        || isBlockStartKeyword(firstWord)
        || firstWord.compare("Import", Qt::CaseInsensitive) == 0
        || firstWord.startsWith("Examples", Qt::CaseInsensitive);
}

// ---------------------------------------------------------------------------
// CSV/TSV → pipe-table conversion
// ---------------------------------------------------------------------------

static QStringList parseCsvLine(const QString& line, QChar delim)
{
    QStringList result;
    QString field;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';  // escaped double-quote
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == delim) {
                result << field.trimmed();
                field.clear();
            } else {
                field += c;
            }
        }
    }
    result << field.trimmed();
    return result;
}

static QVector<QStringList> parseCsvRows(const QString& content, const QString& fname)
{
    QStringList lines = content.split('\n');
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    if (lines.isEmpty()) return {};

    QChar delim = ',';
    const QString ext = QFileInfo(fname).suffix().toLower();
    if (ext == "tsv") {
        delim = '\t';
    } else if (ext != "csv") {
        for (const QString& ln : lines) {
            if (ln.trimmed().isEmpty()) continue;
            if (ln.count('\t') > ln.count(',')) delim = '\t';
            break;
        }
    }

    QVector<QStringList> rows;
    for (const QString& ln : lines) {
        if (ln.trimmed().isEmpty()) continue;
        rows << parseCsvLine(ln, delim);
    }
    return rows;
}

static void validateCsvHeaders(const QVector<QStringList>& rows, const QString& fname,
                                const QStringList& expectedFields, QStringList& warnings)
{
    if (rows.isEmpty() || expectedFields.isEmpty()) return;
    for (const QString& hdr : rows[0]) {
        bool found = false;
        for (const QString& f : expectedFields)
            if (f.compare(hdr, Qt::CaseInsensitive) == 0) { found = true; break; }
        if (!found)
            warnings << QStringLiteral("Header '%1' in '%2' does not match any Attribute").arg(hdr, fname);
    }
}

static QString csvToTable(const QString& content, const QString& fname,
                           const QStringList& expectedFields, QStringList& warnings)
{
    const QVector<QStringList> rows = parseCsvRows(content, fname);
    if (rows.isEmpty()) return {};
    validateCsvHeaders(rows, fname, expectedFields, warnings);
    QString result;
    for (const QStringList& row : rows) {
        result += '|';
        for (const QString& cell : row)
            result += ' ' + cell + " |";
        result += '\n';
    }
    return result;
}

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------

SpectableFile SpectableParser::parse(const QString& filePath)
{
    QSet<QString> visited;
    return parseImpl(filePath, visited);
}

SpectableFile SpectableParser::parseText(const QString& text, const QString& filePath)
{
    QSet<QString> visited;
    visited.insert(QFileInfo(filePath).absoluteFilePath());
    return parseLines(text, filePath, visited);
}

SpectableFile SpectableParser::parseImpl(const QString& filePath, QSet<QString>& visited)
{
    const QString absPath = QFileInfo(filePath).absoluteFilePath();
    if (visited.contains(absPath)) return {};
    visited.insert(absPath);

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SpectableFile result;
        result.filePath = filePath;
        result.messages.push_back({ 0, "Cannot open file: " + filePath, false });
        return result;
    }
    return parseLines(QTextStream(&f).readAll(), filePath, visited);
}

SpectableFile SpectableParser::parseLines(const QString& text, const QString& filePath,
                                          QSet<QString>& visited)
{
    const QString absPath = QFileInfo(filePath).absoluteFilePath();

    SpectableFile result;
    result.filePath = filePath;

    QStringList lines = text.split('\n');

    enum class State {
        Top,
        InAttrDef,         // reading the field-definition pipe table
        InCollectionDef,   // reading the Collection header + data row
        InDefineTable,
        InDefineDocString, // collecting """ ... """ content for the current define
        InBackground,
        InCleanup,
        InScenario,
        AwaitStepTable,
        InStepTable,
        SkipTable,         // discard until blank or non-pipe line
        InNamedBlock,      // inside BusinessRule / Calculation / DataType header
        InExamplesTable,   // reading the Examples table for a named block
        InDocString        // collecting """ ... """ content for the current step
    };

    State       state       = State::Top;
    QString     lastKw;
    QStringList pendingTags;          // @Tag lines accumulate here until consumed by next block
    QStringList pendingGeneratorTags; // $Tag lines — generator-only, never passed to annotations
    int pendingGeneratorTagLine = 0;  // where they were written, to report one that marks nothing

    AttrSet*    curAttr       = nullptr;
    Collection* curCollection = nullptr;
    QStringList collectionHeaders;
    Define*     curDefine     = nullptr;
    Scenario*   curScen       = nullptr;
    Step*       curStep       = nullptr;
    NamedBlock* curNamedBlock = nullptr;
    // Where the next Uses line belongs. Uses is a named comment that follows the
    // element it documents, so it attaches to whatever was opened last -- an
    // Entity, a Collection, a Define, a named block, a Scenario or a step. One
    // pointer rather than a search through the cur* set, because those are
    // cleared at different times and "the element a Uses would land on" is
    // exactly what is wanted.
    QString*    curUses       = nullptr;
    QStringList attrHeaders;

    // A $tag is cleared by a blank line, which is the rule that lets a tag
    // belong to the block right below it. The cost is that a tag separated from
    // its block marks nothing at all, and nothing said so: the block is
    // generated as though it were untagged. That is silent in both directions
    // -- a block believed skipped is tested, and after an edit that inserts a
    // line, a block believed tested is skipped, which looks exactly like a
    // passing test. So say it where the tag was written.
    auto emitMsg = [&](int ln, const QString& msg, bool warn = false) {
        result.messages.push_back({ ln, msg, warn });
    };

    auto dropGeneratorTags = [&]() {
        if (!pendingGeneratorTags.isEmpty()) {
            QStringList shown;
            for (const QString& t : pendingGeneratorTags) shown << "$" + t;
            emitMsg(pendingGeneratorTagLine,
                    QString("%1 marks nothing -- a blank line ends a tag, so what follows is "
                            "generated as though it were untagged. Put the tag immediately "
                            "above the block it belongs to.").arg(shown.join(" ")),
                    true);
        }
        pendingGeneratorTags.clear();
        pendingGeneratorTagLine = 0;
    };

    bool inCleanupBlock = false; // true while collecting cleanup steps
    int  tableCols      = -1;    // column count of the table being read, or -1 between tables

    // The named comment a table may still attach to: set by a Description,
    // Details, Notes, Constraint or Uses line, kept through its indented
    // continuations and its rows, cleared by anything else. Points into the
    // vector of whatever the comment belongs to.
    QVector<NamedComment>* openComment = nullptr;

    // The named comment a leading-backslash line continues. A continuation has
    // to be on the very next line, so the line number is what identifies it:
    // a blank line or anything else in between breaks the chain without any
    // need to clear these.
    QVector<NamedComment>* contOwner = nullptr;
    int  contIndex = -1;
    int  contLine  = -1;
    bool contIsBlockDescription = false;

    // Where a named comment written now belongs.
    auto commentOwner = [&]() -> QVector<NamedComment>* {
        if (curNamedBlock && (state == State::InNamedBlock || state == State::InExamplesTable))
            return &curNamedBlock->comments;
        if (curAttr && state == State::InAttrDef) return &curAttr->comments;
        if (curStep) return &curStep->comments;
        if (curScen) return &curScen->comments;
        return &result.comments;
    };
    // Whether some block is waiting for the next pipe row as its own table,
    // in which case a named comment between them does not take it -- the
    // syntax document places Uses exactly there.
    auto blockAwaitsTable = [&]() {
        return state == State::InAttrDef || state == State::InCollectionDef
            || state == State::InDefineTable || state == State::AwaitStepTable
            || state == State::InStepTable || state == State::InExamplesTable
            || state == State::SkipTable;
    };

    // Top-level "Insert" of a .spectable file splices that file's own lines
    // in place (recursively re-parsed by this same loop), unlike Import,
    // which only pulls in AttrSets/Defines/named blocks by reference. Guards
    // against re-splicing the same file twice (e.g. a cycle of mutual Inserts).
    QSet<QString> insertedSpectableFiles;

    // A step that names an attribute set has said a table follows. When
    // nothing did -- no rows, no =Define -- the glue would be called with
    // nothing, so it is an error, said at the step.
    auto endStepTable = [&]() {
        if (state == State::AwaitStepTable && curStep && !curStep->hasTable
                && curStep->defineRef.isEmpty() && !curStep->attrSetName.isEmpty())
            emitMsg(curStep->line,
                    QString("Step '%1' names '%2' but no table follows -- add the table, "
                            "or =Name for a Define, or drop ': %2'")
                        .arg(curStep->text.trimmed(), curStep->attrSetName),
                    false);
        curStep = nullptr;
        if (curScen)          state = State::InScenario;
        else if (inCleanupBlock) state = State::InCleanup;
        else                  state = State::InBackground;
    };

    auto endAttrDef = [&]() {
        attrHeaders.clear();
        curAttr = nullptr;
        state   = State::Top;
    };

    auto endCollectionDef = [&]() {
        collectionHeaders.clear();
        curCollection = nullptr;
        state = State::Top;
    };

    // A bare "Define" followed by a table of Name | Value rows: each row is a
    // one-line Define, so a specification with a dozen constants can say them
    // once. Shares InDefineTable with the named table-form Define; curDefine
    // is null while a list is being read, and these tell the rows apart.
    bool        defineList = false;
    int         defineListLine     = 0;
    int         defineListNameCol  = -1;
    int         defineListValueCol = -1;

    auto endDefineDef = [&]() {
        curDefine  = nullptr;
        defineList = false;
        state      = State::Top;
    };

    auto endNamedBlock = [&]() {
        curNamedBlock = nullptr;
        state         = State::Top;
    };

    // Number of leading space/tab characters on a line (its indentation column).
    auto leadingWsCount = [](const QString& line) {
        int i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        return i;
    };

    // Append one line of docstring content, resolving Insert commands.
    // `lineNum` is captured by reference so it reflects the current loop variable.
    // `indentCols` is the column of the opening """ — up to that many leading
    // whitespace characters are stripped from literal content lines (structural
    // indentation from nesting inside the .spectable file), preserving any
    // indentation beyond that as intentional formatting within the docstring.
    static QRegularExpression reDocInsert(
        R"re(^\s*Insert\s+(?:"([^"]+)"|'([^']+)'|<([^>]+)>)\s*$)re",
        QRegularExpression::CaseInsensitiveOption);
    const QString baseDir = QFileInfo(absPath).absolutePath();

    // Every file an Insert names, wherever the Insert stands, so the index can
    // list them without reading the file again.
    auto noteInsert = [&](const QString& fullPath, int line) {
        if (!result.inserts.contains(fullPath)) result.inserts.push_back(fullPath);
        if (!result.insertLines.contains(line)) result.insertLines.push_back(line);
    };

    auto appendDocLine = [&](const QString& rawLine, QString& docStr, int lineNum, int indentCols) {
        const QString raw = rawLine.mid(qMin(indentCols, leadingWsCount(rawLine)));
        auto m = reDocInsert.match(raw);
        if (m.hasMatch()) {
            const QString fname = !m.captured(1).isEmpty() ? m.captured(1)
                                : !m.captured(2).isEmpty() ? m.captured(2)
                                                           : m.captured(3);
            const QString fullPath = QFileInfo(baseDir + "/" + fname).absoluteFilePath();
            noteInsert(fullPath, lineNum);
            QFile ins(fullPath);
            if (!ins.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emitMsg(lineNum, QString("Inserted file not found: '%1'").arg(fname), false);
                return;
            }
            QString content = QTextStream(&ins).readAll();
            const QString ext = QFileInfo(fname).suffix().toLower();
            if (ext == "csv" || ext == "tsv") {
                // Collect expected field names from the step's attrSet (if in step context)
                QStringList expectedFields;
                if (curStep && !curStep->attrSetName.isEmpty()) {
                    for (const AttrSet& as : result.attrSets)
                        if (as.name.compare(curStep->attrSetName, Qt::CaseInsensitive) == 0) {
                            for (const Field& fld : as.fields)
                                expectedFields << fld.name;
                            break;
                        }
                }
                QStringList warnings;
                docStr += csvToTable(content, fname, expectedFields, warnings);
                for (const QString& w : warnings)
                    emitMsg(lineNum, w, false);
            } else {
                if (!content.endsWith('\n')) content += '\n';
                docStr += content;
            }
        } else {
            docStr += raw + "\n";
        }
    };

    for (int idx = 0; idx < lines.size(); ++idx) {
        const int    lineNum  = idx + 1;
        const QString raw     = lines[idx];
        const QString trimmed = raw.trimmed();

        // A named comment's table has to follow it directly: a blank line or
        // any other line ends the chance. Its own indented continuations and
        // its rows keep it open. (The comment line itself sets it again below.)
        if (state != State::InDocString && state != State::InDefineDocString) {
            const bool continuation = (raw.startsWith(' ') || raw.startsWith('\t'))
                                      && !isPipeRow(trimmed) && !trimmed.isEmpty();
            if (!isPipeRow(trimmed) && !continuation) openComment = nullptr;
        }

        // ── DocString accumulation ────────────────────────────────────────────
        // Must come before blank/indentation checks so all content is captured.
        if (state == State::InDocString) {
            if (trimmed == "\"\"\"") {
                if (curStep) {
                    if (curStep->docString.endsWith('\n'))
                        curStep->docString.chop(1);
                    curStep->hasDocString     = true;
                    curStep->docStringEndLine = lineNum;
                }
                state   = curScen ? State::InScenario
                                  : (inCleanupBlock ? State::InCleanup : State::InBackground);
                curStep = nullptr;
            } else {
                if (curStep) appendDocLine(raw, curStep->docString, lineNum, curStep->docStringIndent);
            }
            continue;
        }

        // ── Define DocString accumulation ────────────────────────────────────
        if (state == State::InDefineDocString) {
            if (trimmed == "\"\"\"") {
                if (curDefine) {
                    if (curDefine->docString.endsWith('\n'))
                        curDefine->docString.chop(1);
                    curDefine->hasDocString = true;
                    curDefine->isTable = false;
                }
                curDefine = nullptr;
                state = State::Top;
            } else {
                if (curDefine) appendDocLine(raw, curDefine->docString, lineNum, curDefine->docStringIndent);
            }
            continue;
        }

        // ── DocString opener ──────────────────────────────────────────────────
        // """ on its own line immediately after a bare step (no attrSet, no table).
        if (trimmed == "\"\"\"" && curStep
                && curStep->attrSetName.isEmpty() && !curStep->hasTable
                && !curStep->hasDocString
                && (state == State::InScenario || state == State::InBackground
                    || state == State::InCleanup)) {
            curStep->docStringIndent = leadingWsCount(raw);
            state = State::InDocString;
            continue;
        }

        // ── Ragged tables ────────────────────────────────────────────────────
        // A run of pipe rows is one table, whatever block it belongs to, and
        // every row should have the columns the first one has. Blank lines do
        // not end the run; any other line does. A warning, because every reader
        // of a short row treats the missing cells as empty and carries on.
        if (isPipeRow(trimmed)) {
            const int cols = splitPipeRow(trimmed).size();
            if (cols >= 1) {
                if (tableCols < 0)
                    tableCols = cols;
                else if (cols != tableCols)
                    emitMsg(lineNum, QString("Table row has %1 column(s) but header has %2")
                                         .arg(cols).arg(tableCols), true);
            }
        } else if (!trimmed.isEmpty()) {
            tableCols = -1;
        }

        // ── Blank lines ──────────────────────────────────────────────────────
        if (trimmed.isEmpty()) {
            if (state == State::InStepTable || state == State::AwaitStepTable)
                endStepTable();
            if (state == State::SkipTable)
                state = State::Top;
            if (state == State::InCollectionDef)
                endCollectionDef();
            if (state == State::InExamplesTable) {
                if (curNamedBlock && !curNamedBlock->examples.rows.isEmpty())
                    curNamedBlock->hasExamples = true;
                endNamedBlock();
            }
            // InNamedBlock stays alive through blank lines (Description may precede Examples)
            pendingTags.clear();          // tags must immediately precede their block
            dropGeneratorTags();
            continue;
        }

        // ── Comments ─────────────────────────────────────────────────────────
        if (trimmed.startsWith('#'))
            continue;

        // ── Tags — accumulate for the next block ─────────────────────────────
        // @TagName reaches the generated test; $TagName is read by the generator
        // for filtering and never emitted. Both kinds may share a line. Each
        // sigil used to be read only on a line that started with it, so
        // "@smoke $ready" kept smoke and dropped ready silently -- a filter on
        // ready then skipped a scenario that carried it, which looks exactly
        // like a scenario that was never written.
        if (trimmed.startsWith('@') || trimmed.startsWith('$')) {
            static QRegularExpression reTag(R"(@(\w+))");
            static QRegularExpression reGenTag(R"(\$(\w+))");
            auto it = reTag.globalMatch(trimmed);
            while (it.hasNext()) pendingTags << it.next().captured(1);
            auto gi = reGenTag.globalMatch(trimmed);
            if (gi.hasNext() && pendingGeneratorTags.isEmpty())
                pendingGeneratorTagLine = lineNum;
            while (gi.hasNext()) pendingGeneratorTags << gi.next().captured(1);
            continue;
        }

        // ── Continuation / indented text ─────────────────────────────────────
        // Lines starting with whitespace that are not pipe rows are continuations
        // of Description/Details/etc. — always skip them.
        if ((raw.startsWith(' ') || raw.startsWith('\t')) && !isPipeRow(trimmed))
            continue;

        // ── Pipe rows ─────────────────────────────────────────────────────────
        if (isPipeRow(trimmed)) {
            QStringList cells = splitPipeRow(trimmed);

            // A table under a named comment is the comment's: documentation,
            // read by nothing. Unless a block is waiting for this row as its
            // own table, which wins -- "Given x : T" then "Uses ..." then a
            // table still gives the step its table.
            if (openComment && !blockAwaitsTable()) {
                if (!openComment->isEmpty()) openComment->last().rows.push_back(cells);
                continue;
            }

            switch (state) {
            case State::SkipTable:
                break; // discard

            case State::InCollectionDef:
                if (collectionHeaders.isEmpty()) {
                    collectionHeaders = cells;  // header row: DataType | Minimum | Maximum | Notes
                } else if (curCollection) {
                    for (int ci = 0; ci < collectionHeaders.size(); ++ci) {
                        const QString h = collectionHeaders[ci].toLower();
                        const QString v = (ci < cells.size()) ? cells[ci] : QString();
                        if      (h == "datatype") curCollection->elementType = v;
                        else if (h == "minimum")  curCollection->minimum     = v;
                        else if (h == "maximum")  curCollection->maximum     = v;
                        else if (h == "notes")    curCollection->notes       = v;
                    }
                    curCollection = nullptr;  // one data row per Collection
                }
                break;

            case State::InAttrDef:
                if (attrHeaders.isEmpty()) {
                    attrHeaders = cells;
                    if (curAttr) curAttr->headerLine = lineNum;
                    // Warn on unrecognised header columns
                    static const QStringList knownHeaders = {
                        "attribute", "name", "type", "datatype",
                        "default", "note", "notes", "in-out", "in/out", "multiples"
                    };
                    for (const QString& h : cells) {
                        if (h.compare("multiples", Qt::CaseInsensitive) == 0) {
                            ParseMessage pm;
                            pm.line    = lineNum;
                            pm.warning = true;
                            pm.text    = QStringLiteral(
                                "'Multiples' column is deprecated and no longer generates a "
                                "list-valued field — use the Collection keyword instead");
                            result.messages.push_back(pm);
                        } else if (!h.startsWith('#') && !knownHeaders.contains(h.toLower())) {
                            ParseMessage pm;
                            pm.line    = lineNum;
                            pm.warning = true;
                            pm.text    = QString("Unrecognized Attributes/Entity column header '%1' — will be ignored").arg(h);
                            result.messages.push_back(pm);
                        }
                    }
                } else if (curAttr) {
                    // Map cells to fields using header positions
                    Field fd;
                    fd.line = lineNum;
                    for (int ci = 0; ci < attrHeaders.size(); ++ci) {
                        const QString h = attrHeaders[ci].toLower();
                        const QString v = (ci < cells.size()) ? cells[ci] : QString();
                        if (h == "attribute" || h == "name") fd.name         = v;
                        else if (h == "type" || h == "datatype") fd.type     = v;
                        else if (h == "default")             fd.defaultValue = v;
                        else if (h == "notes")               fd.notes        = v;
                        else if (h == "in-out" || h == "in/out") fd.inOut   = v;
                        // "multiples" intentionally unmapped — deprecated, see header warning above
                    }
                    if (!fd.name.isEmpty() && !fd.name.startsWith('#'))
                        curAttr->fields.push_back(fd);
                }
                break;

            case State::InDefineTable:
                if (curDefine) {
                    curDefine->tableRows.push_back(cells);
                    if (curDefine->tableRows.size() == 1) {
                        QString h0 = cells.isEmpty() ? "" : cells[0].toLower();
                        curDefine->vertical = (h0 == "attribute" || h0 == "name");
                    }
                } else if (defineList) {
                    if (defineListNameCol < 0) {
                        // The header row says which column is which. Name and
                        // Value are required; anything else (Notes) is read past.
                        for (int ci = 0; ci < cells.size(); ++ci) {
                            const QString h = cells[ci].toLower();
                            if (h == "name")  defineListNameCol  = ci;
                            if (h == "value") defineListValueCol = ci;
                        }
                        if (defineListNameCol < 0 || defineListValueCol < 0) {
                            emitMsg(lineNum, "A Define table needs a Name column and a Value "
                                             "column -- one Define per row", false);
                            state = State::SkipTable;
                        }
                    } else {
                        const QString name  = defineListNameCol  < cells.size() ? cells[defineListNameCol]  : QString();
                        const QString value = defineListValueCol < cells.size() ? cells[defineListValueCol] : QString();
                        if (!name.isEmpty() && !name.startsWith('#')) {
                            Define def;
                            def.name        = name;
                            def.scalarValue = value;
                            def.isTable     = false;
                            def.line        = lineNum;
                            def.listLine    = defineListLine;
                            result.defines.push_back(def);
                        }
                    }
                }
                break;

            case State::AwaitStepTable: {
                if (!curStep) break;
                curStep->hasTable = true;
                state = State::InStepTable;
                // Orientation comes only from the explicit " : AttrSet Vertical"
                // clause (curStep->vertical, set from the step's colon clause
                // before any table rows arrive) — never guessed from the header
                // row's text. A header row that happens to start with "Name" or
                // "Attribute" (a very common field name) is still a normal
                // horizontal table unless "Vertical" was written explicitly.
                if (curStep->vertical) {
                    // Explicit Vertical: no header row, every row is an
                    // (Attribute, Value...) pair.
                    curStep->table.vertical  = true;
                    curStep->table.hasHeader = false;
                    curStep->table.rows.push_back(cells);
                    curStep->table.rowLines.push_back(lineNum);
                } else {
                    curStep->table.hasHeader = true;
                    curStep->table.rows.push_back(cells);
                    curStep->table.rowLines.push_back(lineNum);
                }
                break;
            }

            case State::InStepTable:
                if (curStep)
                    curStep->table.rows.push_back(cells);
                    curStep->table.rowLines.push_back(lineNum);
                break;

            case State::InExamplesTable:
                if (curNamedBlock) {
                    if (curNamedBlock->examples.header.isEmpty()) {
                        curNamedBlock->examples.header     = cells;  // first row = column headers
                        curNamedBlock->examples.headerLine = lineNum;
                        // Warn if headers don't match the built-in ValidValues/EnumerationValues columns
                        const QString asn = curNamedBlock->examples.attrSetName;
                        QStringList expectedCols;
                        if (asn.compare("ValidValues", Qt::CaseInsensitive) == 0)
                            expectedCols = { "value", "isvalid", "notes" };
                        else if (asn.compare("EnumerationValues", Qt::CaseInsensitive) == 0)
                            expectedCols = { "value", "notes" };
                        if (!expectedCols.isEmpty()) {
                            for (const QString& h : cells) {
                                if (!h.startsWith('#') && !expectedCols.contains(h.toLower())) {
                                    ParseMessage pm;
                                    pm.line    = lineNum;
                                    pm.warning = true;
                                    pm.text    = QString("Unrecognized %1 column '%2' — expected columns: %3")
                                                     .arg(asn, h, expectedCols.join(", "));
                                    result.messages.push_back(pm);
                                }
                            }
                        }
                    } else {
                        curNamedBlock->examples.rows.push_back(cells);
                        curNamedBlock->examples.rowLines.push_back(lineNum);
                    }
                }
                break;

            default:
                // A step that named no attribute set leaves the state machine in
                // the Scenario, so its table lands here. "No active block" is a
                // true but useless thing to say about it: there is an active
                // step, and the table is its own -- it simply has nothing to be
                // read as. Said once, at the step, rather than once per row.
                if (curStep && curStep->attrSetName.isEmpty()) {
                    if (!curStep->tableWithoutAttrSet) {
                        curStep->tableWithoutAttrSet = true;
                        curStep->orphanTableLine     = lineNum;
                        emitMsg(curStep->line,
                                QString("Step '%1' has a table but names no attribute set, so "
                                        "nothing reads the table and no argument reaches the "
                                        "glue -- add ': <Name>'").arg(curStep->text.trimmed()),
                                false);
                    }
                    break;
                }
                emitMsg(lineNum, "Unexpected table row (no active block)", true);
                break;
            }
            continue;
        }

        // ── Non-pipe, non-blank lines ─────────────────────────────────────────

        // =DefineName must be checked before endStepTable(), which clears curStep.
        {
            QString defName;
            if (isDefineLine(trimmed, defName)) {
                if (curStep && (state == State::InScenario || state == State::InBackground
                                || state == State::InCleanup
                                || state == State::AwaitStepTable)) {
                    curStep->defineRef     = defName;
                    curStep->defineRefLine = lineNum;
                    curStep->hasTable  = false;
                }
                if (state == State::InStepTable || state == State::AwaitStepTable)
                    endStepTable();
                continue;
            }
        }

        // Insert "file.csv" in step table position (AwaitStepTable)
        {
            auto mi = reDocInsert.match(trimmed);
            if (mi.hasMatch() && curStep && !curStep->attrSetName.isEmpty()
                    && state == State::AwaitStepTable) {
                const QString fname = !mi.captured(1).isEmpty() ? mi.captured(1)
                                    : !mi.captured(2).isEmpty() ? mi.captured(2)
                                                                : mi.captured(3);
                const QString ext = QFileInfo(fname).suffix().toLower();
                if (ext == "csv" || ext == "tsv") {
                    const QString fullPath = QFileInfo(baseDir + "/" + fname).absoluteFilePath();
                    noteInsert(fullPath, lineNum);
                    QFile ins(fullPath);
                    if (!ins.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        emitMsg(lineNum, QString("Inserted file not found: '%1'").arg(fname), false);
                    } else {
                        QStringList expectedFields;
                        for (const AttrSet& as : result.attrSets)
                            if (as.name.compare(curStep->attrSetName, Qt::CaseInsensitive) == 0) {
                                for (const Field& fld : as.fields) expectedFields << fld.name;
                                break;
                            }
                        const QVector<QStringList> rows = parseCsvRows(QTextStream(&ins).readAll(), fname);
                        QStringList warnings;
                        validateCsvHeaders(rows, fname, expectedFields, warnings);
                        for (const QString& w : warnings) emitMsg(lineNum, w, false);
                        curStep->table.rows      = rows;
                        curStep->table.hasHeader = !rows.isEmpty();
                        curStep->hasTable        = true;
                        curStep->defineRef.clear();
                    }
                    endStepTable();
                    continue;
                }
            }
        }

        // DocString opener for defines: """ immediately after Define Name (no rows yet)
        if (trimmed == "\"\"\"" && state == State::InDefineTable && curDefine
                && curDefine->tableRows.isEmpty()) {
            curDefine->docStringIndent = leadingWsCount(raw);
            state = State::InDefineDocString;
            continue;
        }

        // Keyword dispatch. Read before the step table is closed below, because
        // a named comment must not close it.
        const QString firstWord = trimmed.split(QRegularExpression(R"(\s+)")).first();

        // ── A continuation of the named comment above ───────────────────────
        // A line beginning with a backslash continues the named comment on the
        // line before it, so a long Description can be written over several
        // lines. Handled here, above the step-table reset, because a named
        // comment does not close a table and neither may its continuation.
        //
        // Nothing is done with the text yet beyond keeping it: a named comment
        // carries no behaviour. It is joined with a single space so that every
        // reader of a comment's text still sees one line.
        //
        // A backslash line that follows anything else continues nothing. It is
        // still treated as a comment -- discarding the line would be worse --
        // but it is reported, because the writer meant it to attach.
        if (isContinuation(trimmed)) {
            const QString contText = trimmed.mid(1).trimmed();
            const bool attaches = contOwner && contIndex >= 0
                               && contIndex < contOwner->size()
                               && contLine == lineNum - 1;
            if (!attaches) {
                emitMsg(lineNum, "Continuation line does not follow a named comment, "
                                 "so there is nothing for it to continue", true);
                continue;
            }
            NamedComment& nc = (*contOwner)[contIndex];
            if (!contText.isEmpty())
                nc.text = nc.text.isEmpty() ? contText : nc.text + ' ' + contText;
            // A named block's Description is read by Analyze, so it has to grow
            // with the comment rather than keep only the first line.
            if (contIsBlockDescription && curNamedBlock && !contText.isEmpty()
                    && (state == State::InNamedBlock || state == State::InExamplesTable)) {
                curNamedBlock->description = curNamedBlock->description.isEmpty()
                    ? contText : curNamedBlock->description + ' ' + contText;
            }
            contLine = lineNum;          // so a second backslash line continues too
            continue;
        }

        // ── Uses — a named comment, kept with the element it documents ──────
        // Documentation only: it never reaches a test as behaviour. It is kept
        // so that Analyze can show it and the generators can copy it into a
        // comment beside the code they write. A second Uses on the same element
        // is appended rather than dropped, so a long note can be written over
        // several lines.
        //
        // Handled here, above the step-table reset, so that
        //
        //     Given cart is : ShoppingCart
        //     Uses Initial cart setup
        //     | Orderer | Bill |
        //
        // still gives the step its table. Until 2026-09-13 it did not: any line
        // that was not a pipe row ended the table first, so the step lost its
        // table and the rows were reported as belonging to no block -- and that
        // is the very placement the syntax document shows.
        if (firstWord.compare("Uses", Qt::CaseInsensitive) == 0) {
            const QString text = trimmed.mid(firstWord.length()).trimmed();
            {
                QVector<NamedComment>* owner = commentOwner();
                owner->push_back({ "Uses", text, {}, lineNum });
                openComment = owner;
                contOwner = owner;
                contIndex = owner->size() - 1;
                contLine  = lineNum;
                contIsBlockDescription = false;
            }
            if (!curUses) {
                emitMsg(lineNum, "Uses comment does not follow anything it can "
                                 "describe, so it is ignored", true);
            } else if (text.isEmpty()) {
                emitMsg(lineNum, "Uses comment is empty", true);
            } else if (curUses->isEmpty()) {
                *curUses = text;
            } else {
                *curUses += ' ' + text;
            }
            continue;
        }

        // A named block's Description is kept: Analyze asks whether a
        // BusinessRule, Calculation or DataType has one. The other named
        // comments are not captured yet, but they are equally transparent --
        // none of them ends a table either.
        if (isNamedComment(firstWord)) {
            const QString text = trimmed.mid(firstWord.length()).trimmed();
            const bool isBlockDescription =
                firstWord.compare("Description", Qt::CaseInsensitive) == 0 && curNamedBlock
                && (state == State::InNamedBlock || state == State::InExamplesTable);
            if (isBlockDescription && !text.isEmpty())
                curNamedBlock->description = text;
            // Kept whole, whatever it belongs to, and open for a table.
            QVector<NamedComment>* owner = commentOwner();
            owner->push_back({ firstWord, text, {}, lineNum });
            openComment = owner;
            // A backslash on the next line continues this one.
            contOwner = owner;
            contIndex = owner->size() - 1;
            contLine  = lineNum;
            contIsBlockDescription = isBlockDescription;
            continue;
        }

        // End open step table
        if (state == State::InStepTable || state == State::AwaitStepTable)
            endStepTable();
        if (state == State::SkipTable)
            state = State::Top;

        // ── Import — follow and merge AttrSets / Defines ─────────────────────
        if (firstWord.compare("Import", Qt::CaseInsensitive) == 0) {
            static QRegularExpression reImp("Import\\s+\"([^\"]+)\"",
                                            QRegularExpression::CaseInsensitiveOption);
            auto im = reImp.match(trimmed);
            if (im.hasMatch()) {
                const QString imported = QFileInfo(
                    QFileInfo(absPath).absolutePath() + "/" + im.captured(1)).absoluteFilePath();
                // Said here, at the Import, rather than lost with the messages
                // of the file that could not be parsed. An error: everything
                // the file declares is missing from this one.
                result.imports.push_back(imported);
                result.importLines.push_back(lineNum);
                if (!QFileInfo::exists(imported)) {
                    emitMsg(lineNum, QString("Imported file not found: '%1'").arg(im.captured(1)), false);
                    continue;
                }
                SpectableFile imp = parseImpl(imported, visited);
                for (AttrSet as : imp.attrSets) {
                    as.imported = true;
                    result.attrSets.push_back(as);
                }
                for (Define def : imp.defines) {
                    def.imported = true;
                    result.defines.push_back(def);
                }
                for (NamedBlock nb : imp.namedBlocks) {
                    nb.isContext = true;
                    nb.imported  = true;
                    result.namedBlocks.push_back(nb);
                }
                for (const QString& dt : imp.dataTypeNames)
                    if (!result.dataTypeNames.contains(dt))
                        result.dataTypeNames.push_back(dt);
            }
            continue;
        }

        // ── Insert — top-level: splice a .spectable file's own declarations
        // in place, so its Entities/BusinessRules/Scenarios/etc. become part
        // of this file (re-processed by this same loop as if typed inline).
        // Unlike Import, which only pulls AttrSets/Defines in by reference,
        // this is a literal textual splice — closer to CSV Insert, but for
        // structural content instead of a table.
        if (firstWord.compare("Insert", Qt::CaseInsensitive) == 0) {
            auto insM = reDocInsert.match(trimmed);
            if (insM.hasMatch()) {
                const QString fname = !insM.captured(1).isEmpty() ? insM.captured(1)
                                    : !insM.captured(2).isEmpty() ? insM.captured(2)
                                                                   : insM.captured(3);
                if (QFileInfo(fname).suffix().compare("spectable", Qt::CaseInsensitive) == 0) {
                    const QString fullPath = QFileInfo(baseDir + "/" + fname).absoluteFilePath();
                    noteInsert(fullPath, lineNum);
                    if (insertedSpectableFiles.contains(fullPath)) {
                        // Already spliced once — skip re-insertion (also guards
                        // against an infinite loop from mutual/self Inserts).
                        continue;
                    }
                    insertedSpectableFiles.insert(fullPath);
                    QFile ins(fullPath);
                    if (!ins.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        emitMsg(lineNum, QString("Inserted file not found: '%1'").arg(fname), false);
                        continue;
                    }
                    QStringList insertedLines = QTextStream(&ins).readAll().split('\n');

                    // If this file already declares its own Specification
                    // (anywhere — order relative to this Insert doesn't
                    // matter), drop the inserted file's Specification line
                    // so the two don't collide as duplicate specifications.
                    static QRegularExpression reSpecLine(
                        R"(^\s*Specification\b)", QRegularExpression::CaseInsensitiveOption);
                    bool hostHasSpecification = false;
                    for (const QString& hl : lines)
                        if (reSpecLine.match(hl).hasMatch()) { hostHasSpecification = true; break; }
                    if (hostHasSpecification) {
                        QStringList filtered;
                        filtered.reserve(insertedLines.size());
                        for (const QString& il : insertedLines)
                            if (!reSpecLine.match(il).hasMatch())
                                filtered << il;
                        insertedLines = filtered;
                    }

                    lines.removeAt(idx);
                    for (int i = insertedLines.size() - 1; i >= 0; --i)
                        lines.insert(idx, insertedLines[i]);
                    --idx; // reprocess starting at the first spliced line
                    continue;
                }
            }
        }

        // ── Insert, anywhere other than in step-table position ───────────────
        // Nothing is read from it here, but a file that does not exist is
        // still a mistake in this line, and the one place to say so.
        if (isSkipKeyword(firstWord)) {
            auto insM = reDocInsert.match(trimmed);
            if (insM.hasMatch()) {
                const QString fname = !insM.captured(1).isEmpty() ? insM.captured(1)
                                    : !insM.captured(2).isEmpty() ? insM.captured(2)
                                                                   : insM.captured(3);
                const QString fullPath = QFileInfo(baseDir + "/" + fname).absoluteFilePath();
                noteInsert(fullPath, lineNum);
                if (!QFileInfo::exists(fullPath))
                    emitMsg(lineNum, QString("Inserted file not found: '%1'").arg(fname), false);
            }
            continue;
        }

        // ── Examples: — captured if inside a named block; otherwise discarded ──
        if (firstWord.startsWith("Examples", Qt::CaseInsensitive)) {
            if (state == State::InAttrDef) endAttrDef();
            if (state == State::InNamedBlock && curNamedBlock) {
                QString rest = trimmed.mid(firstWord.length()).trimmed();
                if (rest.startsWith(':')) rest = rest.mid(1).trimmed();
                curNamedBlock->examples.attrSetName = rest;
                curNamedBlock->examples.line        = lineNum;
                state = State::InExamplesTable;
            } else {
                state = State::SkipTable;
            }
            continue;
        }

        // ── A line whose first word is no keyword at all ──────────────────
        //
        // Asked before a named block would be closed for it, and an error
        // rather than a warning: a line nobody can classify is not something to
        // carry on past. A wrapped sentence inside a BusinessRule used to end
        // the rule and leave its Examples: table belonging to nothing, which
        // generated no test and said so only in the third of three warnings.
        // Prose that carries on the comment above it starts with a backslash
        // (syntax 4.1.1); this line does not, so it is read as neither.
        if (!isKnownKeyword(firstWord)) {
            pendingTags.clear();
            dropGeneratorTags();
            emitMsg(lineNum,
                    QString("'%1' is not a keyword -- a misspelled one, or a wrapped line "
                            "that needs a leading '\\' to carry on the comment above it. "
                            "The line is dropped, and nothing it was part of ends here.")
                        .arg(firstWord),
                    false);
            continue;
        }

        // ── End InNamedBlock / InExamplesTable on any other non-skip keyword ──
        if (state == State::InNamedBlock || state == State::InExamplesTable) {
            if (state == State::InExamplesTable && curNamedBlock
                    && !curNamedBlock->examples.rows.isEmpty())
                curNamedBlock->hasExamples = true;
            endNamedBlock();
            // state is now Top — continue processing this keyword normally
        }

        // ── BusinessRule / Calculation / DataType — create a named block ───────
        if (firstWord.compare("BusinessRule", Qt::CaseInsensitive) == 0 ||
            firstWord.compare("Calculation",  Qt::CaseInsensitive) == 0 ||
            firstWord.compare("DataType",     Qt::CaseInsensitive) == 0) {
            if (state == State::InAttrDef)       endAttrDef();
            if (state == State::InCollectionDef)  endCollectionDef();
            if (state == State::InDefineTable)    endDefineDef();
            curScen = nullptr; curStep = nullptr;
            QString kind;
            if      (firstWord.compare("BusinessRule", Qt::CaseInsensitive) == 0) kind = "BusinessRule";
            else if (firstWord.compare("Calculation",  Qt::CaseInsensitive) == 0) kind = "Calculation";
            else                                                                    kind = "DataType";
            NamedBlock nb;
            nb.kind = kind;
            nb.name = trimmed.mid(firstWord.length()).trimmed();
            nb.line = lineNum;
            nb.tags = pendingTags; pendingTags.clear();
            nb.generatorTags = pendingGeneratorTags; pendingGeneratorTags.clear();
            if (kind == "DataType" && !nb.name.isEmpty())
                result.dataTypeNames.push_back(nb.name);
            result.namedBlocks.push_back(nb);
            curNamedBlock = &result.namedBlocks.last();
            curUses = &curNamedBlock->uses;
            state = State::InNamedBlock;
            continue;
        }

        // ── Block-start keywords (DomainTerm, ScenarioGroup) ──────────────────
        if (isBlockStartKeyword(firstWord)) {
            if (state == State::InAttrDef)    endAttrDef();
            if (state == State::InDefineTable) endDefineDef();
            curScen = nullptr;
            curStep = nullptr;
            state   = State::Top;

            // "DomainTerm Roll : Pins" -- recorded rather than merely skipped,
            // so that a field declaring the type Roll can be resolved to Pins
            // before any generator sees it. Until 2026-09-16 the parser threw
            // this line away and every generator emitted a field of a type
            // nothing writes.
            //
            // A term with no ": Type" is still a declared name -- it is what
            // the index lists and what a duplicate is checked against -- so it
            // is recorded with an empty type, which resolves nothing.
            if (firstWord.compare("DomainTerm", Qt::CaseInsensitive) == 0) {
                static QRegularExpression reTerm(
                    R"(^DomainTerm\s+(\w+)(?:\s*:\s*(\w+))?\s*$)",
                    QRegularExpression::CaseInsensitiveOption);
                const auto mt = reTerm.match(trimmed);
                if (mt.hasMatch()) {
                    DomainTerm dt;
                    dt.name = mt.captured(1);
                    dt.type = mt.captured(2);
                    dt.line = lineNum;
                    result.domainTerms.push_back(dt);
                }
            } else if (firstWord.compare("ScenarioGroup", Qt::CaseInsensitive) == 0) {
                QString name = trimmed.mid(firstWord.length()).trimmed();
                if (name.startsWith(':')) name = name.mid(1).trimmed();
                if (!name.isEmpty()) result.scenarioGroups.push_back({ name, lineNum });

                // A ScenarioGroup organises scenarios and is generated into
                // nothing, so a tag on one has no test to land on. Tags above
                // it used to pass over and attach to the group's first
                // scenario -- one scenario of several, chosen by position,
                // with nothing said about it. They are dropped here instead,
                // and said.
                if (!pendingTags.isEmpty() || !pendingGeneratorTags.isEmpty()) {
                    QStringList shown;
                    for (const QString& t : pendingTags)          shown << "@" + t;
                    for (const QString& t : pendingGeneratorTags) shown << "$" + t;
                    emitMsg(lineNum,
                            QString("%1 on a ScenarioGroup is ignored -- a ScenarioGroup "
                                    "generates no test, so there is nothing for a tag to "
                                    "mark. Tag the scenarios in the group instead.")
                                .arg(shown.join(" ")),
                            true);
                    pendingTags.clear();
                    pendingGeneratorTags.clear();
                    pendingGeneratorTagLine = 0;
                }
            }
            continue;
        }

        // ── Terminate open Attributes/Collection/Define table on any other keyword ────
        if (state == State::InAttrDef)      endAttrDef();
        if (state == State::InCollectionDef) endCollectionDef();
        if (state == State::InDefineTable)   endDefineDef();

        // ── Parsed keywords ───────────────────────────────────────────────────

        // Specification
        if (firstWord.compare("Specification", Qt::CaseInsensitive) == 0) {
            result.specName       = trimmed.mid(firstWord.length()).trimmed();
            result.specLine       = lineNum;
            result.tags           = pendingTags;          pendingTags.clear();
            result.generatorTags  = pendingGeneratorTags; pendingGeneratorTags.clear();
            continue;
        }

        // Attributes / Entity
        if (firstWord.compare("Attributes", Qt::CaseInsensitive) == 0 ||
            firstWord.compare("Entity",     Qt::CaseInsensitive) == 0) {
            curScen = nullptr; curStep = nullptr;
            AttrSet as;
            as.kind = firstWord[0].toUpper() + firstWord.mid(1).toLower();
            as.name = trimmed.mid(firstWord.length()).trimmed();
            as.line = lineNum;
            result.attrSets.push_back(as);
            curAttr = &result.attrSets.last();
            curUses = &curAttr->uses;
            attrHeaders.clear();
            state = State::InAttrDef;
            continue;
        }

        // Collection
        if (firstWord.compare("Collection", Qt::CaseInsensitive) == 0) {
            curScen = nullptr; curStep = nullptr;
            Collection col;
            col.name = trimmed.mid(firstWord.length()).trimmed();
            col.line = lineNum;
            result.collections.push_back(col);
            curCollection = &result.collections.last();
            curUses = &curCollection->uses;
            collectionHeaders.clear();
            state = State::InCollectionDef;
            continue;
        }

        // Define
        if (firstWord.compare("Define", Qt::CaseInsensitive) == 0) {
            curScen = nullptr; curStep = nullptr;
            static QRegularExpression reDef(R"(^Define\s+(\w+)\s*(?:=\s*(.*))?$)",
                QRegularExpression::CaseInsensitiveOption);
            // "Define" alone: a table of one-line Defines follows.
            if (trimmed.compare("Define", Qt::CaseInsensitive) == 0) {
                curDefine          = nullptr;
                curUses            = nullptr;
                defineList         = true;
                defineListLine     = lineNum;
                defineListNameCol  = -1;
                defineListValueCol = -1;
                state              = State::InDefineTable;
                continue;
            }
            auto dm = reDef.match(trimmed);
            if (dm.hasMatch()) {
                Define def;
                def.name = dm.captured(1);
                def.line = lineNum;
                QString afterEq = dm.captured(2).trimmed();
                // A trailing comment is not part of the value. "Define TBR = -1
                // # Roll has not occurred" holds -1, not "-1  # Roll has not
                // occurred" -- which is what every =TBR used to expand to, and
                // nobody had ever written one. A # with no space before it is
                // kept, so a value like #123 survives.
                const int hash = afterEq.indexOf(" #");
                if (hash >= 0) afterEq = afterEq.left(hash).trimmed();
                if (!afterEq.isEmpty()) {
                    def.scalarValue = afterEq;
                    def.isTable     = false;
                    result.defines.push_back(def);
                    curDefine = nullptr;
                    curUses   = &result.defines.last().uses;
                    state     = State::Top;
                } else {
                    def.isTable = true;
                    result.defines.push_back(def);
                    curDefine = &result.defines.last();
                    curUses   = &curDefine->uses;
                    state     = State::InDefineTable;
                }
            }
            continue;
        }

        // Background
        if (firstWord.compare("Background", Qt::CaseInsensitive) == 0 ||
            trimmed.startsWith("Background:", Qt::CaseInsensitive)) {
            result.backgroundLines.push_back(lineNum);
            curScen = nullptr; curStep = nullptr; lastKw = {};
            inCleanupBlock = false;
            state = State::InBackground;
            continue;
        }

        // Cleanup
        if (firstWord.compare("Cleanup", Qt::CaseInsensitive) == 0 ||
            trimmed.startsWith("Cleanup:", Qt::CaseInsensitive)) {
            result.cleanupLines.push_back(lineNum);
            curScen = nullptr; curStep = nullptr; lastKw = {};
            inCleanupBlock = true;
            state = State::InCleanup;
            continue;
        }

        // Scenario / Scenario: (Gherkin-style colon)
        if (firstWord.compare("Scenario", Qt::CaseInsensitive) == 0 ||
            firstWord.compare("Scenario:", Qt::CaseInsensitive) == 0) {
            curStep = nullptr; lastKw = {};
            Scenario sc;
            QString nameRaw = trimmed.mid(firstWord.length()).trimmed();
            if (nameRaw.startsWith(':')) nameRaw = nameRaw.mid(1).trimmed();
            sc.name = nameRaw;
            sc.line = lineNum;
            sc.tags = pendingTags; pendingTags.clear();
            sc.generatorTags = pendingGeneratorTags; pendingGeneratorTags.clear();
            result.scenarios.push_back(sc);
            curScen = &result.scenarios.last();
            curUses = &curScen->uses;
            state   = State::InScenario;
            continue;
        }

        // Steps (Given / When / Then / And / WhenThen). "But" is not a keyword
        // and never was -- this comment used to list it, which is the only place
        // it has ever appeared.
        if (state == State::InScenario || state == State::InBackground
                                       || state == State::InCleanup) {
            QString kw, text, attrSet, badModifier;
            bool    trans = false, cmpOnly = false, everyCellMod = false;
            if (isStepLine(trimmed, kw, text, attrSet, trans, cmpOnly,
                           everyCellMod, badModifier)) {
                if (!badModifier.isEmpty())
                    emitMsg(lineNum,
                            QString("Unrecognized step modifier '%1' -- expected "
                                    "CompareOnly, EveryCell or Vertical").arg(badModifier),
                            true);
                // A Cleanup block runs after the Scenario to put things back, so
                // it asserts and tidies -- Then and And. A Given or a When there
                // would be arranging or acting after the test is over, which is
                // a Scenario's job. Checked on the raw keyword, before And is
                // normalised to whatever preceded it.
                if (state == State::InCleanup
                    && (kw.compare("Given", Qt::CaseInsensitive) == 0
                     || kw.compare("When", Qt::CaseInsensitive) == 0
                     || kw.compare("WhenThen", Qt::CaseInsensitive) == 0)) {
                    emitMsg(lineNum,
                            QString("Cleanup may only contain Then and And steps, and this "
                                    "is a %1 -- arranging or acting belongs in a Scenario")
                                .arg(kw),
                            false);
                }

                // And continues the run above it, so there has to be one. First
                // in its block there is nothing to continue: normalizeKeyword
                // reads it as a Given, which is a choice the file never made.
                // Say so, rather than letting it read as though it had.
                if (kw.compare("And", Qt::CaseInsensitive) == 0 && lastKw.isEmpty()) {
                    const QString blockName =
                        state == State::InBackground ? QStringLiteral("Background")
                      : state == State::InCleanup    ? QStringLiteral("Cleanup")
                                                     : QStringLiteral("Scenario");
                    emitMsg(lineNum,
                            QString("'And' continues the step above it, and this is the "
                                    "first step of the %1 -- it is read as a Given. Write "
                                    "the keyword you mean.").arg(blockName),
                            true);
                }

                lastKw = normalizeKeyword(kw, lastKw);
                Step st;
                st.keyword      = lastKw;
                st.text         = text;
                st.attrSetName  = attrSet;
                st.vertical   = trans;
                st.compareOnly  = cmpOnly;
                st.everyCell    = everyCellMod;
                st.line         = lineNum;

                if (state == State::InScenario && curScen) {
                    curScen->steps.push_back(st);
                    curStep = &curScen->steps.last();
                    curUses = &curStep->uses;
                } else if (inCleanupBlock) {
                    result.cleanupSteps.push_back(st);
                    curStep = &result.cleanupSteps.last();
                    curUses = &curStep->uses;
                } else {
                    result.backgroundSteps.push_back(st);
                    curStep = &result.backgroundSteps.last();
                    curUses = &curStep->uses;
                }

                State nextState = attrSet.isEmpty()
                    ? (curScen ? State::InScenario
                               : (inCleanupBlock ? State::InCleanup : State::InBackground))
                    : State::AwaitStepTable;
                state = nextState;
                continue;
            }
        }

        // A keyword in a position where it cannot be used. A first word that is
        // no keyword at all was dealt with further up, so what reaches here is a
        // step with nothing open to put it in, or a keyword with nothing after
        // it.
        pendingTags.clear();
        dropGeneratorTags();
        {
            QString kw2, text2, attrSet2, badMod2;
            bool v2 = false, c2 = false, e2 = false;
            const bool isStep = isStepLine(trimmed, kw2, text2, attrSet2,
                                           v2, c2, e2, badMod2);
            emitMsg(lineNum,
                    isStep ? QString("'%1' is a step, and no Scenario, Background or "
                                     "Cleanup is open for it to belong to").arg(firstWord)
                           : QString("'%1' is a keyword with nothing after it")
                                 .arg(firstWord),
                    false);
        }
    }

    // A $tag at the end of the file, with no block after it.
    dropGeneratorTags();

    // A step still waiting for its table when the file ends.
    if (state == State::AwaitStepTable) endStepTable();

    return result;
}

// ---------------------------------------------------------------------------
// Step tables against the attribute sets they name
// ---------------------------------------------------------------------------
//
// This lived in JavaGenerator, which meant the other eight languages generated a
// specification Java refused, and Analyze had to reimplement it to say anything
// before the build. One reading, shared: main.cpp calls this after merging
// context files, and the IDE's Analyze calls it on the file it just parsed.
//
// A Vertical table is a horizontal one transposed -- the attribute names run
// down the first column instead of across the header, every further column is
// another instance, and nothing else differs. So the same missing and unknown
// attributes are the same mistakes either way, and get the same message. They
// used to be checked in the horizontal case alone.
//
// An attribute set the file cannot see is skipped rather than reported: the
// caller may not have merged its declaration yet, and an unknown name is the
// caller's own check to make.

// ---------------------------------------------------------------------------
// DomainTerm resolution, and field types that resolve to nothing
// ---------------------------------------------------------------------------

void resolveDomainTermTypes(SpectableFile& file)
{
    if (file.domainTerms.isEmpty()) return;

    QMap<QString, QString> byName;
    for (const DomainTerm& dt : file.domainTerms) {
        if (dt.name.isEmpty() || dt.type.isEmpty()) continue;
        // A term named like a built-in type is reported by Analyze; it must not
        // also take the built-in over. "DomainTerm Integer : String" once turned
        // every Integer field in the project into a String, silently.
        if (isBuiltinDataType(dt.name)) continue;
        byName.insert(dt.name.toLower(), dt.type);
    }

    for (AttrSet& as : file.attrSets) {
        for (Field& f : as.fields) {
            const QString declared = f.type.trimmed();
            if (declared.isEmpty()) continue;

            // One hop only. A term standing for another term is a chain nobody
            // has asked for, and following it would need a cycle check for a
            // case that has never arisen.
            const QString resolved = byName.value(declared.toLower());
            if (!resolved.isEmpty()) f.type = resolved;
        }
    }
}

void resolveDefineReferences(SpectableFile& file)
{
    if (file.defines.isEmpty()) return;

    // Only what one cell can hold. A table-form Define stays out of this map, so
    // a cell naming one is left as written and reported elsewhere.
    QMap<QString, QString> scalar;
    for (const Define& d : file.defines) {
        if (d.isTable) continue;
        scalar.insert(d.name.toLower(), d.hasDocString ? d.docString : d.scalarValue);
    }
    if (scalar.isEmpty()) return;

    auto expand = [&](QString& cell) {
        const QString c = cell.trimmed();
        if (!c.startsWith('=')) return;
        const QString name = c.mid(1).trimmed().toLower();
        if (scalar.contains(name)) cell = scalar.value(name);
    };

    // Default columns. A default of =TBR is the same idea as a cell of =TBR: the
    // author wants the value the Define holds, not the text "=TBR".
    for (AttrSet& as : file.attrSets)
        for (Field& f : as.fields)
            expand(f.defaultValue);

    // Examples tables, on every kind of named block.
    for (NamedBlock& nb : file.namedBlocks)
        for (QStringList& row : nb.examples.rows)
            for (QString& cell : row)
                expand(cell);
}

QVector<ParseMessage> validateFieldTypes(const SpectableFile& file)
{
    QVector<ParseMessage> msgs;

    auto known = [&](const QString& type) {
        const QString t = type.trimmed().toLower();
        if (t.isEmpty() || isBuiltinDataType(t)) return true;
        for (const QString& dt : file.dataTypeNames)
            if (dt.compare(type, Qt::CaseInsensitive) == 0) return true;
        for (const AttrSet& as : file.attrSets)
            if (as.name.compare(type, Qt::CaseInsensitive) == 0) return true;
        for (const Collection& c : file.collections)
            if (c.name.compare(type, Qt::CaseInsensitive) == 0) return true;
        // A DomainTerm should already have been resolved away; accept one here
        // so that calling this without resolveDomainTermTypes first does not
        // produce a confusing error about a term that is perfectly legal.
        for (const DomainTerm& dt : file.domainTerms)
            if (dt.name.compare(type, Qt::CaseInsensitive) == 0) return true;
        return false;
    };

    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;
        for (const Field& f : as.fields) {
            if (known(f.type)) continue;

            ParseMessage m;
            m.line    = f.line;
            m.warning = false;
            m.text    = QString(
                "Attribute '%1' of '%2' has type '%3', which is not a built-in, a "
                "DataType, an Entity or Attributes block, a Collection, or a "
                "DomainTerm. The generated class would declare a field of a type "
                "that does not exist").arg(f.name, as.name, f.type.trimmed());
            msgs.push_back(m);
        }
    }
    return msgs;
}

// ---------------------------------------------------------------------------
// One misspelled heading, said once
// ---------------------------------------------------------------------------
//
// One misspelled heading produces two complaints about one mistake: a column
// nobody recognises, and an attribute nobody filled. When the two are the same
// word written differently -- a space, a capital, a hyphen -- they are one
// finding, and saying it once while naming both spellings is the whole fix the
// reader needs. A typo is the same mistake as well: AssumedHere typed as
// "Asumed Hree" is this column misspelled and not a column of its own, so it
// is paired too, by how far apart the two spellings are. Anything further off
// than that keeps its own message, because then they really are two problems.
static QString spellingKey(const QString& s)
{
    QString out;
    for (const QChar& c : s)
        if (c.isLetterOrNumber()) out += c.toLower();
    return out;
}

// How far apart two spellings are, counted in single-character edits
// (Levenshtein, two rows of it). The names are short and this runs only for a
// heading that matched no attribute at all, so the shape of it does not matter.
static int spellingDistance(const QString& a, const QString& b)
{
    const int n = int(a.size()), m = int(b.size());
    if (n == 0) return m;
    if (m == 0) return n;
    QVector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[j] = qMin(qMin(cur[j - 1] + 1, prev[j] + 1), prev[j - 1] + cost);
        }
        prev = cur;
    }
    return prev[m];
}

// Pairs each unrecognised heading with the attribute it misspells, reports the
// pair once, and takes both out of the lists, so the caller's own messages are
// left with the headings and attributes that really are on their own.
//
// A pair is allowed when the two spelling keys are equal -- the same word
// written differently -- or when they are within a budget that grows with the
// name: one edit at four letters, three at eleven. That pairs "Asumed Hree"
// with AssumedHere and leaves "Notes" and "Amount" apart, which matters,
// because a budget generous enough to join those would turn one clear
// complaint into a confident wrong one.
//
// Three cases are left as two messages on purpose. A name of three letters or
// fewer must match exactly, because at that length one edit reaches a word that
// was never meant. A heading equally close to two attributes names neither:
// nothing here knows which was meant, and two plain messages say more than a
// guess would. And a heading that differs from an attribute only in its digits
// is a different attribute -- Addend3 where Addend1 was declared is not
// Addend1 misspelled, because a number in a name says which one, not how it is
// spelled.
static bool differsOnlyInDigits(const QString& a, const QString& b)
{
    QString letters[2], digits[2];
    const QString* in[2] = { &a, &b };
    for (int i = 0; i < 2; ++i)
        for (const QChar& c : *in[i])
            (c.isDigit() ? digits[i] : letters[i]) += c;
    return digits[0] != digits[1] && letters[0] == letters[1]
        && !digits[0].isEmpty() && !digits[1].isEmpty();
}

static void pairMisspelledHeadings(const QString& tableWord, const QString& setName,
                                   int line, QStringList& unknownCols,
                                   QStringList& unfilled, QVector<ParseMessage>& msgs)
{
    for (int c = 0; c < unknownCols.size();) {
        const QString colKey = spellingKey(unknownCols[c]);
        int best = -1, bestDist = 0, tied = 0;
        for (int f = 0; f < unfilled.size(); ++f) {
            const QString fieldKey = spellingKey(unfilled[f]);
            const int dist   = spellingDistance(colKey, fieldKey);
            const int longer = int(qMax(colKey.size(), fieldKey.size()));
            const int budget = longer < 4 ? 0 : 1 + longer / 4;
            if (dist > budget) continue;
            if (dist > 0 && differsOnlyInDigits(colKey, fieldKey)) continue;
            if (best < 0 || dist < bestDist) { best = f; bestDist = dist; tied = 1; }
            else if (dist == bestDist)       { ++tied; }
        }
        if (best < 0 || (tied > 1 && bestDist > 0)) { ++c; continue; }

        ParseMessage m;
        m.line    = line;
        m.warning = false;
        m.text    = QString("%1 for '%2' has column '%3' where attribute '%4' is expected "
                            "-- a heading must match the attribute name exactly, so the column "
                            "is ignored and '%4' is left with no value")
                        .arg(tableWord, setName, unknownCols[c], unfilled[best]);
        msgs.push_back(m);
        unknownCols.removeAt(c);
        unfilled.removeAt(best);
    }
}

// ---------------------------------------------------------------------------
//
// Moved here from Analyze on 2026-09-16, so that the converter checks a cell
// the same way the editor does. A user DataType, Entity or Collection is not
// checked -- its own constructor decides what it accepts, in each language.

static void validateValue(int line, const QString& value, const QString& dtype,
                          QVector<ParseMessage>& out)
{
    if (dtype.isEmpty()) return;

    static const QRegularExpression reInteger (R"(^-?\d+$)");
    static const QRegularExpression reFloat   (R"(^-?\d+(\.\d+)?([eE][+-]?\d+)?$)");
    // Decimal has no exponent, and no thousands separator: every generator reads
    // it with its language's exact-decimal type, and none of them accept a comma.
    // "25,200.00" in a Decimal column used to reach BigDecimal and throw there.
    static const QRegularExpression reDecimal (R"(^[+-]?(\d+(\.\d*)?|\.\d+)$)");
    static const QRegularExpression reDate    (R"(^\d{4}-\d{2}-\d{2}$)");
    static const QRegularExpression reTime    (R"(^\d{2}:\d{2}(:\d{2})?$)");
    static const QRegularExpression reDateTime(R"(^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2})");

    auto warn = [&](const QString& text) {
        ParseMessage m;
        m.line    = line;
        m.warning = true;
        m.text    = text;
        out.push_back(m);
    };

    const QString ltype = dtype.toLower();
    if (ltype == "integer") {
        if (!reInteger.match(value).hasMatch())
            warn(QString("'%1' is not a valid Integer").arg(value));
    } else if (ltype == "float") {
        if (!reFloat.match(value).hasMatch())
            warn(QString("'%1' is not a valid Float").arg(value));
    } else if (ltype == "decimal" || ltype == "scientific") {
        // Scientific is the one numeric type that may carry an exponent.
        const bool ok = (ltype == "scientific") ? reFloat.match(value).hasMatch()
                                                : reDecimal.match(value).hasMatch();
        if (!ok)
            warn(QString("'%1' is not a valid %2").arg(value, dtype));
    } else if (ltype == "boolean") {
        static const QStringList valid{"true", "false"};
        if (!valid.contains(value.toLower()))
            warn(QString("'%1' is not a valid Boolean (use true/false)").arg(value));
    } else if (ltype == "yesno") {
        static const QStringList valid{"y", "n", "yes", "no", "t", "f", "true", "false"};
        if (!valid.contains(value.toLower()))
            warn(QString("'%1' is not a valid YesNo value").arg(value));
    } else if (ltype == "date") {
        if (!reDate.match(value).hasMatch())
            warn(QString("'%1' is not a valid Date (use YYYY-MM-DD)").arg(value));
    } else if (ltype == "time") {
        if (!reTime.match(value).hasMatch())
            warn(QString("'%1' is not a valid Time (use HH:MM or HH:MM:SS)").arg(value));
    } else if (ltype == "datetime") {
        if (!reDateTime.match(value).hasMatch())
            warn(QString("'%1' is not a valid DateTime").arg(value));
    }
}

bool isValidValueForType(const QString& value, const QString& type)
{
    QVector<ParseMessage> msgs;
    validateValue(0, value, type, msgs);
    return msgs.isEmpty();
}

// A cell that states no value to check: blank, a Define reference that is
// resolved later, or the do-not-care marker.
static bool statesNoValue(const QString& cell)
{
    return cell.isEmpty() || cell.startsWith('=') || cell == "?DNC?";
}

// The attribute sets by name, the file's own declaration winning over a
// sibling's of the same name -- a duplicate is reported elsewhere, and the
// one in this file is the one this file's tables are written against.
static QMap<QString, const AttrSet*> attrSetsByName(const SpectableFile& file)
{
    QMap<QString, const AttrSet*> byName;
    for (const AttrSet& as : file.attrSets)
        if (!byName.contains(as.name.toLower()))
            byName.insert(as.name.toLower(), &as);
    return byName;
}

static const Field* fieldOf(const AttrSet& as, const QString& name)
{
    for (const Field& f : as.fields)
        if (f.name.compare(name, Qt::CaseInsensitive) == 0) return &f;
    return nullptr;
}

static QString fieldTypeOf(const AttrSet& as, const QString& name)
{
    const Field* f = fieldOf(as, name);
    return f ? f->type.trimmed() : QString();
}

QVector<ParseMessage> validateStepTables(const SpectableFile& file)
{
    QVector<ParseMessage> msgs;

    const QMap<QString, const AttrSet*> byName = attrSetsByName(file);

    auto check = [&](const Step& step) {
        if (!step.hasTable || step.attrSetName.isEmpty()) return;

        // An EveryCell table is a grid of values, not a table of named columns,
        // so there are no headers to match against the attribute set. One thing
        // is worth checking though: a cell may be written =Name, and a Define
        // that holds a table is rows rather than one value, so there is nothing
        // to hand the type's fromText. Left alone it fails quietly -- resolveValue
        // skips a table Define, the cell keeps the literal text "=Name", and
        // fromText makes an object out of that.
        if (step.everyCell) {
            for (const QStringList& row : step.table.rows) {
                for (const QString& cell : row) {
                    const QString c = cell.trimmed();
                    if (!c.startsWith('=')) continue;
                    const QString name = c.mid(1).trimmed();
                    for (const Define& d : file.defines) {
                        if (d.name.compare(name, Qt::CaseInsensitive) != 0) continue;
                        if (!d.isTable) break;

                        ParseMessage m;
                        m.line    = step.line;
                        m.warning = false;
                        m.text    = QString(
                            "Define '%1' holds a table, so it cannot fill a cell of an "
                            "EveryCell grid -- a cell holds one value, the text form of "
                            "'%2'. Use a scalar or docstring Define, or write the value "
                            "in the cell").arg(name, step.attrSetName);
                        msgs.push_back(m);
                        break;
                    }
                }
            }
            return;
        }

        if (step.table.rows.isEmpty()) return;

        const AttrSet* as = byName.value(step.attrSetName.toLower(), nullptr);
        if (!as || as->fields.isEmpty()) return;

        // The names the table gives, whichever way round it is written.
        QStringList named;
        if (step.table.vertical) {
            for (const QStringList& row : step.table.rows)
                if (!row.isEmpty() && !row.first().trimmed().isEmpty())
                    named << row.first().trimmed();
        } else {
            if (!step.table.hasHeader) return;
            for (const QString& h : step.table.rows.first())
                if (!h.trimmed().isEmpty()) named << h.trimmed();
        }

        // CompareOnly names only the attributes it cares about, so a missing one
        // is the point rather than a mistake. An unknown one still is not.
        QStringList unfilled;
        if (!step.compareOnly) {
            for (const Field& f : as->fields) {
                if (f.name.isEmpty()) continue;
                bool found = false;
                for (const QString& n : named)
                    if (n.compare(f.name, Qt::CaseInsensitive) == 0) { found = true; break; }
                if (found) continue;
                if (!f.defaultValue.trimmed().isEmpty()) continue;
                unfilled << f.name;
            }
        }

        QStringList unknownCols;
        for (const QString& n : named) {
            bool known = false;
            for (const Field& f : as->fields)
                if (n.compare(f.name, Qt::CaseInsensitive) == 0) { known = true; break; }
            if (!known) unknownCols << n;
        }

        // One misspelled heading is one mistake, said once.
        pairMisspelledHeadings(QStringLiteral("Table"), as->name, step.line,
                               unknownCols, unfilled, msgs);

        for (const QString& f : unfilled) {
            ParseMessage m;
            m.line    = step.line;
            m.warning = false;
            m.text    = QString("Table is missing column '%1' and '%1' has no default value")
                            .arg(f);
            msgs.push_back(m);
        }

        for (const QString& n : unknownCols) {
            ParseMessage m;
            m.line    = step.line;
            m.warning = true;
            m.text    = QString("Table has column '%1' which doesn't match any field on '%2' "
                                "— it will be ignored").arg(n, as->name);
            msgs.push_back(m);
        }

        // Each cell against the type its column declares, reported at the
        // row it was read from -- or at the step, when the rows came from an
        // Inserted file and have no line here.
        auto rowLine = [&](int r) {
            return r < step.table.rowLines.size() ? step.table.rowLines[r] : step.line;
        };
        if (step.table.vertical) {
            for (int r = 0; r < step.table.rows.size(); ++r) {
                const QStringList& row = step.table.rows[r];
                if (row.size() < 2) continue;
                const QString type = fieldTypeOf(*as, row.first().trimmed());
                for (int c = 1; c < row.size(); ++c)
                    if (!statesNoValue(row[c].trimmed()))
                        validateValue(rowLine(r), row[c].trimmed(), type, msgs);
            }
        } else {
            const QStringList& headers = step.table.rows.first();
            for (int r = 1; r < step.table.rows.size(); ++r) {
                const QStringList& cells = step.table.rows[r];
                for (int c = 0; c < qMin(cells.size(), headers.size()); ++c)
                    if (!statesNoValue(cells[c].trimmed()))
                        validateValue(rowLine(r), cells[c].trimmed(),
                                      fieldTypeOf(*as, headers[c].trimmed()), msgs);
            }
        }
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) check(step);
    for (const Step& step : file.backgroundSteps) check(step);
    for (const Step& step : file.cleanupSteps)    check(step);

    return msgs;
}

// ---------------------------------------------------------------------------
// The shape a DataType or built-in table takes when it reaches the glue
// ---------------------------------------------------------------------------

bool isDataTypeOrBuiltin(const QString& name, const SpectableFile& file)
{
    if (name.trimmed().isEmpty()) return false;
    if (isBuiltinDataType(name)) return true;
    for (const QString& d : file.dataTypeNames)
        if (d.compare(name, Qt::CaseInsensitive) == 0) return true;
    return false;
}

CellShape cellShapeOf(const Step& step, const SpectableFile& file,
                      bool flattenSingleRowToList, bool flattenSingleCellToScalar)
{
    // Only a DataType or a built-in has a shape to choose. An Attributes set or
    // an Entity is one object per row, which is a list whatever the table looks
    // like -- and a grid of them only by saying EveryCell.
    if (!isDataTypeOrBuiltin(step.attrSetName, file)) return CellShape::Grid;
    if (!step.hasTable) return CellShape::Grid;

    int rows = 0, cols = 0;
    for (const QStringList& row : step.table.rows) {
        int cells = 0;
        for (const QString& c : row) if (!c.trimmed().isEmpty()) ++cells;
        if (cells == 0) continue;
        ++rows;
        cols = qMax(cols, cells);
    }
    if (rows == 0) return CellShape::Grid;

    if (flattenSingleCellToScalar && rows == 1 && cols == 1) return CellShape::Scalar;
    if (flattenSingleRowToList && (rows == 1 || cols == 1))   return CellShape::List;
    return CellShape::Grid;
}

QVector<ParseMessage> validateStepTableShapes(const SpectableFile& file,
                                              bool flattenSingleRowToList,
                                              bool flattenSingleCellToScalar)
{
    QVector<ParseMessage> msgs;

    struct Seen { CellShape shape; int line; };
    QMap<QString, Seen> first;

    auto shapeWord = [](CellShape s) {
        switch (s) {
        case CellShape::Scalar: return QStringLiteral("a single value");
        case CellShape::List:   return QStringLiteral("a list");
        default:                return QStringLiteral("a grid");
        }
    };

    auto check = [&](const Step& step) {
        if (!isDataTypeOrBuiltin(step.attrSetName, file)) return;
        if (!step.hasTable) return;
        const CellShape shape =
            cellShapeOf(step, file, flattenSingleRowToList, flattenSingleCellToScalar);

        // One glue method per step text and type, so that is the key.
        const QString key = step.text.trimmed().toLower() + "|" + step.attrSetName.toLower();
        auto it = first.find(key);
        if (it == first.end()) { first.insert(key, { shape, step.line }); return; }
        if (it->shape == shape) return;

        ParseMessage m;
        m.line    = step.line;
        m.warning = false;
        m.text    = QString("Step '%1' takes %2 here and %3 at line %4. One step is one glue "
                            "method, so every occurrence has to agree on the shape of its "
                            "table -- write them alike, or turn off the flattening that "
                            "separates them")
                        .arg(step.text.trimmed(), shapeWord(shape),
                             shapeWord(it->shape), QString::number(it->line));
        msgs.push_back(m);
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) check(step);
    for (const Step& step : file.backgroundSteps) check(step);
    for (const Step& step : file.cleanupSteps)    check(step);

    return msgs;
}

// ---------------------------------------------------------------------------
// Which glue methods need the attribute set in their name
// ---------------------------------------------------------------------------

QString stepPhraseKey(const QString& keyword, const QString& stepText)
{
    QString s = keyword + "_" + stepText;
    s.replace(QRegularExpression(R"([^A-Za-z0-9]+)"), "_");
    s.remove(QRegularExpression("^_+|_+$"));
    return s.toLower();
}

// What a step hands its glue method, as a key rather than a type name: the nine
// languages spell the types differently but divide the steps the same way.
static QString glueTakeKey(const Step& step, const SpectableFile& file)
{
    if (step.hasDocString) return QStringLiteral("docstring");

    if (!step.defineRef.isEmpty() && step.attrSetName.isEmpty()) {
        for (const Define& d : file.defines)
            if (d.name.compare(step.defineRef, Qt::CaseInsensitive) == 0)
                return d.hasDocString ? QStringLiteral("docstring") : QString();
        return QString();
    }

    // No set named means nothing reaches the glue, table or no table: an
    // unread table is its own warning.
    if (step.attrSetName.trimmed().isEmpty()) return QString();

    const QString named = step.attrSetName.trimmed();
    if (step.everyCell)                        return "grid:" + named.toLower();
    if (isDataTypeOrBuiltin(named, file))      return "type:" + named.toLower();

    // A Collection reaches the glue as a list of its element type, so a step
    // naming the collection and one naming the element take the same thing and
    // do not need telling apart.
    for (const Collection& c : file.collections)
        if (c.name.compare(named, Qt::CaseInsensitive) == 0)
            return "set:" + c.elementType.toLower();
    return "set:" + named.toLower();
}

QSet<QString> stepPhrasesNeedingAttrSet(const SpectableFile& file)
{
    QMap<QString, QSet<QString>> takes;   // phrase -> what its occurrences hand the glue
    QMap<QString, QSet<QString>> sets;    // phrase -> the attribute sets they name

    auto note = [&](const Step& step) {
        const QString key = stepPhraseKey(step.keyword, step.text);
        if (key.isEmpty()) return;
        takes[key].insert(glueTakeKey(step, file));
        sets[key].insert(step.attrSetName.trimmed().toLower());
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) note(step);
    for (const Step& step : file.backgroundSteps) note(step);
    for (const Step& step : file.cleanupSteps)    note(step);

    QSet<QString> needed;
    for (auto it = takes.cbegin(); it != takes.cend(); ++it)
        if (it.value().size() > 1 && sets.value(it.key()).size() > 1)
            needed.insert(it.key());
    return needed;
}

// ---------------------------------------------------------------------------
// An Examples: table against the attribute set it names
// ---------------------------------------------------------------------------
//
// The same three questions validateStepTables asks of a step table: a column
// naming no field, a field with no column and no default, and each cell
// against its column's type. A built-in set such as ValidValues has no
// declaration to read, and the generator supplies its shape.

QVector<ParseMessage> validateExamplesTables(const SpectableFile& file)
{
    QVector<ParseMessage> msgs;
    const QMap<QString, const AttrSet*> byName = attrSetsByName(file);

    for (const NamedBlock& nb : file.namedBlocks) {
        if (nb.isContext || nb.examples.line == 0) continue;
        const AttrSet* as = byName.value(nb.examples.attrSetName.toLower(), nullptr);
        if (!as || as->fields.isEmpty()) continue;
        const ExamplesBlock& ex = nb.examples;
        if (ex.header.isEmpty()) continue;

        const int exLine = ex.headerLine ? ex.headerLine : ex.line;

        // A column naming no field is data the generator drops on the floor.
        QStringList unknownCols;
        for (const QString& col : ex.header) {
            const QString c = col.trimmed();
            if (c.isEmpty() || fieldOf(*as, c)) continue;
            unknownCols << c;
        }

        // A field with no column and no default is one the generator cannot
        // fill, and it refuses the file for it. A field with a default is filled
        // from the default, which is what declaring one is for.
        QStringList unfilled;
        for (const Field& f : as->fields) {
            if (f.name.isEmpty()) continue;
            bool found = false;
            for (const QString& col : ex.header)
                if (col.trimmed().compare(f.name, Qt::CaseInsensitive) == 0) { found = true; break; }
            if (found || !f.defaultValue.trimmed().isEmpty()) continue;
            unfilled << f.name;
        }

        // One misspelled heading is one mistake, said once.
        pairMisspelledHeadings(QStringLiteral("Examples table"), ex.attrSetName, exLine,
                               unknownCols, unfilled, msgs);

        for (const QString& c : unknownCols) {
            ParseMessage m;
            m.line    = exLine;
            m.warning = true;
            m.text    = QString("Examples table has column '%1', which is not an attribute of "
                                "'%2' -- its values are ignored").arg(c, ex.attrSetName);
            msgs.push_back(m);
        }
        for (const QString& f : unfilled) {
            ParseMessage m;
            m.line    = exLine;
            m.warning = false;
            m.text    = QString("Examples table for '%1' has no column '%2', and '%2' has no "
                                "default value").arg(ex.attrSetName, f);
            msgs.push_back(m);
        }

        for (int r = 0; r < ex.rows.size(); ++r) {
            const QStringList& row = ex.rows[r];
            const int line = r < ex.rowLines.size() ? ex.rowLines[r] : ex.line;
            for (int c = 0; c < qMin(row.size(), ex.header.size()); ++c)
                if (!statesNoValue(row[c].trimmed()))
                    validateValue(line, row[c].trimmed(),
                                  fieldTypeOf(*as, ex.header[c].trimmed()), msgs);
        }
    }
    return msgs;
}

// ---------------------------------------------------------------------------
// The Default column of an Attributes or Entity block
// ---------------------------------------------------------------------------
//
// Each default against the type declared beside it on the same row. Blank
// means no default; "~" is the spec's explicit empty string; "(none)" says
// there is none. A "=Name" still here is one resolveDefineReferences could not
// resolve, and is reported as that elsewhere.

QVector<ParseMessage> validateAttributeDefaults(const SpectableFile& file)
{
    QVector<ParseMessage> msgs;
    for (const AttrSet& as : file.attrSets) {
        if (as.isContext) continue;
        for (const Field& f : as.fields) {
            const QString v = f.defaultValue.trimmed();
            if (statesNoValue(v) || v == "~" || v.compare("(none)", Qt::CaseInsensitive) == 0)
                continue;
            validateValue(f.line, v, f.type.trimmed(), msgs);
        }
    }
    return msgs;
}

// ---------------------------------------------------------------------------
// A sibling's declarations, made visible
// ---------------------------------------------------------------------------

void mergeContext(SpectableFile& file, const SpectableFile& ctx)
{
    for (AttrSet as : ctx.attrSets)         { as.isContext = true;  file.attrSets.push_back(as); }
    for (Collection col : ctx.collections)  { col.isContext = true; file.collections.push_back(col); }
    for (DomainTerm dt : ctx.domainTerms)   { dt.isContext = true;  file.domainTerms.push_back(dt); }
    for (Define def : ctx.defines)          { def.isContext = true; file.defines.push_back(def); }
    for (NamedBlock nb : ctx.namedBlocks)   { nb.isContext = true;  file.namedBlocks.push_back(nb); }
    for (const QString& dt : ctx.dataTypeNames)
        if (!file.dataTypeNames.contains(dt)) file.dataTypeNames.push_back(dt);
}

// ---------------------------------------------------------------------------
// Which way a table runs, when the step did not say
// ---------------------------------------------------------------------------
//
// Groundwork from 2026-09-13: a Vertical table is treated as a transposed
// horizontal one everywhere, so nothing downstream needs to know the word was
// inferred rather than written. The word is kept, and still wins when
// present -- it is worth having where a reader wants the orientation stated.

void inferTableOrientation(SpectableFile& file)
{
    const QMap<QString, const AttrSet*> byName = attrSetsByName(file);

    auto infer = [&](Step& step) {
        if (!step.hasTable || step.vertical || step.everyCell) return;
        if (!step.table.hasHeader || step.table.rows.size() < 1) return;
        const AttrSet* as = byName.value(step.attrSetName.toLower(), nullptr);
        if (!as || as->fields.isEmpty()) return;

        auto isField = [&](const QString& cell) { return fieldOf(*as, cell.trimmed()) != nullptr; };

        const QStringList& header = step.table.rows.first();
        int headerFields = 0;
        for (const QString& c : header) if (isField(c)) ++headerFields;
        const bool headerIsAllFields = !header.isEmpty() && headerFields == header.size();
        if (headerIsAllFields) return;   // horizontal, as written

        int firstColumnFields = 0;
        for (const QStringList& row : step.table.rows)
            if (!row.isEmpty() && isField(row.first())) ++firstColumnFields;
        const bool firstColumnIsAllFields = firstColumnFields == step.table.rows.size();
        if (!firstColumnIsAllFields) return;   // neither reading fits; leave it

        step.vertical        = true;
        step.table.vertical  = true;
        step.table.hasHeader = false;
    };

    for (Scenario& s : file.scenarios)
        for (Step& step : s.steps) infer(step);
    for (Step& step : file.backgroundSteps) infer(step);
    for (Step& step : file.cleanupSteps)    infer(step);
}
