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
                                  QString& badModifier)
{
    badModifier.clear();
    static QRegularExpression reStep(
        R"(^\s*(Given|When|Then|And|WhenThen)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reAttr(
        R"(\s*:\s*(\w+)(?:\s+(Vertical|CompareOnly))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    auto m = reStep.match(trimmed);
    if (!m.hasMatch()) return false;

    kw           = m.captured(1);
    QString rest = m.captured(2).trimmed();

    // "Given items are : Item Sideways" -- a second word after the type that is
    // not a modifier. reAttr does not match it, so without this the line reads
    // as a step naming no attribute set at all, and the real mistake, a misspelt
    // modifier, is never named.
    static QRegularExpression reTrailingWord(
        R"(:\s*(\w+)\s+(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    const auto mb = reTrailingWord.match(rest);
    if (mb.hasMatch()) {
        const QString mod = mb.captured(2).toLower();
        if (mod != "vertical" && mod != "compareonly")
            badModifier = mb.captured(2);
    }

    auto ma = reAttr.match(rest);
    if (ma.hasMatch()) {
        attrSet     = ma.captured(1);
        const QString mod = ma.captured(2).toLower();
        vertical  = (mod == "vertical");
        compareOnly = (mod == "compareonly");
        text        = rest.left(ma.capturedStart()).trimmed();
    } else {
        attrSet     = {};
        vertical  = false;
        compareOnly = false;
        text        = rest;
    }
    return true;
}

bool SpectableParser::isContinuation(const QString& line)
{
    return line.endsWith('\\') || line.endsWith("\\ ");
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

SpectableFile SpectableParser::parseImpl(const QString& filePath, QSet<QString>& visited)
{
    const QString absPath = QFileInfo(filePath).absoluteFilePath();
    if (visited.contains(absPath)) return {};
    visited.insert(absPath);

    SpectableFile result;
    result.filePath = filePath;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.messages.push_back({ 0, "Cannot open file: " + filePath, false });
        return result;
    }
    QTextStream in(&f);
    QStringList lines = in.readAll().split('\n');

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

    auto emitMsg = [&](int ln, const QString& msg, bool warn = false) {
        result.messages.push_back({ ln, msg, warn });
    };

    bool inCleanupBlock = false; // true while collecting cleanup steps

    // Top-level "Insert" of a .spectable file splices that file's own lines
    // in place (recursively re-parsed by this same loop), unlike Import,
    // which only pulls in AttrSets/Defines/named blocks by reference. Guards
    // against re-splicing the same file twice (e.g. a cycle of mutual Inserts).
    QSet<QString> insertedSpectableFiles;

    auto endStepTable = [&]() {
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

    auto endDefineDef = [&]() {
        curDefine = nullptr;
        state     = State::Top;
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

    auto appendDocLine = [&](const QString& rawLine, QString& docStr, int lineNum, int indentCols) {
        const QString raw = rawLine.mid(qMin(indentCols, leadingWsCount(rawLine)));
        auto m = reDocInsert.match(raw);
        if (m.hasMatch()) {
            const QString fname = !m.captured(1).isEmpty() ? m.captured(1)
                                : !m.captured(2).isEmpty() ? m.captured(2)
                                                           : m.captured(3);
            const QString fullPath = QFileInfo(baseDir + "/" + fname).absoluteFilePath();
            QFile ins(fullPath);
            if (!ins.open(QIODevice::ReadOnly | QIODevice::Text)) {
                emitMsg(lineNum, "WARNING: Cannot insert file: " + fname, false);
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

        // ── DocString accumulation ────────────────────────────────────────────
        // Must come before blank/indentation checks so all content is captured.
        if (state == State::InDocString) {
            if (trimmed == "\"\"\"") {
                if (curStep) {
                    if (curStep->docString.endsWith('\n'))
                        curStep->docString.chop(1);
                    curStep->hasDocString = true;
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
            pendingGeneratorTags.clear();
            continue;
        }

        // ── Comments ─────────────────────────────────────────────────────────
        if (trimmed.startsWith('#'))
            continue;

        // ── Tags (@TagName) — accumulate for the next block ──────────────────
        if (trimmed.startsWith('@')) {
            static QRegularExpression reTag(R"(@(\w+))");
            auto it = reTag.globalMatch(trimmed);
            while (it.hasNext()) pendingTags << it.next().captured(1);
            continue;
        }

        // ── Generator tags ($TagName) — filtering only, never emitted ────────
        if (trimmed.startsWith('$')) {
            static QRegularExpression reGenTag(R"(\$(\w+))");
            auto it = reGenTag.globalMatch(trimmed);
            while (it.hasNext()) pendingGeneratorTags << it.next().captured(1);
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
                } else {
                    curStep->table.hasHeader = true;
                    curStep->table.rows.push_back(cells);
                }
                break;
            }

            case State::InStepTable:
                if (curStep)
                    curStep->table.rows.push_back(cells);
                break;

            case State::InExamplesTable:
                if (curNamedBlock) {
                    if (curNamedBlock->examples.header.isEmpty()) {
                        curNamedBlock->examples.header = cells;  // first row = column headers
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
                    curStep->defineRef = defName;
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
                    QFile ins(fullPath);
                    if (!ins.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        emitMsg(lineNum, "WARNING: Cannot insert file: " + fname, false);
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

        // The other named comments are not captured yet, but they are equally
        // transparent — none of them ends a table either.
        if (isNamedComment(firstWord))
            continue;

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
                SpectableFile imp = parseImpl(imported, visited);
                for (const AttrSet& as : imp.attrSets)
                    result.attrSets.push_back(as);
                for (const Define& def : imp.defines)
                    result.defines.push_back(def);
                for (NamedBlock nb : imp.namedBlocks) {
                    nb.isContext = true;
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
                    if (insertedSpectableFiles.contains(fullPath)) {
                        // Already spliced once — skip re-insertion (also guards
                        // against an infinite loop from mutual/self Inserts).
                        continue;
                    }
                    insertedSpectableFiles.insert(fullPath);
                    QFile ins(fullPath);
                    if (!ins.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        emitMsg(lineNum, "WARNING: Cannot insert file: " + fname, false);
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
        if (isSkipKeyword(firstWord))
            continue;

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
            auto dm = reDef.match(trimmed);
            if (dm.hasMatch()) {
                Define def;
                def.name = dm.captured(1);
                def.line = lineNum;
                QString afterEq = dm.captured(2).trimmed();
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
            curScen = nullptr; curStep = nullptr; lastKw = {};
            inCleanupBlock = false;
            state = State::InBackground;
            continue;
        }

        // Cleanup
        if (firstWord.compare("Cleanup", Qt::CaseInsensitive) == 0 ||
            trimmed.startsWith("Cleanup:", Qt::CaseInsensitive)) {
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

        // Steps (Given / When / Then / And / But)
        if (state == State::InScenario || state == State::InBackground
                                       || state == State::InCleanup) {
            QString kw, text, attrSet, badModifier;
            bool    trans = false, cmpOnly = false;
            if (isStepLine(trimmed, kw, text, attrSet, trans, cmpOnly, badModifier)) {
                if (!badModifier.isEmpty())
                    emitMsg(lineNum,
                            QString("Unrecognized step modifier '%1' -- expected "
                                    "CompareOnly or Vertical").arg(badModifier),
                            true);
                lastKw = normalizeKeyword(kw, lastKw);
                Step st;
                st.keyword      = lastKw;
                st.text         = text;
                st.attrSetName  = attrSet;
                st.vertical   = trans;
                st.compareOnly  = cmpOnly;
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

        pendingTags.clear();
        pendingGeneratorTags.clear();
        emitMsg(lineNum, QString("Unrecognized keyword '%1'").arg(firstWord), true);
    }

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

QVector<ParseMessage> validateStepTables(const SpectableFile& file)
{
    QVector<ParseMessage> msgs;

    QMap<QString, const AttrSet*> byName;
    for (const AttrSet& as : file.attrSets)
        byName.insert(as.name.toLower(), &as);

    auto check = [&](const Step& step) {
        if (!step.hasTable || step.attrSetName.isEmpty()) return;
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
        if (!step.compareOnly) {
            for (const Field& f : as->fields) {
                if (f.name.isEmpty()) continue;
                bool found = false;
                for (const QString& n : named)
                    if (n.compare(f.name, Qt::CaseInsensitive) == 0) { found = true; break; }
                if (found) continue;
                if (!f.defaultValue.trimmed().isEmpty()) continue;

                ParseMessage m;
                m.line    = step.line;
                m.warning = false;
                m.text    = QString("Table is missing column '%1' and '%1' has no default value")
                                .arg(f.name);
                msgs.push_back(m);
            }
        }

        for (const QString& n : named) {
            bool known = false;
            for (const Field& f : as->fields)
                if (n.compare(f.name, Qt::CaseInsensitive) == 0) { known = true; break; }
            if (known) continue;

            ParseMessage m;
            m.line    = step.line;
            m.warning = true;
            m.text    = QString("Table has column '%1' which doesn't match any field on '%2' "
                                "— it will be ignored").arg(n, as->name);
            msgs.push_back(m);
        }
    };

    for (const Scenario& s : file.scenarios)
        for (const Step& step : s.steps) check(step);
    for (const Step& step : file.backgroundSteps) check(step);
    for (const Step& step : file.cleanupSteps)    check(step);

    return msgs;
}
