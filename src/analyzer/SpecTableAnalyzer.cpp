#include "SpecTableAnalyzer.h"

#include <QFile>
#include <QDir>
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
    checkDomainTermDuplicates       (filePath, diags);
    checkDomainTermColumnTypes      (filePath, m_index->domainTermTypes(), diags);
    checkDomainTermVsDataTypeNames  (filePath, visible, diags);
    checkStepsWithTableButNoAttrSet (filePath, diags);
    checkCollectionElementTypes     (filePath, visible, diags);
    checkDuplicateDeclarations      (filePath, diags);
    checkExamplesTableContents      (filePath, visible, diags);
    checkAttributeDefaultValues     (filePath, diags);
    runModelChecks                  (filePath, diags);   // PROTOTYPE

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

    // "Given/When/Then ... : AttributeSet [Vertical|CompareOnly]"
    static QRegularExpression reStepAttr(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(?:\s+(?:Vertical|CompareOnly))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "When applying BusinessRule X : Y"
    static QRegularExpression reBizRuleStep(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b.*applying\s+BusinessRule\s+(\w+)\s*:\s*(\w+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "When applying Calculation X : Y"
    static QRegularExpression reCalcStep(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b.*applying\s+Calculation\s+(\w+)\s*:\s*(\w+)\s*$)",
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
        R"(^\s*(Specification|Entity|Collection|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
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
        if (line.trimmed().startsWith('#')) continue;   // commented-out line

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
        R"(^\s*(Specification|Entity|Collection|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reInvalidStep(R"(^\s*(Given|When|WhenThen)\b)",
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
    // Decimal has no exponent, and no thousands separator: every generator reads
    // it with its language's exact-decimal type, and none of them accept a comma.
    // "25,200.00" in a Decimal column used to reach BigDecimal and throw there.
    static QRegularExpression reDecimal (R"(^[+-]?(\d+(\.\d*)?|\.\d+)$)");
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
    } else if (ltype == "decimal" || ltype == "scientific") {
        // Scientific is the one numeric type that may carry an exponent.
        const bool ok = (ltype == "scientific")
            ? reFloat.match(value).hasMatch()
            : reDecimal.match(value).hasMatch();
        if (!ok)
            out.append(makeDiag(filePath, lineNo,
                QStringLiteral("'%1' is not a valid %2").arg(value, dtype),
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
//           DataType values are valid (3.3), Vertical structure (3.4)
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkStepTableContents(const QString& filePath,
                                                const SpecTableSymbols& visible,
                                                QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reStep(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b.+:\s*(\w+)(\s+Vertical)?\s*$)",
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
        const bool    vertical = !m.captured(2).trimmed().isEmpty();

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

        // Collect the step's table rows (skip blank/comment lines, stop at non-pipe non-blank)
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
            } else if (lines[j].trimmed().startsWith('#')) {
                continue;
            } else {
                break;
            }
        }
        if (tableRows.isEmpty()) continue;

        if (vertical) {
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
// Check 9 — Given/When/Then step has a following table but no ': AttrSet' suffix
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkStepsWithTableButNoAttrSet(const QString& filePath,
                                                          QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reStep(
        R"(^\s*(?:Given|When|Then|And|WhenThen)\b)",
        QRegularExpression::CaseInsensitiveOption);
    // Capture everything after the last ':' in the line
    static QRegularExpression reColonSuffix(R"(:\s*([\w][\w\s]*)\s*$)");
    static QRegularExpression reRow(R"(^\s*\|)");
    static const QSet<QString> knownModifiers = { "compareonly", "vertical" };

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    for (int i = 0; i < lines.size() - 1; ++i) {
        if (!reStep.match(lines[i]).hasMatch()) continue;

        // Look ahead (skipping blanks and comments) for a table row
        bool hasTable = false;
        for (int j = i + 1; j < lines.size(); ++j) {
            const QString& next = lines[j];
            if (next.trimmed().isEmpty() || next.trimmed().startsWith('#')) continue;
            if (reRow.match(next).hasMatch()) hasTable = true;
            break;
        }
        if (!hasTable) continue;

        auto cm = reColonSuffix.match(lines[i]);
        if (!cm.hasMatch()) {
            // No ':' at all — no AttrSet
            out.append(makeDiag(filePath, i + 1,
                QStringLiteral("Step has a data table but no AttributeSet, Entity, or DataType — "
                               "add ': <Name>' to specify which, or create a new one"),
                Diagnostic::Severity::Warning));
            continue;
        }

        // Parse words after the ':'
        const QStringList words = cm.captured(1)
            .split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
        // words[0] = AttrSet/DataType name; words[1] (if present) = modifier
        if (words.size() >= 2) {
            const QString modifier = words[1].toLower();
            if (!knownModifiers.contains(modifier))
                out.append(makeDiag(filePath, i + 1,
                    QStringLiteral("Unrecognized step modifier '%1' — expected CompareOnly or Vertical")
                        .arg(words[1]),
                    Diagnostic::Severity::Warning));
        }
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

// ---------------------------------------------------------------------------
// Check — DomainTerm name must not collide with any DataType name
// (user-declared or built-in)
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDomainTermVsDataTypeNames(
    const QString& filePath,
    const SpecTableSymbols& visible,
    QList<Diagnostic>& out) const
{
    for (auto it = visible.domainTerms.cbegin(); it != visible.domainTerms.cend(); ++it) {
        if (it.value().filePath != filePath) continue;  // report once, at the DomainTerm's own file

        for (const QString& builtin : k_builtinDataTypes) {
            if (it.key().compare(builtin, Qt::CaseInsensitive) == 0) {
                out.append(makeDiag(filePath, it.value().line,
                    QString("DomainTerm '%1' has the same name as the built-in DataType '%2' — "
                            "DomainTerm and DataType names must be distinct")
                        .arg(it.key(), builtin),
                    Diagnostic::Severity::Warning));
                break;
            }
        }

        for (auto dtIt = visible.dataTypes.cbegin(); dtIt != visible.dataTypes.cend(); ++dtIt) {
            if (it.key().compare(dtIt.key(), Qt::CaseInsensitive) == 0) {
                out.append(makeDiag(filePath, it.value().line,
                    QString("DomainTerm '%1' has the same name as DataType '%2' (declared in %3:%4) — "
                            "DomainTerm and DataType names must be distinct")
                        .arg(it.key(), dtIt.key(),
                             QFileInfo(dtIt.value().filePath).fileName(),
                             QString::number(dtIt.value().line)),
                    Diagnostic::Severity::Warning));
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Check — DomainTerm name used as an attribute: its DataType must match
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDomainTermColumnTypes(
    const QString& filePath,
    const QMap<QString, QString>& dtTypes,
    QList<Diagnostic>& out) const
{
    if (dtTypes.isEmpty()) return;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reDecl(
        R"(^\s*(Attributes|Entity)\s+\w+\b)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow(R"(^\s*\|)");
    static QRegularExpression reSkip(
        R"(^\s*(Description|Details|Notes|Constraint|Uses|In-Out)\b)",
        QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines << in.readLine();

    for (int i = 0; i < lines.size(); ++i) {
        if (!reDecl.match(lines[i]).hasMatch()) continue;

        // Find the first pipe row (header)
        int j = i + 1;
        while (j < lines.size()
               && !reRow.match(lines[j]).hasMatch()
               && (lines[j].trimmed().isEmpty() || reSkip.match(lines[j]).hasMatch()))
            ++j;
        if (j >= lines.size() || !reRow.match(lines[j]).hasMatch()) continue;

        // Parse header columns
        const QStringList hParts = lines[j].split('|');
        QStringList headers;
        for (int p = 1; p < hParts.size() - 1; ++p)
            headers << hParts[p].trimmed();

        int attrCol = -1, typeCol = -1;
        for (int c = 0; c < headers.size(); ++c) {
            const QString h = headers[c];
            if (h.compare("Attribute", Qt::CaseInsensitive) == 0 ||
                h.compare("Name", Qt::CaseInsensitive) == 0)
                attrCol = c;
            if (h.compare("Type", Qt::CaseInsensitive) == 0 ||
                h.compare("DataType", Qt::CaseInsensitive) == 0)
                typeCol = c;
        }
        if (attrCol < 0 || typeCol < 0) continue;

        // Check each data row
        for (int k = j + 1; k < lines.size(); ++k) {
            const QString& ln = lines[k];
            if (ln.trimmed().isEmpty() || ln.trimmed().startsWith('#')) continue;
            if (!reRow.match(ln).hasMatch()) break;

            const QStringList rParts = ln.split('|');
            QStringList row;
            for (int p = 1; p < rParts.size() - 1; ++p)
                row << rParts[p].trimmed();

            if (attrCol >= row.size() || row[attrCol].isEmpty()) continue;
            const QString attrName = row[attrCol];
            if (!dtTypes.contains(attrName)) continue;

            const QString dtType = dtTypes[attrName];
            if (typeCol >= row.size() || row[typeCol].isEmpty()) continue;
            const QString declaredType = row[typeCol];

            if (dtType.compare(declaredType, Qt::CaseInsensitive) != 0)
                out.append(makeDiag(filePath, k + 1,
                    QStringLiteral("DomainTerm '%1' has type '%2' but is declared as '%3' here")
                        .arg(attrName, dtType, declaredType),
                    Diagnostic::Severity::Warning));
        }
    }
}

// ---------------------------------------------------------------------------
// Check — every Attributes/Entity field must declare a recognized type:
// a built-in, a user DataType, an Entity/Attributes/Collection name, or a
// DomainTerm. Mirrors the build-time check already done in JavaGenerator's
// parseExpr(), but runs live in the editor's Analyze pass.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Helper — the fields an AttributeSet declares, as name -> type
// ---------------------------------------------------------------------------

QMap<QString, QString> SpecTableAnalyzer::fieldTypesOf(const QString& attrSetName) const
{
    QMap<QString, QString> fields;
    if (!m_index) return fields;

    // A built-in set such as ValidValues has no declaration to read, and the
    // generator supplies its shape. Returning empty says "nothing to check".
    const QVector<QStringList> rows = m_index->attributeRows(attrSetName);
    if (rows.size() < 2) return fields;

    const QStringList& header = rows.first();
    int nameCol = -1, typeCol = -1;
    for (int c = 0; c < header.size(); ++c) {
        const QString h = header[c];
        if (h.compare("Attribute", Qt::CaseInsensitive) == 0 ||
            h.compare("Name", Qt::CaseInsensitive) == 0)
            nameCol = c;
        if (h.compare("Type", Qt::CaseInsensitive) == 0 ||
            h.compare("DataType", Qt::CaseInsensitive) == 0)
            typeCol = c;
    }
    if (nameCol < 0) return fields;

    for (int r = 1; r < rows.size(); ++r) {
        const QStringList& row = rows[r];
        if (nameCol >= row.size() || row[nameCol].isEmpty()) continue;
        fields.insert(row[nameCol],
                      (typeCol >= 0 && typeCol < row.size()) ? row[typeCol] : QString());
    }
    return fields;
}

// ---------------------------------------------------------------------------
// Check — a Collection's element type must be an Entity
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkCollectionElementTypes(const QString& filePath,
                                                     const SpecTableSymbols& visible,
                                                     QList<Diagnostic>& out) const
{
    if (!m_index) return;

    for (auto it = visible.collections.cbegin(); it != visible.collections.cend(); ++it) {
        if (it.value().filePath != filePath) continue;   // report at its own declaration

        const QString element = m_index->collectionElementType(it.key());
        if (element.isEmpty()) continue;                 // no DataType column to read

        if (visible.entities.contains(element)) continue;            // the good case
        if (k_builtinDataTypes.contains(element)) continue;          // a list of a built-in

        if (visible.attributes.contains(element)) {
            out.append(makeDiag(filePath, it.value().line,
                QStringLiteral("Collection '%1' holds '%2', which is declared with Attributes. "
                               "A Collection's type must be an Entity -- a production class is "
                               "written for an Entity and not for an Attributes block, so this "
                               "generates code referring to a type nothing writes.")
                    .arg(it.key(), element)));
            continue;
        }

        if (!visible.hasDataType(element) && !visible.hasAttributeSet(element)
            && !visible.domainTerms.contains(element))
            out.append(makeDiag(filePath, it.value().line,
                QStringLiteral("Collection '%1' holds unknown type '%2'")
                    .arg(it.key(), element)));
    }
}

// ---------------------------------------------------------------------------
// Check — a name declared in more than one file
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDuplicateDeclarations(const QString& filePath,
                                                    QList<Diagnostic>& out) const
{
    if (!m_index) return;

    struct KindName { SpecTableIndex::SymbolKind kind; const char* label; };
    static const KindName kinds[] = {
        { SpecTableIndex::SymbolKind::Entity,     "Entity"     },
        { SpecTableIndex::SymbolKind::Attributes, "Attributes" },
        { SpecTableIndex::SymbolKind::DataType,   "DataType"   },
        { SpecTableIndex::SymbolKind::Collection, "Collection" },
    };

    for (const KindName& k : kinds) {
        const auto dupes = m_index->duplicatesOfKind(k.kind);
        for (auto it = dupes.cbegin(); it != dupes.cend(); ++it) {
            for (const SymbolLocation& loc : it.value()) {
                if (loc.filePath != filePath) continue;

                QStringList others;
                for (const SymbolLocation& other : it.value()) {
                    if (other.filePath == filePath && other.line == loc.line) continue;
                    // A bare file name is no help when the other declaration is in
                    // a file of the same name in another folder -- the message then
                    // appears to point at the line you are already looking at. Show
                    // the containing folder too when the names match.
                    const QFileInfo of(other.filePath);
                    const QString shown =
                        (of.fileName() == QFileInfo(filePath).fileName())
                            ? of.dir().dirName() + "/" + of.fileName()
                            : of.fileName();
                    others << shown + ":" + QString::number(other.line);
                }
                if (others.isEmpty()) continue;

                out.append(makeDiag(filePath, loc.line,
                    QStringLiteral("%1 '%2' is also declared in %3 -- only the first declaration "
                                   "is generated, so the others are never tested")
                        .arg(QString::fromLatin1(k.label), it.key(), others.join(", ")),
                    Diagnostic::Severity::Warning));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Check — an Examples: table against the AttributeSet it names
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkExamplesTableContents(const QString& filePath,
                                                    const SpecTableSymbols& visible,
                                                    QList<Diagnostic>& out) const
{
    Q_UNUSED(visible);
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reExamples(R"(^\s*Examples:\s*(\w+))",
                                         QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow(R"(^\s*\|)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines << in.readLine();

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reExamples.match(lines[i]);
        if (!m.hasMatch()) continue;

        const QString attrSet = m.captured(1);
        const QMap<QString, QString> fields = fieldTypesOf(attrSet);
        if (fields.isEmpty()) continue;   // built-in or undeclared — checked elsewhere

        // The header is the first pipe row after the Examples: line.
        int h = i + 1;
        while (h < lines.size() && !reRow.match(lines[h]).hasMatch()) {
            const QString t = lines[h].trimmed();
            if (!t.isEmpty() && !t.startsWith('#')) break;
            ++h;
        }
        if (h >= lines.size() || !reRow.match(lines[h]).hasMatch()) continue;

        const QStringList hParts = lines[h].split('|');
        QStringList headers;
        for (int p = 1; p < hParts.size() - 1; ++p) headers << hParts[p].trimmed();

        // A column naming no field is data the generator drops on the floor.
        for (const QString& col : headers) {
            if (col.isEmpty() || fields.contains(col)) continue;
            out.append(makeDiag(filePath, h + 1,
                QStringLiteral("Examples table has column '%1', which is not an attribute of "
                               "'%2' -- its values are ignored").arg(col, attrSet),
                Diagnostic::Severity::Warning));
        }

        // A field with no column and no default is one the generator cannot
        // fill, and it refuses the file for it. A field with a default is filled
        // from the default, which is what declaring one is for, so that is not a
        // finding -- the generator is silent about it too.
        const QMap<QString, QString> defaults = fieldDefaultsOf(attrSet);
        for (auto fit = fields.cbegin(); fit != fields.cend(); ++fit) {
            if (headers.contains(fit.key())) continue;
            if (!defaults.value(fit.key()).trimmed().isEmpty()) continue;

            out.append(makeDiag(filePath, h + 1,
                QStringLiteral("Examples table for '%1' has no column '%2', and '%2' has no "
                               "default value").arg(attrSet, fit.key())));
        }

        // Each cell against the type its column declares.
        for (int r = h + 1; r < lines.size(); ++r) {
            const QString& ln = lines[r];
            if (ln.trimmed().isEmpty() || ln.trimmed().startsWith('#')) continue;
            if (!reRow.match(ln).hasMatch()) break;

            const QStringList rParts = ln.split('|');
            QStringList cells;
            for (int p = 1; p < rParts.size() - 1; ++p) cells << rParts[p].trimmed();

            for (int c = 0; c < qMin(cells.size(), headers.size()); ++c) {
                const QString& val = cells[c];
                // Blank states nothing; "=Name" is a Define reference resolved
                // later; ?DNC? is the do-not-care marker.
                if (val.isEmpty() || val.startsWith('=') || val == "?DNC?") continue;
                validateDataTypeValue(filePath, r + 1, val, fields.value(headers[c]), out);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Check — the Default column of an Attributes/Entity block
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkAttributeDefaultValues(const QString& filePath,
                                                     QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reDecl(R"(^\s*(Attributes|Entity)\s+(\w+))",
                                     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow(R"(^\s*\|)");
    static QRegularExpression reSkip(
        R"(^\s*(Description|Details|Notes|Constraint|Uses|In-Out)\b)",
        QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines << in.readLine();

    for (int i = 0; i < lines.size(); ++i) {
        if (!reDecl.match(lines[i]).hasMatch()) continue;

        int j = i + 1;
        while (j < lines.size()
               && !reRow.match(lines[j]).hasMatch()
               && (lines[j].trimmed().isEmpty() || reSkip.match(lines[j]).hasMatch()))
            ++j;
        if (j >= lines.size() || !reRow.match(lines[j]).hasMatch()) continue;

        const QStringList hParts = lines[j].split('|');
        QStringList headers;
        for (int p = 1; p < hParts.size() - 1; ++p) headers << hParts[p].trimmed();

        int typeCol = -1, defaultCol = -1;
        for (int c = 0; c < headers.size(); ++c) {
            if (headers[c].compare("Type", Qt::CaseInsensitive) == 0 ||
                headers[c].compare("DataType", Qt::CaseInsensitive) == 0)
                typeCol = c;
            if (headers[c].compare("Default", Qt::CaseInsensitive) == 0)
                defaultCol = c;
        }
        if (typeCol < 0 || defaultCol < 0) continue;

        for (int k = j + 1; k < lines.size(); ++k) {
            const QString& ln = lines[k];
            if (ln.trimmed().isEmpty() || ln.trimmed().startsWith('#')) continue;
            if (!reRow.match(ln).hasMatch()) break;

            const QStringList rParts = ln.split('|');
            QStringList row;
            for (int p = 1; p < rParts.size() - 1; ++p) row << rParts[p].trimmed();

            if (typeCol >= row.size() || defaultCol >= row.size()) continue;
            const QString value = row[defaultCol];
            // Blank means no default; "~" is the spec's explicit empty string;
            // "(none)" says there is none; "=Name" defers to a Define.
            if (value.isEmpty() || value == "~" || value.startsWith('=')
                || value.compare("(none)", Qt::CaseInsensitive) == 0) continue;

            validateDataTypeValue(filePath, k + 1, value, row[typeCol], out);
        }
    }
}

// ---------------------------------------------------------------------------
// Helper — the defaults an AttributeSet declares, as name -> default
// ---------------------------------------------------------------------------
//
// A field with a default is filled from it when a table gives no column, so a
// missing column is only a finding when there is no default to fall back on.
// That is the rule the generator applies, and this is how to ask about it.

QMap<QString, QString> SpecTableAnalyzer::fieldDefaultsOf(const QString& attrSetName) const
{
    QMap<QString, QString> defaults;
    if (!m_index) return defaults;

    const QVector<QStringList> rows = m_index->attributeRows(attrSetName);
    if (rows.size() < 2) return defaults;

    const QStringList& header = rows.first();
    int nameCol = -1, defaultCol = -1;
    for (int c = 0; c < header.size(); ++c) {
        const QString h = header[c];
        if (h.compare("Attribute", Qt::CaseInsensitive) == 0 ||
            h.compare("Name", Qt::CaseInsensitive) == 0)
            nameCol = c;
        if (h.compare("Default", Qt::CaseInsensitive) == 0)
            defaultCol = c;
    }
    if (nameCol < 0 || defaultCol < 0) return defaults;

    for (int r = 1; r < rows.size(); ++r) {
        const QStringList& row = rows[r];
        if (nameCol >= row.size() || row[nameCol].isEmpty()) continue;
        defaults.insert(row[nameCol],
                        defaultCol < row.size() ? row[defaultCol] : QString());
    }
    return defaults;
}
