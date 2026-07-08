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
    if (kwLow == "and" || kwLow == "but")
        return last.isEmpty() ? "Given" : last;
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
                                  QString& attrSet, bool& transposed)
{
    static QRegularExpression reStep(
        R"(^\s*(Given|When|Then|And|But)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reAttr(
        R"(\s*:\s*(\w+)(?:\s+(Transposed))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    auto m = reStep.match(trimmed);
    if (!m.hasMatch()) return false;

    kw           = m.captured(1);
    QString rest = m.captured(2).trimmed();

    auto ma = reAttr.match(rest);
    if (ma.hasMatch()) {
        attrSet    = ma.captured(1);
        transposed = !ma.captured(2).isEmpty();
        text       = rest.left(ma.capturedStart()).trimmed();
    } else {
        attrSet    = {};
        transposed = false;
        text       = rest;
    }
    return true;
}

bool SpectableParser::isContinuation(const QString& line)
{
    return line.endsWith('\\') || line.endsWith("\\ ");
}

// Lines that appear inside any block and should be silently skipped
// WITHOUT changing the current parser state
bool SpectableParser::isSkipKeyword(const QString& firstWord)
{
    static const QStringList words = {
        "Description", "Details", "Constraint", "Notes",
        "Insert"
    };
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
    const QStringList lines = in.readAll().split('\n');

    enum class State {
        Top,
        InAttrDef,         // reading the field-definition pipe table
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
    Define*     curDefine     = nullptr;
    Scenario*   curScen       = nullptr;
    Step*       curStep       = nullptr;
    NamedBlock* curNamedBlock = nullptr;
    QStringList attrHeaders;

    auto emitMsg = [&](int ln, const QString& msg, bool warn = false) {
        result.messages.push_back({ ln, msg, warn });
    };

    bool inCleanupBlock = false; // true while collecting cleanup steps

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

    auto endDefineDef = [&]() {
        curDefine = nullptr;
        state     = State::Top;
    };

    auto endNamedBlock = [&]() {
        curNamedBlock = nullptr;
        state         = State::Top;
    };

    // Append one line of docstring content, resolving Insert commands.
    // `lineNum` is captured by reference so it reflects the current loop variable.
    static QRegularExpression reDocInsert(
        R"re(^\s*Insert\s+(?:"([^"]+)"|'([^']+)'|<([^>]+)>)\s*$)re",
        QRegularExpression::CaseInsensitiveOption);
    const QString baseDir = QFileInfo(absPath).absolutePath();

    auto appendDocLine = [&](const QString& raw, QString& docStr, int lineNum) {
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
                if (curStep) appendDocLine(raw, curStep->docString, lineNum);
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
                if (curDefine) appendDocLine(raw, curDefine->docString, lineNum);
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
            state = State::InDocString;
            continue;
        }

        // ── Blank lines ──────────────────────────────────────────────────────
        if (trimmed.isEmpty()) {
            if (state == State::InStepTable || state == State::AwaitStepTable)
                endStepTable();
            if (state == State::SkipTable)
                state = State::Top;
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

            case State::InAttrDef:
                if (attrHeaders.isEmpty()) {
                    attrHeaders = cells;
                    // Warn on unrecognised header columns
                    static const QStringList knownHeaders = {
                        "attribute", "name", "type", "datatype",
                        "default", "notes", "in-out", "in/out", "multiples"
                    };
                    for (const QString& h : cells) {
                        if (!h.startsWith('#') && !knownHeaders.contains(h.toLower())) {
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
                    for (int ci = 0; ci < attrHeaders.size(); ++ci) {
                        const QString h = attrHeaders[ci].toLower();
                        const QString v = (ci < cells.size()) ? cells[ci] : QString();
                        if (h == "attribute" || h == "name") fd.name         = v;
                        else if (h == "type" || h == "datatype") fd.type     = v;
                        else if (h == "default")             fd.defaultValue = v;
                        else if (h == "notes")               fd.notes        = v;
                        else if (h == "in-out" || h == "in/out") fd.inOut   = v;
                        else if (h == "multiples")           fd.multiples    =
                            !v.isEmpty() && (v[0].toLower() == 'y' ||
                             v.compare("true", Qt::CaseInsensitive) == 0);
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
                        curDefine->transposed = (h0 == "attribute" || h0 == "name");
                    }
                }
                break;

            case State::AwaitStepTable: {
                if (!curStep) break;
                curStep->hasTable = true;
                state = State::InStepTable;
                // Detect format
                if (!curStep->transposed && cells.size() >= 2) {
                    QString h0 = cells[0].toLower();
                    if (h0 == "attribute" || h0 == "name") {
                        // Implicit transposed: | Attribute | Value | header
                        curStep->transposed       = true;
                        curStep->table.transposed = true;
                        curStep->table.hasHeader  = true;
                        // This row is the header — don't add to rows
                    } else {
                        // Normal: first row = column headers
                        curStep->table.hasHeader = true;
                        curStep->table.rows.push_back(cells);
                    }
                } else if (curStep->transposed) {
                    // Explicit Transposed: no header row
                    curStep->table.transposed = true;
                    curStep->table.hasHeader  = false;
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
                    if (curNamedBlock->examples.header.isEmpty())
                        curNamedBlock->examples.header = cells;  // first row = column headers
                    else
                        curNamedBlock->examples.rows.push_back(cells);
                }
                break;

            default:
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
            state = State::InDefineDocString;
            continue;
        }

        // End open step table
        if (state == State::InStepTable || state == State::AwaitStepTable)
            endStepTable();
        if (state == State::SkipTable)
            state = State::Top;

        // Keyword dispatch
        const QString firstWord = trimmed.split(QRegularExpression(R"(\s+)")).first();

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
            }
            continue;
        }

        // ── Inline skips (Description, Details, etc.) — transparent to state ──
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
            if (state == State::InAttrDef)    endAttrDef();
            if (state == State::InDefineTable) endDefineDef();
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

        // ── Terminate open Attributes/Define table on any other keyword ────────
        if (state == State::InAttrDef)    endAttrDef();
        if (state == State::InDefineTable) endDefineDef();

        // ── Parsed keywords ───────────────────────────────────────────────────

        // Specification
        if (firstWord.compare("Specification", Qt::CaseInsensitive) == 0) {
            result.specName = trimmed.mid(firstWord.length()).trimmed();
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
            attrHeaders.clear();
            state = State::InAttrDef;
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
                    state     = State::Top;
                } else {
                    def.isTable = true;
                    result.defines.push_back(def);
                    curDefine = &result.defines.last();
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
            state   = State::InScenario;
            continue;
        }

        // Steps (Given / When / Then / And / But)
        if (state == State::InScenario || state == State::InBackground
                                       || state == State::InCleanup) {
            QString kw, text, attrSet;
            bool    trans = false;
            if (isStepLine(trimmed, kw, text, attrSet, trans)) {
                lastKw = normalizeKeyword(kw, lastKw);
                Step st;
                st.keyword     = lastKw;
                st.text        = text;
                st.attrSetName = attrSet;
                st.transposed  = trans;
                st.line        = lineNum;

                if (state == State::InScenario && curScen) {
                    curScen->steps.push_back(st);
                    curStep = &curScen->steps.last();
                } else if (inCleanupBlock) {
                    result.cleanupSteps.push_back(st);
                    curStep = &result.cleanupSteps.last();
                } else {
                    result.backgroundSteps.push_back(st);
                    curStep = &result.backgroundSteps.last();
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
