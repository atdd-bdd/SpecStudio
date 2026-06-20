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
    "Character", "String", "Text", "Integer", "Float",
    "Boolean", "Date", "Time", "DateTime", "Duration", "YesNo"
};

struct SpecTableSymbols
{
    QMap<QString, QString> entities;       // name → filePath
    QMap<QString, QString> domainTerms;    // name → filePath
    QMap<QString, QString> dataTypes;      // name → filePath
    QMap<QString, QString> attributes;     // name → filePath
    QMap<QString, QString> businessRules;  // name → filePath
    QMap<QString, QString> calculations;   // name → filePath
    QMap<QString, QString> scenarios;      // name → filePath
    QMap<QString, QString> scenarioGroups; // name → filePath
    QMap<QString, QString> specifications; // name → filePath
    QMap<QString, QString> defines;        // name → filePath

    bool hasAttributeSet(const QString& name) const
    {
        return attributes.contains(name) || entities.contains(name)
               || k_builtinAttributeSets.contains(name);
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
};

// Parses one .spectable file and populates a SpecTableSymbols map.
// Imports are followed transitively (up to one level) so callers can
// resolve cross-file references.
class SpecTableIndex
{
public:
    // Parse filePath and all files it Import-s. Returns combined symbols.
    SpecTableSymbols buildFor(const QString& filePath) const;

    // Parse the entire project directory.
    void rebuildProject(const QStringList& specTableFiles);

    // Symbols visible project-wide (union of all files in rebuildProject).
    const SpecTableSymbols& projectSymbols() const { return m_project; }

    // Import / Insert paths declared in a file (absolute).
    QStringList importsFor(const QString& filePath) const;
    QStringList insertsFor(const QString& filePath) const;

    // Returns the pipe-table rows defined under "Attributes <name>" (first row = headers).
    QVector<QStringList> attributeRows(const QString& name) const;

private:
    void parseFile(const QString& filePath, SpecTableSymbols& out,
                   QSet<QString>& visited) const;

    // Cache: file → symbols declared IN that file only (no transitive imports)
    mutable QMap<QString, SpecTableSymbols> m_fileSymbols;
    mutable QMap<QString, QStringList>      m_fileImports; // absolute paths (recursively parsed)
    mutable QMap<QString, QStringList>      m_fileInserts; // absolute paths (data files, not parsed)

    SpecTableSymbols m_project;
};
