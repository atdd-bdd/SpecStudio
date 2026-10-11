#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// A cross-project .spectable file whose types are visible to this project.
struct ExternalSpectable {
    QString     file;          // path to the external .spectable (absolute or relative to config)
    QString     productionDir; // production code folder for types declared in this file
    QStringList codeImports;   // import/using statements for types in this file (wildcards OK)
};

// Configuration stored in a .specconfig JSON file at the project root.
// Controls how SpecTableConverter is invoked for .spectable files.
struct SpecConfig
{
    int     version        = 1;
    QString outputDirectory = "generated";   // relative to this .specconfig file
    QString language        = "CSharp";      // see SpecConfig::frameworksFor for the full set
    QString framework       = "MSTest";      // valid values depend on the language
    QString namespacePrefix;
    bool        overwriteGlue  = false;      // regenerate glue stubs even if they exist
    // False since 2026-10-11. The copy put a second declaration of every symbol
    // inside the folder people analyse, and Analyze reported the whole project
    // as duplicated -- 496 diagnostics over 24 files, from 12 real ones. Tick
    // the box to get the copy back where the output tree is meant to stand on
    // its own.
    bool        copySpectable  = false;      // copy the .spectable source file to the output directory
    QString     converterPath;               // empty = auto-detect next to AlignThree.exe
    QStringList imports;                     // extra import/using statements for all generated files
    QString     tagFilter;                   // boolean $tag expression — only matching blocks generated
    // A table of a DataType or a built-in reaches the glue as values of that
    // type; these say what shape. Row count is a property of the table and
    // every occurrence of a step agrees on it, so flattening a single row is
    // safe by default. Cell count is cardinality and varies between scenarios
    // of one step -- Bowling's rolls steps occur with 16, 17, 18 and 1 cells --
    // so flattening a single cell to a bare value is off unless asked for.
    bool        flattenSingleRowToList    = true;
    bool        flattenSingleCellToScalar = false;

    // Production class generation (all languages)
    bool    createProductionClasses  = false; // generate production stubs for DataTypes if not present
    QString productionClassesDir;             // output folder for production classes
    QString productionClassesPackage;         // Java package for production classes

    // End every generated glue stub with a failure, so an unimplemented step
    // cannot report success. On by default; turning it off is a deliberate
    // choice to let a fresh scaffold pass green.
    bool    failEveryTest = true;

    // Append the step's AttributeSet/Entity to its glue method name. Absent
    // from a .specconfig means false, so no existing project changes when it
    // is upgraded: turning it on renames every affected glue method, and
    // appendMissingStubs matches by name, so the old ones stay behind holding
    // their implementations. New Project writes it true.
    bool    stepNameIncludesAttrSet = false;

    // Cross-project type imports — external .spectable files whose types are visible here
    QList<ExternalSpectable> externalSpectables;

    // Load from a .specconfig JSON file; returns defaults if the file cannot be read
    static SpecConfig load(const QString& filePath);

    // Save to a .specconfig JSON file; returns true on success
    bool save(const QString& filePath) const;

    // Returns the list of frameworks valid for the given language
    static QStringList frameworksFor(const QString& language);
};
