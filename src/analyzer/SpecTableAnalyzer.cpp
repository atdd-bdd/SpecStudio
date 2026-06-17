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

    checkImports       (filePath, visible, diags);
    checkStepRefs      (filePath, visible, diags);
    checkDescriptions  (filePath, diags);
    checkDataTypeTables(filePath, diags);

    return diags;
}

// ---------------------------------------------------------------------------
// Check 1 — Import paths exist and are .spectable files
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkImports(const QString& filePath,
                                      const SpecTableSymbols&,
                                      QList<Diagnostic>& out) const
{
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

        if (!importedPath.endsWith(".spectable", Qt::CaseInsensitive))
            out.append(makeDiag(filePath, lineNum,
                QStringLiteral("Import '%1' is not a .spectable file").arg(importedPath),
                Diagnostic::Severity::Warning));

        if (!QFile::exists(resolved))
            out.append(makeDiag(filePath, lineNum,
                QStringLiteral("Imported file not found: '%1'").arg(importedPath)));
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
// Check 3 — BusinessRule, DataType, Calculation should have a description (* line)
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDescriptions(const QString& filePath,
                                           QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reDecl(
        R"(^\s*(BusinessRule|DataType|Calculation)\s+(\w+))",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDesc(R"(^\s*\*.+)");
    static QRegularExpression reBlank(R"(^\s*$)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reDecl.match(lines[i]);
        if (!m.hasMatch()) continue;

        // Look ahead: skip blank lines, expect a * description within 3 lines
        bool found = false;
        for (int j = i + 1; j < qMin(i + 4, lines.size()); ++j) {
            if (reDesc.match(lines[j]).hasMatch()) { found = true; break; }
            if (!reBlank.match(lines[j]).hasMatch()) break;
        }
        if (!found)
            out.append(makeDiag(filePath, i + 1,
                QStringLiteral("%1 '%2' has no description (* line)")
                    .arg(m.captured(1), m.captured(2)),
                Diagnostic::Severity::Warning));
    }
}

// ---------------------------------------------------------------------------
// Check 4 — DataType tables must have "Value" and "Valid" columns
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDataTypeTables(const QString& filePath,
                                             QList<Diagnostic>& out) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reDataType(R"(^\s*DataType\s+(\w+))",
                                         QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reTableRow(R"(^\s*\|)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reDataType.match(lines[i]);
        if (!m.hasMatch()) continue;

        const QString name = m.captured(1);

        // Find the first table header row following this declaration
        for (int j = i + 1; j < qMin(i + 8, lines.size()); ++j) {
            if (!reTableRow.match(lines[j]).hasMatch()) continue;

            const QString header = lines[j].toLower();
            const bool hasValue = header.contains("value");
            const bool hasValid = header.contains("valid");

            if (!hasValue || !hasValid)
                out.append(makeDiag(filePath, j + 1,
                    QStringLiteral("DataType '%1' table must have 'Value' and 'Valid' columns")
                        .arg(name)));
            break;
        }
    }
}
