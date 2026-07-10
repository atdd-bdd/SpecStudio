#include "SpectableParser.h"
#include "CSharpGenerator.h"
#include "JavaGenerator.h"
#include "RustGenerator.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <iostream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("SpecTableConverter");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser cli;
    cli.setApplicationDescription(
        "Convert a .spectable file into unit test scaffolding.");
    cli.addHelpOption();
    cli.addVersionOption();

    cli.addPositionalArgument("input",  "Path to the .spectable file");
    cli.addPositionalArgument("output", "Output directory for generated files");

    QCommandLineOption langOpt({ "l", "language" },
        "Target language: CSharp (default), Java, or Rust", "language", "CSharp");
    cli.addOption(langOpt);

    QCommandLineOption fwOpt({ "f", "framework" },
        "Test framework: MSTest/NUnit/xUnit (C#) or JUnit/TestNG (Java). "
        "Default: MSTest for C#, JUnit for Java", "framework", "");
    cli.addOption(fwOpt);

    QCommandLineOption nsOpt({ "n", "namespace" },
        "Namespace/package prefix (empty = no prefix)", "prefix", "");
    cli.addOption(nsOpt);

    QCommandLineOption overwriteGlueOpt("overwrite-glue",
        "Overwrite the glue file even if it exists");
    cli.addOption(overwriteGlueOpt);

    QCommandLineOption noCopyOpt("no-copy-spectable",
        "Do not copy the .spectable source file to the output directory");
    cli.addOption(noCopyOpt);

    QCommandLineOption ctxOpt("context",
        "Additional .spectable files whose Attributes/Entities/Defines are globally visible "
        "(no code generated for them). May be specified multiple times.", "file");
    cli.addOption(ctxOpt);

    QCommandLineOption srcRootOpt("source-root",
        "Project source root; used by Java generator to derive package from subfolder path.",
        "dir", "");
    cli.addOption(srcRootOpt);

    QCommandLineOption importOpt("import",
        "Extra import/using statement injected into all generated files. "
        "May be specified multiple times.", "statement");
    cli.addOption(importOpt);

    QCommandLineOption tagFilterOpt("tag-filter",
        "Boolean $tag expression; only matching Scenario/BusinessRule/Calculation/DataType "
        "blocks are generated. E.g.: \"smoke AND NOT wip\". Empty = generate all.",
        "expr", "");
    cli.addOption(tagFilterOpt);

    QCommandLineOption prodDirOpt("prod-dir",
        "Output folder for production classes (Java only). "
        "When set, generates a production class/enum for each DataType if not already present.",
        "dir", "");
    cli.addOption(prodDirOpt);

    QCommandLineOption prodPkgOpt("prod-package",
        "Java package for generated production classes.", "package", "");
    cli.addOption(prodPkgOpt);

    cli.process(app);

    const QStringList pos = cli.positionalArguments();
    if (pos.size() < 2) {
        std::cerr << "Usage: SpecTableConverter <input.spectable> <output-dir> [options]\n";
        return 1;
    }

    const QString inputPath = pos[0];
    const QString outputDir = pos[1];
    const QString language  = cli.value(langOpt).trimmed();
    QString framework       = cli.value(fwOpt).trimmed();

    if (!QFileInfo::exists(inputPath)) {
        std::cerr << "ERROR:0:Input file not found: " << inputPath.toStdString() << "\n";
        return 1;
    }

    // Parse primary file
    SpectableParser parser;
    SpectableFile   file = parser.parse(inputPath);

    // Merge global context: Attributes/Entities/Defines/DataTypes from other project files
    for (const QString& ctxPath : cli.values(ctxOpt)) {
        if (!QFileInfo::exists(ctxPath)) continue;
        SpectableFile ctx = parser.parse(ctxPath);
        for (AttrSet& as : ctx.attrSets)         { as.isContext = true; file.attrSets.push_back(as); }
        for (Define& def : ctx.defines)           { def.isContext = true; file.defines.push_back(def); }
        for (NamedBlock& nb : ctx.namedBlocks)    { nb.isContext = true; file.namedBlocks.push_back(nb); }
        for (const QString& dt : ctx.dataTypeNames)
            if (!file.dataTypeNames.contains(dt)) file.dataTypeNames.push_back(dt);
    }

    // Emit FILE: before any messages so the IDE's output parser attributes
    // warnings to this file even when multiple converters run concurrently.
    std::cout << "FILE:" << inputPath.toStdString() << "\n";

    bool hasError = false;
    for (const ParseMessage& m : file.messages) {
        const char* sev = m.warning ? "WARNING" : "ERROR";
        std::cout << sev << ":" << m.line << ":" << m.text.toStdString() << "\n";
        if (!m.warning) hasError = true;
    }
    if (hasError) return 1;

    // Generate
    QStringList genMsgs;

    if (language.compare("Java", Qt::CaseInsensitive) == 0) {
        if (framework.isEmpty()) framework = "JUnit";
        JavaGenerator::Options opts;
        opts.packagePrefix = cli.value(nsOpt);
        opts.outputDir     = outputDir;
        opts.sourceRoot    = cli.value(srcRootOpt);
        opts.overwriteGlue = cli.isSet(overwriteGlueOpt);
        opts.copySpectable = !cli.isSet(noCopyOpt);
        opts.framework     = framework;
        opts.extraImports  = cli.values(importOpt);
        opts.tagFilter     = cli.value(tagFilterOpt);
        opts.productionClassesDir     = cli.value(prodDirOpt);
        opts.productionClassesPackage = cli.value(prodPkgOpt);
        opts.createProductionClasses  = !opts.productionClassesDir.isEmpty();
        JavaGenerator gen;
        genMsgs = gen.generate(file, opts);
    } else if (language.compare("Rust", Qt::CaseInsensitive) == 0) {
        RustGenerator::Options opts;
        opts.cratePrefix   = cli.value(nsOpt);
        opts.outputDir     = outputDir;
        opts.overwriteGlue = cli.isSet(overwriteGlueOpt);
        opts.copySpectable = !cli.isSet(noCopyOpt);
        opts.extraUses     = cli.values(importOpt);
        opts.tagFilter     = cli.value(tagFilterOpt);
        RustGenerator gen;
        genMsgs = gen.generate(file, opts);
    } else {
        // Default: C#
        if (framework.isEmpty()) framework = "MSTest";
        CSharpGenerator::Options opts;
        opts.nsPrefix      = cli.value(nsOpt);
        opts.outputDir     = outputDir;
        opts.sourceRoot    = cli.value(srcRootOpt);
        opts.overwriteGlue = cli.isSet(overwriteGlueOpt);
        opts.copySpectable = !cli.isSet(noCopyOpt);
        opts.extraImports  = cli.values(importOpt);
        opts.tagFilter     = cli.value(tagFilterOpt);
        CSharpGenerator gen;
        genMsgs = gen.generate(file, opts);
    }

    for (const QString& msg : genMsgs) {
        std::cout << msg.toStdString() << "\n";
        if (msg.startsWith("ERROR")) hasError = true;
    }

    return hasError ? 1 : 0;
}
