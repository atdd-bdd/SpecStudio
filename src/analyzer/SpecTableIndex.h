#pragma once

#include "SpectableModel.h"
#include "SpectableParser.h"

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// Built-in AttributeSet names that need no declaration
static const QStringList k_builtinAttributeSets = { "EnumerationValues", "ValidValues" };

// The built-in DataType names are the parser's list, not a copy of it. This
// header used to keep its own, and it fell behind: Decimal was accepted by
// every generator while Analyze reported it undeclared.
inline const QStringList& k_builtinDataTypes = builtinDataTypeNames();

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
    QMap<QString, SymbolLocation> collections;
    QMap<QString, SymbolLocation> businessRules;
    QMap<QString, SymbolLocation> calculations;
    QMap<QString, SymbolLocation> scenarios;
    QMap<QString, SymbolLocation> scenarioGroups;
    QMap<QString, SymbolLocation> specifications;
    QMap<QString, SymbolLocation> defines;

    bool hasAttributeSet(const QString& name) const
    {
        return attributes.contains(name) || entities.contains(name)
               || collections.contains(name)
               || k_builtinAttributeSets.contains(name);
    }

    bool hasCollection(const QString& name) const
    {
        return collections.contains(name);
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
                                &collections, &businessRules, &calculations, &scenarios,
                                &scenarioGroups, &specifications, &defines }) {
            if (m->contains(name)) return m->value(name).filePath;
        }
        return {};
    }

    // Returns the declaration location of the named symbol, or an invalid location.
    SymbolLocation locationFor(const QString& name) const
    {
        for (const auto* m : { &entities, &domainTerms, &dataTypes, &attributes,
                                &collections, &businessRules, &calculations, &scenarios,
                                &scenarioGroups, &specifications, &defines }) {
            if (m->contains(name)) return m->value(name);
        }
        return {};
    }
};

// The project-wide symbol table, read off the converter's parse trees. Every
// file is parsed once per rebuildProject(); each question below is answered
// from those trees rather than by reading a file again.
class SpecTableIndex
{
public:
    // The symbols filePath declares, and those of every file it Imports.
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

    // The file as the converter sees it before generating: its own parse tree
    // with every other project and external file merged in as context, then
    // DomainTerms and Define references resolved. Analyze's model checks read
    // this, so a name declared in a sibling is visible to them exactly as it
    // is to the generators. Each file is parsed once per rebuildProject().
    SpectableFile fileWithContext(const QString& filePath) const;

    // The file's own tree, as parsed at the last rebuildProject(); null if the
    // file was not part of it.
    const SpectableFile* parsedFile(const QString& filePath) const;

    // Returns true if filePath was loaded as an external file in the last rebuildProject().
    bool isExternalFile(const QString& absFilePath) const;

    // Import / Insert paths declared in a file (absolute).
    QStringList importsFor(const QString& filePath) const;
    QStringList insertsFor(const QString& filePath) const;

    // The fields declared under "Attributes <name>" or "Entity <name>", as table
    // rows: a header of Name | DataType | Default | Notes (and In-Out when any
    // field uses it), then one row per field in that order. The parser's
    // reading, so the columns are always in this order whatever the file wrote.
    QVector<QStringList> attributeRows(const QString& name) const;

    // Returns the element type (the "DataType" column value) of a "Collection <name>"
    // declaration, e.g. Collection ItemCollection containing OrderItem -> "OrderItem".
    // Empty if name isn't a known Collection or its declaration can't be read.
    QString collectionElementType(const QString& name) const;

    // Returns the content of a Define declaration.
    // scalar: first == value string, second is empty.
    // table:  first is empty, second holds the pipe-table rows.
    QPair<QString, QVector<QStringList>> defineInfo(const QString& name) const;

    // Returns all DomainTerm names declared in more than one file, with all their locations.
    QMap<QString, QVector<SymbolLocation>> duplicateDomainTerms() const;

    // The declarable kinds a name can be duplicated across files in. Kept
    // separate from DomainTerm, which has its own stricter rule.
    enum class SymbolKind { Entity, Attributes, DataType, Collection };

    // Returns names of the given kind declared in more than one file, with every
    // location. The generator tests only the first declaration it sees and warns
    // about the rest, so a second declaration is silently not tested.
    QMap<QString, QVector<SymbolLocation>> duplicatesOfKind(SymbolKind kind) const;

    // Returns a map of DomainTerm name → declared base type (from "DomainTerm Foo : Bar").
    // Only entries that have an explicit ": Type" are included.
    QMap<QString, QString> domainTermTypes() const;

private:
    // file -> symbols declared IN that file only (no transitive imports)
    QMap<QString, SpecTableSymbols> m_fileSymbols;

    // The converter's own reading of each file, keyed by absolute path.
    QMap<QString, SpectableFile>    m_parsed;

    SpecTableSymbols m_project;
    QSet<QString>    m_externalFilePaths; // absolute paths of files added as external
};
