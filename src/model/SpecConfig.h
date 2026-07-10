#pragma once

#include <QString>
#include <QStringList>

// Configuration stored in a .specconfig JSON file at the project root.
// Controls how SpecTableConverter is invoked for .spectable files.
struct SpecConfig
{
    int     version        = 1;
    QString outputDirectory = "generated";   // relative to this .specconfig file
    QString language        = "CSharp";      // CSharp | Java (future)
    QString framework       = "MSTest";      // MSTest | NUnit | xUnit | JUnit | TestNG
    QString namespacePrefix;
    bool        overwriteGlue  = false;      // regenerate glue stubs even if they exist
    bool        copySpectable  = true;       // copy the .spectable source file to the output directory
    QString     converterPath;               // empty = auto-detect next to SpecStudio.exe
    QStringList imports;                     // extra import/using statements for all generated files
    QString     tagFilter;                   // boolean $tag expression — only matching blocks generated

    // Production class generation (Java only)
    bool    createProductionClasses  = false; // generate production stubs for DataTypes if not present
    QString productionClassesDir;             // output folder for production classes
    QString productionClassesPackage;         // Java package for production classes

    // Load from a .specconfig JSON file; returns defaults if the file cannot be read
    static SpecConfig load(const QString& filePath);

    // Save to a .specconfig JSON file; returns true on success
    bool save(const QString& filePath) const;

    // Returns the list of frameworks valid for the given language
    static QStringList frameworksFor(const QString& language);
};
