#include "SpecTableIndex.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

// ---------------------------------------------------------------------------
// SpecTableIndex
// ---------------------------------------------------------------------------

void SpecTableIndex::rebuildProject(const QStringList& specTableFiles)
{
    m_fileSymbols.clear();
    m_fileImports.clear();
    m_project = {};

    for (const QString& f : specTableFiles) {
        QSet<QString> visited;
        SpecTableSymbols sym;
        parseFile(f, sym, visited);
    }

    // Merge all file-level symbols into the project-wide view
    for (const auto& sym : m_fileSymbols) {
        for (auto it = sym.entities.cbegin();      it != sym.entities.cend();      ++it) m_project.entities.insert(it.key(), it.value());
        for (auto it = sym.domainTerms.cbegin();   it != sym.domainTerms.cend();   ++it) m_project.domainTerms.insert(it.key(), it.value());
        for (auto it = sym.dataTypes.cbegin();     it != sym.dataTypes.cend();     ++it) m_project.dataTypes.insert(it.key(), it.value());
        for (auto it = sym.attributes.cbegin();    it != sym.attributes.cend();    ++it) m_project.attributes.insert(it.key(), it.value());
        for (auto it = sym.businessRules.cbegin(); it != sym.businessRules.cend(); ++it) m_project.businessRules.insert(it.key(), it.value());
        for (auto it = sym.calculations.cbegin();  it != sym.calculations.cend();  ++it) m_project.calculations.insert(it.key(), it.value());
        for (auto it = sym.constraints.cbegin();   it != sym.constraints.cend();   ++it) m_project.constraints.insert(it.key(), it.value());
    }
}

SpecTableSymbols SpecTableIndex::buildFor(const QString& filePath) const
{
    QSet<QString> visited;
    SpecTableSymbols result;
    parseFile(filePath, result, visited);
    return result;
}

QStringList SpecTableIndex::importsFor(const QString& filePath) const
{
    return m_fileImports.value(QFileInfo(filePath).absoluteFilePath());
}

void SpecTableIndex::parseFile(const QString& filePath,
                               SpecTableSymbols& out,
                               QSet<QString>& visited) const
{
    const QString abs = QFileInfo(filePath).absoluteFilePath();
    if (visited.contains(abs)) return;
    visited.insert(abs);

    QFile f(abs);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    static QRegularExpression reEntity     (R"(^\s*Entity\s+(\w+))",       QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDomainTerm (R"(^\s*DomainTerm\s+(\w+))",   QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDataType   (R"(^\s*DataType\s+(\w+))",     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reAttributes (R"(^\s*Attributes\s+(\w+))",   QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reBizRule    (R"(^\s*BusinessRule\s+(\w+))", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reCalc       (R"(^\s*Calculation\s+(\w+))",  QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reConstraint (R"(^\s*Constraint\s+(\w+))",   QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reImport     ("^\\s*Import\\s+\"([^\"]+)\"", QRegularExpression::CaseInsensitiveOption);

    SpecTableSymbols& fileSym = m_fileSymbols[abs];
    QStringList&      fileImp = m_fileImports[abs];

    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        QRegularExpressionMatch m;

        m = reEntity.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.entities.insert(n, abs); out.entities.insert(n, abs); continue; }

        m = reDomainTerm.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.domainTerms.insert(n, abs); out.domainTerms.insert(n, abs); continue; }

        m = reDataType.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.dataTypes.insert(n, abs); out.dataTypes.insert(n, abs); continue; }

        m = reAttributes.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.attributes.insert(n, abs); out.attributes.insert(n, abs); continue; }

        m = reBizRule.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.businessRules.insert(n, abs); out.businessRules.insert(n, abs); continue; }

        m = reCalc.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.calculations.insert(n, abs); out.calculations.insert(n, abs); continue; }

        m = reConstraint.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); fileSym.constraints.insert(n, abs); out.constraints.insert(n, abs); continue; }

        m = reImport.match(line);
        if (m.hasMatch()) {
            const QString resolved = QFileInfo(
                QFileInfo(abs).absolutePath() + "/" + m.captured(1)).absoluteFilePath();
            if (!fileImp.contains(resolved))
                fileImp.append(resolved);
            parseFile(resolved, out, visited);
        }
    }
}
