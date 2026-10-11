// Runs the IDE's Analyze over a folder of .spectable files and prints what it
// finds, one diagnostic per line.
//
// Analyze lives behind a menu item, so the only way to see its output was to
// open the IDE and look. That makes it awkward to check a change against every
// specification in a project, and awkward to prove a new check finds what it
// should without also finding things it should not. This does both from a
// terminal.
//
//     analyze_cli <folder> [--errors-only] [--include-generated]
//
// Generated output is skipped. A project build writes its tests into
// <project>/generated/ and the converter copies the .spectable source in
// beside them, so analysing the project folder otherwise finds every
// specification twice and reports every symbol in it as a duplicate -- 496
// diagnostics over 24 files, from 12 real ones. --include-generated analyses
// them anyway, for the rare case where that folder holds something real.
//
// Exit code is the number of diagnostics, capped at 250, so a script can tell
// "found nothing" from "found something" without parsing the output.

#include "analyzer/SpecTableAnalyzer.h"
#include "analyzer/SpecTableIndex.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        err << "usage: analyze_cli <folder> [--errors-only]\n";
        return 2;
    }

    // Absolute, because the index keys every symbol by absolute path and the
    // cross-file checks compare paths: given a relative folder, a duplicate
    // declared in two files was silently not reported.
    const QString folder = QFileInfo(args[1]).absoluteFilePath();
    const bool errorsOnly = args.contains("--errors-only");
    const bool includeGenerated = args.contains("--include-generated");

    if (!QFileInfo(folder).isDir()) {
        err << "not a folder: " << folder << "\n";
        return 2;
    }

    // Every .spectable under the folder, at any depth -- the project's own
    // subfolders hold specifications too.
    QStringList files;
    QStringList skipped;
    QDirIterator it(folder, { "*.spectable" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        // "generated" is the default outputDirectory, and the copy of the source
        // that lands there is a copy, not a second declaration.
        const QString rel = QDir(folder).relativeFilePath(path);
        if (!includeGenerated && rel.split('/').contains(QStringLiteral("generated"))) {
            skipped << rel;
            continue;
        }
        files << path;
    }
    files.sort();
    skipped.sort();

    if (files.isEmpty()) {
        err << "no .spectable files under " << folder << "\n";
        return 2;
    }

    // Analyze resolves symbols project-wide, so the index has to see every file
    // before any one of them is analysed.
    SpecTableIndex index;
    index.rebuildProject(files);
    SpecTableAnalyzer analyzer(&index);

    int count = 0;
    for (const QString& file : files) {
        const QList<Diagnostic> diags = analyzer.analyzeFile(file);
        for (const Diagnostic& d : diags) {
            if (errorsOnly && d.severity != Diagnostic::Severity::Error) continue;
            // Relative to the folder being analysed, so that both halves of
            // "X is also declared in Y" are paths of the same kind. A bare name
            // here against a path in the message made a copy of a file look
            // like the file itself.
            out << QDir(folder).relativeFilePath(d.filePath) << ":" << d.line
                << ": " << d.severityString() << ": " << d.message << "\n";
            ++count;
        }
    }

    out << "-- " << count << " diagnostic(s) over " << files.size() << " file(s)";
    if (!skipped.isEmpty())
        out << ", " << skipped.size()
            << " generated copy/copies skipped (--include-generated to analyse them)";
    out << "\n";
    out.flush();
    return count > 250 ? 250 : count;
}
