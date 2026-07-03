#include "SpecTableAnalyzer.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Diagnostic SpecTableAnalyzer::makeDiag(const QString& filePath, int line,
                                        const QString& msg, Diagnostic::Severity sev)
{
    Diagnostic d;
    d.filePath = filePath;
    d.line     = line;
    d.column   = 1;
    d.message  = msg;
    d.severity = sev;
    return d;
}

// ---------------------------------------------------------------------------
// SpecTableAnalyzer
// ---------------------------------------------------------------------------

SpecTableAnalyzer::SpecTableAnalyzer(SpecTableIndex* index)
    : m_index(index)
{}

QList<Diagnostic> SpecTableAnalyzer::analyzeFile(const QString& filePath) const
{
    QList<Diagnostic> diags;
    if (!m_index) return diags;

    // All files in the same project share full visibility.
    // Import is only needed for files outside the project.
    const SpecTableSymbols& visible = m_index->projectSymbols();

    checkImports               (filePath, visible, diags);
    checkInserts               (filePath, diags);
    checkStepRefs              (filePath, visible, diags);
    checkDescriptions          (filePath, diags);
    checkExamples              (filePath, visible, diags);
    checkDefineRefs            (filePath, visible, diags);
    checkCleanup               (filePath, diags);
    checkTableColumnConsistency(filePath, diags);
    checkStepTableContents     (filePath, visible, diags);
    checkDomainTermDuplicates  (filePath, diags);
    checkUnrecognizedLines     (filePath, diags);

    return diags;
}

// ---------------------------------------------------------------------------
// Check 1a — Import paths exist
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkImports(const QString& filePath,
                                      const SpecTableSymbols&,
                                      QList<Diagnostic>& out) const
{
    const QStringList imports = m_index->importsFor(filePath);

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reImport("^\\s*Import\\s+\"([^\"]+)\"",
                                       QRegularExpression::CaseInsensitiveOption);
    const QString dir = QFileInfo(filePath).absolutePath();

    QTextStream in(&f);
    int lineNum = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNum;
        auto m = reImport.match(line);
        if (!m.hasMatch()) continue;

        const QString importedPath = m.captured(1);
        const QString resolved     = QFileInfo(dir + "/" + importedPath).absoluteFilePath();

        if (!QFile::exists(resolved))
            out.append(makeDiag(filePath, lineNum,
                QStringLiteral("Imported file not found: '%1'").arg(importedPath)));
    }
}

// ---------------------------------------------------------------------------
// Check 1b — Insert paths exist (any file type allowed)
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkInserts(const QString& filePath,
                                      QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reInsert("^\\s*Insert\\s+\"([^\"]+)\"",
                                       QRegularExpression::CaseInsensitiveOption);
    const QString dir = QFileInfo(filePath).absolutePath();

    QTextStream in(&f);
    int lineNum = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNum;
        auto m = reInsert.match(line);
        if (!m.hasMatch()) continue;

        const QString insertedPath = m.captured(1);
        const QString resolved     = QFileInfo(dir + "/" + insertedPath).absoluteFilePath();

        if (!QFile::exists(resolved))
            out.append(makeDiag(filePath, lineNum,
                QStringLiteral("Inserted file not found: '%1'").arg(insertedPath)));
    }
}

// ---------------------------------------------------------------------------
// Check 2 — AttributeSet / BusinessRule / Calculation references in steps
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkStepRefs(const QString& filePath,
                                       const SpecTableSymbols& visible,
                                       QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    // "Given/When/Then ... : AttributeSet"
    static QRegularExpression reStepAttr(
        R"(^\s*(?:Given|When|Then|And|But)\b.+:\s*(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "When applying BusinessRule X : Y"
    static QRegularExpression reBizRuleStep(
        R"(^\s*(?:Given|When|Then|And|But)\b.*applying\s+BusinessRule\s+(\w+)\s*:\s*(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "When applying Calculation X : Y"
    static QRegularExpression reCalcStep(
        R"(^\s*(?:Given|When|Then|And|But)\b.*applying\s+Calculation\s+(\w+)\s*:\s*(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&f);
    int lineNum = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNum;

        // Check BusinessRule step first (more specific)
        auto m = reBizRuleStep.match(line);
        if (m.hasMatch()) {
            const QString ruleName = m.captured(1);
            const QString attrName = m.captured(2);
            if (!visible.hasBusinessRule(ruleName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Unknown BusinessRule '%1'").arg(ruleName)));
            if (!visible.hasAttributeSet(attrName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Unknown AttributeSet '%1'").arg(attrName)));
            continue;
        }

        // Check Calculation step
        m = reCalcStep.match(line);
        if (m.hasMatch()) {
            const QString calcName = m.captured(1);
            const QString attrName = m.captured(2);
            if (!visible.hasCalculation(calcName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Unknown Calculation '%1'").arg(calcName)));
            if (!visible.hasAttributeSet(attrName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Unknown AttributeSet '%1'").arg(attrName)));
            continue;
        }

        // Generic step with AttributeSet or DataType reference
        m = reStepAttr.match(line);
        if (m.hasMatch()) {
            const QString attrName = m.captured(1);
            if (!visible.hasAttributeSet(attrName) && !visible.hasDataType(attrName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Unknown AttributeSet, Entity, or DataType '%1'").arg(attrName)));
        }
    }
}

// ---------------------------------------------------------------------------
// Check 3 — BusinessRule, DataType, Calculation should have a Description
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDescriptions(const QString& filePath,
                                           QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reDecl(
        R"(^\s*(BusinessRule|DataType|Calculation)\s+(\w+))",
        QRegularExpression::CaseInsensitiveOption);
    // Accept "Description <text>" (new spec) or "* text" (legacy)
    static QRegularExpression reDesc(R"(^\s*(Description\s+\S|\*.+))",
                                     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reBlank(R"(^\s*$)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reDecl.match(lines[i]);
        if (!m.hasMatch()) continue;

        // Look ahead: skip blank lines, expect a Description within 3 lines
        bool found = false;
        for (int j = i + 1; j < qMin(i + 4, lines.size()); ++j) {
            if (reDesc.match(lines[j]).hasMatch()) { found = true; break; }
            if (!reBlank.match(lines[j]).hasMatch()) break;
        }
        if (!found)
            out.append(makeDiag(filePath, i + 1,
                QStringLiteral("%1 '%2' has no Description")
                    .arg(m.captured(1), m.captured(2)),
                Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// Check 4 — BusinessRule/Calculation need Examples:; DataType needs a table
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkExamples(const QString& filePath,
                                       const SpecTableSymbols& visible,
                                       QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reDecl(
        R"(^\s*(BusinessRule|DataType|Calculation)\s+(\w+))",
        QRegularExpression::CaseInsensitiveOption);
    // Examples: <AttributeSet> — colon required in v2.7.2
    static QRegularExpression reExamples(R"(^\s*Examples:\s*(\w+))",
                                         QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reTableRow(R"(^\s*\|)");
    static QRegularExpression reNextTopLevel(
        R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
        QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reDecl.match(lines[i]);
        if (!m.hasMatch()) continue;

        const QString keyword = m.captured(1).toLower();
        const QString name    = m.captured(2);

        bool hasExamples    = false;
        bool hasTable       = false;
        for (int j = i + 1; j < lines.size(); ++j) {
            if (reNextTopLevel.match(lines[j]).hasMatch()) break;
            auto em = reExamples.match(lines[j]);
            if (em.hasMatch()) {
                hasExamples = true;
                // Validate the AttributeSet reference
                const QString attrName = em.captured(1);
                if (!visible.hasAttributeSet(attrName))
                    out.append(makeDiag(filePath, j + 1,
                        QStringLiteral("Unknown AttributeSet '%1' in Examples:").arg(attrName)));
            }
            if (reTableRow.match(lines[j]).hasMatch()) hasTable = true;
        }

        if (keyword == "datatype") {
            if (!hasTable && !hasExamples)
                out.append(makeDiag(filePath, i + 1,
                    QStringLiteral("DataType '%1' has no data table or Examples: section").arg(name),
                    Diagnostic::Severity::Warning));
        } else {
            if (!hasExamples)
                out.append(makeDiag(filePath, i + 1,
                    QStringLiteral("%1 '%2' has no Examples: section")
                        .arg(m.captured(1), name),
                    Diagnostic::Severity::Warning));
        }
    }
}

// ---------------------------------------------------------------------------
// Check 5 — =Name value references must resolve to a Define declaration
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDefineRefs(const QString& filePath,
                                         const SpecTableSymbols& visible,
                                         QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reRef(R"(=([A-Za-z_]\w*))");

    QTextStream in(&f);
    int lineNum = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNum;

        QRegularExpressionMatchIterator it = reRef.globalMatch(line);
        while (it.hasNext()) {
            auto m = it.next();
            const QString refName = m.captured(1);
            if (!visible.hasDefine(refName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Undefined value reference '=%1'").arg(refName)));
        }
    }
}

// ---------------------------------------------------------------------------
// Check 6 — Cleanup block may only contain Then and And steps
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkCleanup(const QString& filePath,
                                      QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reCleanup(R"(^\s*Cleanup\b)",
                                        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reTopLevel(
        R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reInvalidStep(R"(^\s*(Given|When|But)\b)",
                                            QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    bool inCleanup = false;
    for (int i = 0; i < lines.size(); ++i) {
        if (reCleanup.match(lines[i]).hasMatch()) {
            inCleanup = true;
            continue;
        }
        if (inCleanup) {
            if (reTopLevel.match(lines[i]).hasMatch()) {
                inCleanup = false;
                // Don't skip — let the loop re-evaluate this line normally next pass
                // (but since we just set inCleanup=false it won't be re-checked here)
                continue;
            }
            if (reInvalidStep.match(lines[i]).hasMatch())
                out.append(makeDiag(filePath, i + 1,
                    QStringLiteral("Cleanup block may only contain Then and And steps")));
        }
    }
}

// ---------------------------------------------------------------------------
// Check 7 — Table column count consistency within each table block
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkTableColumnConsistency(const QString& filePath,
                                                     QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reRow(R"(^\s*\|)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    int expectedCols = -1;

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (reRow.match(line).hasMatch()) {
            const QStringList parts = line.split('|');
            const int cols = parts.size() - 2; // exclude leading/trailing empty strings
            if (cols < 1) continue;
            if (expectedCols == -1) {
                expectedCols = cols;
            } else if (cols != expectedCols) {
                out.append(makeDiag(filePath, i + 1,
                    QStringLiteral("Table row has %1 column(s) but header has %2")
                        .arg(cols).arg(expectedCols),
                    Diagnostic::Severity::Warning));
            }
        } else if (!line.trimmed().isEmpty()) {
            expectedCols = -1; // non-blank, non-pipe line ends the current table block
        }
    }
}

// ---------------------------------------------------------------------------
// Helper — validate one cell value against a built-in DataType name
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::validateDataTypeValue(const QString& filePath, int lineNo,
                                               const QString& value, const QString& dtype,
                                               QList<Diagnostic>& out)
{
    if (dtype.isEmpty()) return;

    static QRegularExpression reInteger (R"(^-?\d+$)");
    static QRegularExpression reFloat   (R"(^-?\d+(\.\d+)?([eE][+-]?\d+)?$)");
    static QRegularExpression reDate    (R"(^\d{4}-\d{2}-\d{2}$)");
    static QRegularExpression reTime    (R"(^\d{2}:\d{2}(:\d{2})?$)");
    static QRegularExpression reDateTime(R"(^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2})");

    const QString ltype = dtype.toLower();

    if (ltype == "integer") {
        if (!reInteger.match(value).hasMatch())
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid Integer").arg(value),
                Diagnostic::Severity::Warning));
    } else if (ltype == "float") {
        if (!reFloat.match(value).hasMatch())
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid Float").arg(value),
                Diagnostic::Severity::Warning));
    } else if (ltype == "boolean") {
        static const QStringList valid{"true", "false"};
        if (!valid.contains(value.toLower()))
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid Boolean (use true/false)").arg(value),
                Diagnostic::Severity::Warning));
    } else if (ltype == "yesno") {
        static const QStringList valid{"y", "n", "yes", "no", "t", "f", "true", "false"};
        if (!valid.contains(value.toLower()))
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid YesNo value").arg(value),
                Diagnostic::Severity::Warning));
    } else if (ltype == "date") {
        if (!reDate.match(value).hasMatch())
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid Date (use YYYY-MM-DD)").arg(value),
                Diagnostic::Severity::Warning));
    } else if (ltype == "time") {
        if (!reTime.match(value).hasMatch())
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid Time (use HH:MM or HH:MM:SS)").arg(value),
                Diagnostic::Severity::Warning));
    } else if (ltype == "datetime") {
        if (!reDateTime.match(value).hasMatch())
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid DateTime").arg(value),
                Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// Check 8 — Step table attribute names and DataType values
//   Covers: attribute names match the AttributeSet (3.2),
//           DataType values are valid (3.3), Transposed structure (3.4)
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkStepTableContents(const QString& filePath,
                                                const SpecTableSymbols& visible,
                                                QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reStep(
        R"(^\s*(?:Given|When|Then|And|But)\b.+:\s*(\w+)(\s+Transposed)?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reApplying(
        R"(\bapplying\s+(?:BusinessRule|Calculation)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow(R"(^\s*\|)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reStep.match(lines[i]);
        if (!m.hasMatch()) continue;
        // BusinessRule/Calculation steps have more complex structure — skip here
        if (reApplying.match(lines[i]).hasMatch()) continue;

        const QString attrName   = m.captured(1);
        const bool    transposed = !m.captured(2).trimmed().isEmpty();

        // Skip if unknown (already reported by checkStepRefs) or not an AttributeSet
        if (!visible.hasAttributeSet(attrName)) continue;

        const QVector<QStringList> attrDef = m_index->attributeRows(attrName);
        if (attrDef.size() < 2) continue; // built-in or no definition rows

        // Build declared attribute name set and name→DataType map from data rows
        QSet<QString>          declaredAttrs;
        QMap<QString, QString> attrType;
        for (int r = 1; r < attrDef.size(); ++r) {
            if (attrDef[r].isEmpty()) continue;
            const QString aname = attrDef[r][0];
            declaredAttrs.insert(aname);
            if (attrDef[r].size() > 1)
                attrType[aname] = attrDef[r][1];
        }

        // Collect the step's table rows (skip blank lines, stop at non-pipe non-blank)
        QVector<QPair<int, QStringList>> tableRows;
        for (int j = i + 1; j < lines.size(); ++j) {
            if (reRow.match(lines[j]).hasMatch()) {
                const QStringList parts = lines[j].split('|');
                QStringList cells;
                for (int p = 1; p < parts.size() - 1; ++p)
                    cells << parts[p].trimmed();
                tableRows.append({j + 1, cells});
            } else if (lines[j].trimmed().isEmpty()) {
                continue;
            } else {
                break;
            }
        }
        if (tableRows.isEmpty()) continue;

        if (transposed) {
            // Each row is | AttributeName | Value [| Value2 | ...] |
            // Extra columns are additional list items — all valid.
            for (const auto& row : tableRows) {
                if (row.second.size() < 2) continue;
                const QString aname = row.second[0];
                if (!aname.isEmpty() && !declaredAttrs.contains(aname)) {
                    out.append(makeDiag(filePath, row.first,
                        QStringLiteral("'%1' is not an attribute of '%2'")
                            .arg(aname, attrName),
                        Diagnostic::Severity::Warning));
                }
                for (int c = 1; c < row.second.size(); ++c) {
                    const QString val = row.second[c];
                    if (!val.isEmpty() && !val.startsWith('='))
                        validateDataTypeValue(filePath, row.first, val, attrType.value(aname), out);
                }
            }
        } else {
            // First row = column headers, remaining rows = data values
            const QStringList& headers   = tableRows[0].second;
            const int          headerLine = tableRows[0].first;

            for (const QString& hdr : headers) {
                if (!hdr.isEmpty() && !declaredAttrs.contains(hdr))
                    out.append(makeDiag(filePath, headerLine,
                        QStringLiteral("'%1' is not an attribute of '%2'")
                            .arg(hdr, attrName),
                        Diagnostic::Severity::Warning));
            }

            for (int r = 1; r < tableRows.size(); ++r) {
                const QStringList& cells = tableRows[r].second;
                for (int c = 0; c < qMin(cells.size(), headers.size()); ++c) {
                    const QString& val = cells[c];
                    if (val.isEmpty() || val.startsWith('=')) continue;
                    validateDataTypeValue(filePath, tableRows[r].first,
                                          val, attrType.value(headers[c]), out);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Check — Lines whose first word is not a recognised keyword
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkUnrecognizedLines(const QString& filePath,
                                                QList<Diagnostic>& out) const
{
    static const QSet<QString> known = {
        "specification",
        "attributes", "entity",
        "define",
        "background", "cleanup",
        "scenario",
        "businessrule", "calculation", "datatype", "domainterm", "scenariogroup",
        "import", "insert",
        "examples",
        "description", "details", "constraint", "notes",
        "given", "when", "then", "and", "but"
    };

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&f);
    int lineNum    = 0;
    bool inDocStr  = false;
    while (!in.atEnd()) {
        const QString raw     = in.readLine();
        ++lineNum;
        const QString trimmed = raw.trimmed();

        if (trimmed == "\"\"\"") { inDocStr = !inDocStr; continue; }  // docstring delimiter
        if (inDocStr)                        continue;   // content inside """ ... """

        if (trimmed.isEmpty())               continue;   // blank
        if (trimmed.startsWith('#'))         continue;   // comment
        if (trimmed.startsWith('@'))         continue;   // @tag — pass-through annotation
        if (trimmed.startsWith('$'))         continue;   // $tag — generator filter tag
        if (trimmed.startsWith('|'))         continue;   // pipe row
        if (trimmed.startsWith('='))         continue;   // =DefineName reference
        // indented non-pipe line = continuation of Description/Notes/etc.
        if (raw.startsWith(' ') || raw.startsWith('\t')) continue;

        QString firstWord = trimmed.split(QRegularExpression(R"(\s+)")).first();
        if (firstWord.endsWith(':')) firstWord.chop(1);  // handle "Scenario:", "Background:", etc.

        if (!known.contains(firstWord.toLower()))
            out.append(makeDiag(filePath, lineNum,
                QStringLiteral("Unrecognized keyword '%1'").arg(firstWord),
                Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// Check — DomainTerm name declared in more than one file
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDomainTermDuplicates(const QString& filePath,
                                                   QList<Diagnostic>& out) const
{
    const auto dupes = m_index->duplicateDomainTerms();
    for (auto it = dupes.cbegin(); it != dupes.cend(); ++it) {
        for (const SymbolLocation& loc : it.value()) {
            if (loc.filePath != filePath) continue;

            QStringList others;
            for (const SymbolLocation& other : it.value()) {
                if (other.filePath == filePath) continue;
                others << QFileInfo(other.filePath).fileName()
                          + ":" + QString::number(other.line);
            }
            out.append(makeDiag(filePath, loc.line,
                QString("DomainTerm '%1' already declared in %2")
                    .arg(it.key(), others.join(", ")),
                Diagnostic::Severity::Warning));
        }
    }
}
