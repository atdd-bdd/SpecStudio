// Prints the suggestion lists the .spectable editor offers, so a change to them
// can be checked from a terminal instead of by typing in the IDE.
//
//     complete_cli <folder> [file.spectable ...]
//
// The folder is indexed the way Analyze indexes a project -- every .spectable
// under it, generated/ skipped -- and the three index-backed lists are printed
// from it: the general popup, the step-colon dropdown (attribute sets) and the
// Type column dropdown. Naming files as well prints the step phrases harvested
// from each of those documents, which is the one list no rebuild feeds.
//
// Passing more than one folder, separated by --and, indexes them together, as
// Analyze > Solution does for a solution of several projects.

#include "analyzer/SpecTableIndex.h"
#include "editors/CompletionLists.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

// The same static list SpecTableEditor's constructor installs.
static const QStringList k_staticKeywords = {
    "Specification ", "Entity ", "Collection ", "DomainTerm ", "DataType ", "Attributes ",
    "BusinessRule ", "Calculation ", "Import ", "Insert ", "Define ",
    "Scenario ", "ScenarioGroup ", "Background ", "Cleanup ",
    "Description ", "Details ", "Constraint ", "Uses ",
    "Examples: EnumerationValues", "Examples: ValidValues", "Examples: ",
    "Vertical",
    "Given ", "When ", "Then ", "And ", "WhenThen ",
    "applying BusinessRule ", "applying Calculation ",
    "Character", "String", "Text", "Integer", "Float", "Scientific", "Decimal",
    "Boolean", "Date", "Time", "DateTime", "Duration", "YesNo",
    "EnumerationValues", "ValidValues",
};

static QStringList specTablesUnder(const QString& folder)
{
    QStringList files;
    QDirIterator it(folder, { "*.spectable" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel  = QDir(folder).relativeFilePath(path);
        if (rel.split('/').contains(QStringLiteral("generated"))) continue;
        files << QFileInfo(path).absoluteFilePath();
    }
    files.sort();
    return files;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        err << "usage: complete_cli <folder> [--and <folder> ...] [file.spectable ...]\n";
        return 2;
    }

    QStringList folders;
    QStringList docs;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--and") continue;
        const QFileInfo fi(args[i]);
        if (fi.isDir())       folders << fi.absoluteFilePath();
        else if (fi.isFile()) docs    << fi.absoluteFilePath();
        else { err << "no such file or folder: " << args[i] << "\n"; return 2; }
    }
    if (folders.isEmpty()) { err << "name at least one folder\n"; return 2; }

    QStringList files;
    for (const QString& f : folders) files += specTablesUnder(f);
    if (files.isEmpty()) { err << "no .spectable files found\n"; return 2; }

    SpecTableIndex index;
    index.rebuildProject(files, {});
    const SpecTableSymbols& syms = index.projectSymbols();

    out << "indexed " << files.size() << " file(s) under "
        << folders.size() << " folder(s)\n\n";

    const QStringList words    = specTableCompletionWords(syms, k_staticKeywords);
    const QStringList attrSets = specTableAttrSetWords(syms);
    const QStringList types    = specTableTypeWords(syms);

    out << "popup words (" << words.size() << ", "
        << (words.size() - k_staticKeywords.size()) << " from the index):\n";
    for (const QString& w : words)
        if (!k_staticKeywords.contains(w)) out << "  " << w << "\n";

    out << "\nstep-colon dropdown -- attribute sets (" << attrSets.size() << "):\n";
    for (const QString& w : attrSets) out << "  " << w << "\n";

    out << "\nType column dropdown (" << types.size() << "):\n";
    for (const QString& w : types) out << "  " << w << "\n";

    for (const QString& doc : docs) {
        QFile f(doc);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err << "cannot open " << doc << "\n";
            continue;
        }
        const QString text = QTextStream(&f).readAll();
        const QStringList phrases = harvestStepPhrases(text, words);
        out << "\nstep phrases in " << QFileInfo(doc).fileName()
            << " (" << phrases.size() << "):\n";
        for (const QString& p : phrases) out << "  " << p << "\n";
    }

    out.flush();
    return 0;
}
