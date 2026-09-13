// Runs the IDE's Analyze over a folder of .spectable files and prints what it
// finds, one diagnostic per line.
//
// Analyze lives behind a menu item, so the only way to see its output was to
// open the IDE and look. That makes it awkward to check a change against every
// specification in a project, and awkward to prove a new check finds what it
// should without also finding things it should not. This does both from a
// terminal.
//
//     analyze_cli <folder> [--errors-only]
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

    const QString folder = args[1];
    const bool errorsOnly = args.contains("--errors-only");

    if (!QFileInfo(folder).isDir()) {
        err << "not a folder: " << folder << "\n";
        return 2;
    }

    // Every .spectable under the folder, at any depth -- the project's own
    // subfolders hold specifications too.
    QStringList files;
    QDirIterator it(folder, { "*.spectable" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) files << it.next();
    files.sort();

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
            out << QFileInfo(d.filePath).fileName() << ":" << d.line
                << ": " << d.severityString() << ": " << d.message << "\n";
            ++count;
        }
    }

    out << "-- " << count << " diagnostic(s) over " << files.size() << " file(s)\n";
    out.flush();
    return count > 250 ? 250 : count;
}
