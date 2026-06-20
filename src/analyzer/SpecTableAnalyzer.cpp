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

    // Build the symbol set visible from this file (own declarations + imports)
    const SpecTableSymbols visible = m_index->buildFor(filePath);

    checkImports     (filePath, visible, diags);
    checkInserts     (filePath, diags);
    checkStepRefs    (filePath, visible, diags);
    checkDescriptions(filePath, diags);
    checkExamples    (filePath, visible, diags);
    checkDefineRefs  (filePath, visible, diags);

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

        // Generic step with AttributeSet reference
        m = reStepAttr.match(line);
        if (m.hasMatch()) {
            const QString attrName = m.captured(1);
            if (!visible.hasAttributeSet(attrName))
                out.append(makeDiag(filePath, lineNum,
                    QStringLiteral("Unknown AttributeSet or Entity '%1'").arg(attrName)));
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
        R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Define)\b)",
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
