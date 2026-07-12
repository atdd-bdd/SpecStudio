#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// Built-in AttributeSet names that need no declaration
static const QStringList k_builtinAttributeSets = { "EnumerationValues", "ValidValues" };

// Built-in DataType names that need no declaration
static const QStringList k_builtinDataTypes = {
    "Character", "String", "Text", "Integer", "Float", "Scientific",
    "Boolean", "Date", "Time", "DateTime", "Duration", "YesNo"
};

struct SymbolLocation {
    QString filePath;
    int     line = 0;
};

struct SpecTableSymbols
{
    QMap<QString, SymbolLocation> entities;
    QMap<QString, SymbolLocation> domainTerms;
    QMap<QString, SymbolLocation> dataTypes;
    QMap<QString, SymbolLocation> attributes;
    QMap<QString, SymbolLocation> businessRules;
    QMap<QString, SymbolLocation> calculations;
    QMap<QString, SymbolLocation> scenarios;
    QMap<QString, SymbolLocation> scenarioGroups;
    QMap<QString, SymbolLocation> specifications;
    QMap<QString, SymbolLocation> defines;

    bool hasAttributeSet(const QString& name) const
    {
        return attributes.contains(name) || entities.contains(name)
               || k_builtinAttributeSets.contains(name);
    }

    bool hasDataType(const QString& name) const
    {
        return dataTypes.contains(name) || k_builtinDataTypes.contains(name);
    }

    bool hasBusinessRule(const QString& name) const
    {
        return businessRules.contains(name);
    }

    bool hasCalculation(const QString& name) const
    {
        return calculations.contains(name);
    }

    bool hasDefine(const QString& name) const
    {
        return defines.contains(name);
    }

    // Returns the file path where the symbol is declared (empty if not found).
    QString filePathFor(const QString& name) const
    {
        for (const auto* m : { &entities, &domainTerms, &dataTypes, &attributes,
                                &businessRules, &calculations, &scenarios,
                                &scenarioGroups, &specifications, &defines }) {
            if (m->contains(name)) return m->value(name).filePath;
        }
        return {};
    }

    // Returns the declaration location of the named symbol, or an invalid location.
    SymbolLocation locationFor(const QString& name) const
    {
        for (const auto* m : { &entities, &domainTerms, &dataTypes, &attributes,
                                &businessRules, &calculations, &scenarios,
                                &scenarioGroups, &specifications, &defines }) {
            if (m->contains(name)) return m->value(name);
        }
        return {};
    }
};

// Parses one .spectable file and populates a SpecTableSymbols map.
// Imports are followed transitively (up to one level) so callers can
// resolve cross-file references.
class SpecTableIndex
{
public:
    // Parse filePath and all files it Import-s. Returns combined symbols.
    SpecTableSymbols buildFor(const QString& filePath) const;

    // Parse the entire project directory. externalFiles are parsed for symbol visibility
    // only — their symbols are available project-wide but marked as external.
    void rebuildProject(const QStringList& specTableFiles,
                        const QStringList& externalFiles = {});

    // Symbols visible project-wide (union of all files in rebuildProject).
    const SpecTableSymbols& projectSymbols() const { return m_project; }

    // Returns symbols declared directly in filePath (no transitive imports).
    // Only populated after rebuildProject() has been called.
    SpecTableSymbols symbolsForFile(const QString& filePath) const;

    // Returns true if filePath was loaded as an external file in the last rebuildProject().
    bool isExternalFile(const QString& absFilePath) const;

    // Import / Insert paths declared in a file (absolute).
    QStringList importsFor(const QString& filePath) const;
    QStringList insertsFor(const QString& filePath) const;

    // Returns the pipe-table rows defined under "Attributes <name>" (first row = headers).
    QVector<QStringList> attributeRows(const QString& name) const;

    // Returns the content of a Define declaration.
    // scalar: first == value string, second is empty.
    // table:  first is empty, second holds the pipe-table rows.
    QPair<QString, QVector<QStringList>> defineInfo(const QString& name) const;

    // Returns all DomainTerm names declared in more than one file, with all their locations.
    QMap<QString, QVector<SymbolLocation>> duplicateDomainTerms() const;

private:
    void parseFile(const QString& filePath, SpecTableSymbols& out,
                   QSet<QString>& visited) const;

    // Cache: file → symbols declared IN that file only (no transitive imports)
    mutable QMap<QString, SpecTableSymbols> m_fileSymbols;
    mutable QMap<QString, QStringList>      m_fileImports; // absolute paths (recursively parsed)
    mutable QMap<QString, QStringList>      m_fileInserts; // absolute paths (data files, not parsed)

    SpecTableSymbols m_project;
    QSet<QString>    m_externalFilePaths; // absolute paths of files added as external
};
