#include "SpectableParser.h"
#include "CSharpGenerator.h"

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
        "Convert a .spectable file into C# unit test scaffolding.");
    cli.addHelpOption();
    cli.addVersionOption();

    cli.addPositionalArgument("input",  "Path to the .spectable file");
    cli.addPositionalArgument("output", "Output directory for generated .cs files");

    QCommandLineOption nsOpt({ "n", "namespace" },
        "C# namespace prefix (default: gherkinexecutor)", "prefix", "gherkinexecutor");
    cli.addOption(nsOpt);

    QCommandLineOption overwriteGlueOpt("overwrite-glue",
        "Overwrite the glue file even if it exists");
    cli.addOption(overwriteGlueOpt);

    cli.process(app);

    const QStringList pos = cli.positionalArguments();
    if (pos.size() < 2) {
        std::cerr << "Usage: SpecTableConverter <input.spectable> <output-dir> [options]\n";
        return 1;
    }

    const QString inputPath  = pos[0];
    const QString outputDir  = pos[1];

    if (!QFileInfo::exists(inputPath)) {
        std::cerr << "ERROR:0:Input file not found: " << inputPath.toStdString() << "\n";
        return 1;
    }

    // Parse
    SpectableParser parser;
    SpectableFile   file = parser.parse(inputPath);

    // Print parse diagnostics
    bool hasError = false;
    for (const ParseMessage& m : file.messages) {
        const char* sev = m.warning ? "WARNING" : "ERROR";
        std::cout << sev << ":" << m.line << ":" << m.text.toStdString() << "\n";
        if (!m.warning) hasError = true;
    }

    if (hasError) return 1;

    // Generate
    CSharpGenerator::Options opts;
    opts.nsPrefix      = cli.value(nsOpt);
    opts.outputDir     = outputDir;
    opts.overwriteGlue = cli.isSet(overwriteGlueOpt);

    CSharpGenerator gen;
    const QStringList genMsgs = gen.generate(file, opts);

    for (const QString& msg : genMsgs) {
        std::cout << msg.toStdString() << "\n";
        if (msg.startsWith("ERROR")) hasError = true;
    }

    return hasError ? 1 : 0;
}
