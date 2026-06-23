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
    m_fileInserts.clear();
    m_project = {};

    for (const QString& f : specTableFiles) {
        QSet<QString> visited;
        SpecTableSymbols sym;
        parseFile(f, sym, visited);
    }

    // Merge all file-level symbols into the project-wide view
    for (const auto& sym : m_fileSymbols) {
        for (auto it = sym.entities.cbegin();       it != sym.entities.cend();       ++it) m_project.entities.insert(it.key(), it.value());
        for (auto it = sym.domainTerms.cbegin();    it != sym.domainTerms.cend();    ++it) m_project.domainTerms.insert(it.key(), it.value());
        for (auto it = sym.dataTypes.cbegin();      it != sym.dataTypes.cend();      ++it) m_project.dataTypes.insert(it.key(), it.value());
        for (auto it = sym.attributes.cbegin();     it != sym.attributes.cend();     ++it) m_project.attributes.insert(it.key(), it.value());
        for (auto it = sym.businessRules.cbegin();  it != sym.businessRules.cend();  ++it) m_project.businessRules.insert(it.key(), it.value());
        for (auto it = sym.calculations.cbegin();   it != sym.calculations.cend();   ++it) m_project.calculations.insert(it.key(), it.value());
        for (auto it = sym.scenarios.cbegin();      it != sym.scenarios.cend();      ++it) m_project.scenarios.insert(it.key(), it.value());
        for (auto it = sym.scenarioGroups.cbegin(); it != sym.scenarioGroups.cend(); ++it) m_project.scenarioGroups.insert(it.key(), it.value());
        for (auto it = sym.specifications.cbegin(); it != sym.specifications.cend(); ++it) m_project.specifications.insert(it.key(), it.value());
        for (auto it = sym.defines.cbegin();        it != sym.defines.cend();        ++it) m_project.defines.insert(it.key(), it.value());
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

QStringList SpecTableIndex::insertsFor(const QString& filePath) const
{
    return m_fileInserts.value(QFileInfo(filePath).absoluteFilePath());
}

QMap<QString, QVector<SymbolLocation>> SpecTableIndex::duplicateDomainTerms() const
{
    QMap<QString, QVector<SymbolLocation>> all;
    for (auto fit = m_fileSymbols.cbegin(); fit != m_fileSymbols.cend(); ++fit) {
        for (auto it = fit.value().domainTerms.cbegin(); it != fit.value().domainTerms.cend(); ++it)
            all[it.key()].append(it.value());
    }

    QMap<QString, QVector<SymbolLocation>> dupes;
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        if (it.value().size() > 1)
            dupes.insert(it.key(), it.value());
    return dupes;
}

QVector<QStringList> SpecTableIndex::attributeRows(const QString& name) const
{
    // Look up in Attributes declarations first, then Entity declarations
    QString filePath = m_project.attributes.value(name).filePath;
    if (filePath.isEmpty())
        filePath = m_project.entities.value(name).filePath;
    if (filePath.isEmpty()) return {};

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

    static QRegularExpression reDecl(R"(^\s*(Attributes|Entity)\s+(\w+))",
                                     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow(R"(^\s*\|)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines << in.readLine();

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reDecl.match(lines[i]);
        if (!m.hasMatch() || m.captured(2).compare(name, Qt::CaseInsensitive) != 0)
            continue;

        static QRegularExpression reSkipLine(
            R"(^\s*(Description|Details|Notes|Constraint|In-Out)\b)",
            QRegularExpression::CaseInsensitiveOption);
        static QRegularExpression reTopLevel(
            R"(^\s*(Specification|Entity|DomainTerm|DataType|Attributes|BusinessRule|Calculation|Import|Insert|Scenario|ScenarioGroup|Background|Cleanup|Define)\b)",
            QRegularExpression::CaseInsensitiveOption);

        QVector<QStringList> result;
        for (int j = i + 1; j < lines.size(); ++j) {
            const QString& ln = lines[j];
            if (!reRow.match(ln).hasMatch()) {
                if (ln.trimmed().isEmpty()) continue;
                if (!ln.isEmpty() && ln[0].isSpace()) continue; // indented continuation
                if (reSkipLine.match(ln).hasMatch()) continue;  // metadata keyword
                break; // new top-level block or unrecognised non-pipe line
            }
            const QStringList parts = ln.split('|');
            QStringList cells;
            for (int p = 1; p < parts.size() - 1; ++p)
                cells << parts[p].trimmed();
            if (!cells.isEmpty())
                result << cells;
        }
        return result;
    }
    return {};
}

QPair<QString, QVector<QStringList>> SpecTableIndex::defineInfo(const QString& name) const
{
    const QString filePath = m_project.defines.value(name).filePath;
    if (filePath.isEmpty()) return {};

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

    static QRegularExpression reDecl(R"(^\s*Define\s+(\w+)\s*(?:=\s*(.*))?$)",
                                     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reRow2(R"(^\s*\|)");

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines << in.readLine();

    for (int i = 0; i < lines.size(); ++i) {
        auto m = reDecl.match(lines[i]);
        if (!m.hasMatch() || m.captured(1).compare(name, Qt::CaseInsensitive) != 0)
            continue;

        const QString afterEq = m.captured(2).trimmed();
        if (!afterEq.isEmpty())
            return { afterEq, {} };

        // Table define — collect rows
        QVector<QStringList> rows;
        for (int j = i + 1; j < lines.size(); ++j) {
            const QString& ln = lines[j];
            if (!reRow2.match(ln).hasMatch()) {
                if (ln.trimmed().isEmpty()) continue;
                break;
            }
            QStringList parts = ln.split('|');
            QStringList cells;
            for (int p = 1; p < parts.size() - 1; ++p)
                cells << parts[p].trimmed();
            if (!cells.isEmpty())
                rows << cells;
        }
        return { {}, rows };
    }
    return {};
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

    static QRegularExpression reEntity       (R"(^\s*Entity\s+(\w+))",          QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDomainTerm   (R"(^\s*DomainTerm\s+(\w+))",      QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDataType     (R"(^\s*DataType\s+(\w+))",        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reAttributes   (R"(^\s*Attributes\s+(\w+))",      QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reBizRule      (R"(^\s*BusinessRule\s+(\w+))",    QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reCalc         (R"(^\s*Calculation\s+(\w+))",     QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reScenario     (R"(^\s*Scenario\s+(.+)$)",        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reScenarioGrp  (R"(^\s*ScenarioGroup\s+(.+)$)",   QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reSpecification(R"(^\s*Specification\s+(.+)$)",   QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDefine       (R"(^\s*Define\s+(\w+))",           QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reImport       ("^\\s*Import\\s+\"([^\"]+)\"",    QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reInsert       ("^\\s*Insert\\s+\"([^\"]+)\"",    QRegularExpression::CaseInsensitiveOption);

    SpecTableSymbols& fileSym = m_fileSymbols[abs];
    QStringList&      fileImp = m_fileImports[abs];
    QStringList&      fileIns = m_fileInserts[abs];

    QTextStream in(&f);
    int lineNum = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNum;
        QRegularExpressionMatch m;

        m = reEntity.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.entities.insert(n, loc); out.entities.insert(n, loc); continue; }

        m = reDomainTerm.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.domainTerms.insert(n, loc); out.domainTerms.insert(n, loc); continue; }

        m = reDataType.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.dataTypes.insert(n, loc); out.dataTypes.insert(n, loc); continue; }

        m = reAttributes.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.attributes.insert(n, loc); out.attributes.insert(n, loc); continue; }

        m = reBizRule.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.businessRules.insert(n, loc); out.businessRules.insert(n, loc); continue; }

        m = reCalc.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.calculations.insert(n, loc); out.calculations.insert(n, loc); continue; }

        m = reScenario.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1).trimmed(); SymbolLocation loc{abs, lineNum}; fileSym.scenarios.insert(n, loc); out.scenarios.insert(n, loc); continue; }

        m = reScenarioGrp.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1).trimmed(); SymbolLocation loc{abs, lineNum}; fileSym.scenarioGroups.insert(n, loc); out.scenarioGroups.insert(n, loc); continue; }

        m = reSpecification.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1).trimmed(); SymbolLocation loc{abs, lineNum}; fileSym.specifications.insert(n, loc); out.specifications.insert(n, loc); continue; }

        m = reDefine.match(line);
        if (m.hasMatch()) { const QString n = m.captured(1); SymbolLocation loc{abs, lineNum}; fileSym.defines.insert(n, loc); out.defines.insert(n, loc); continue; }

        m = reImport.match(line);
        if (m.hasMatch()) {
            const QString resolved = QFileInfo(
                QFileInfo(abs).absolutePath() + "/" + m.captured(1)).absoluteFilePath();
            if (!fileImp.contains(resolved))
                fileImp.append(resolved);
            parseFile(resolved, out, visited);
            continue;
        }

        m = reInsert.match(line);
        if (m.hasMatch()) {
            const QString resolved = QFileInfo(
                QFileInfo(abs).absolutePath() + "/" + m.captured(1)).absoluteFilePath();
            if (!fileIns.contains(resolved))
                fileIns.append(resolved);
        }
    }
}
