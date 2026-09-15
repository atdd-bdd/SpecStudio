#include "app/MainWindow.h"
#include "app/AppSettings.h"
#include "app/ThemeManager.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // Not "AlignThree", on purpose. These two names decide where QSettings puts
    // the INI, so renaming them to match the product would point the application
    // at an empty folder and lose every existing setting: window geometry,
    // recent solutions, per-project git configuration. They are never displayed
    // -- the window title and the About box are in MainWindow.
    app.setApplicationName("SpecStudio");
    app.setOrganizationName("SpecStudio");
    // SPECSTUDIO_VERSION comes from project() in the top-level CMakeLists, so
    // the running application and the packaging scripts can never disagree
    // about which release this is.
    app.setApplicationVersion(SPECSTUDIO_VERSION);

    // Two positional arguments, so that a shell, a shortcut or a double-click
    // can land somewhere useful instead of an empty window.
    //
    //     AlignThree                                 as before
    //     AlignThree Work.sspec                      a solution
    //     AlignThree Work.sspec Orders.spectable     and a file within it
    //     AlignThree Orders.spectable                a file, solution found for it
    //
    // The third form is what makes registering a .spectable file association
    // worth doing: the file names its own attribute sets but not its siblings',
    // not the .specconfig, and not the project its steps resolve against, so
    // MainWindow walks up for the .sspec that owns it and opens that first.
    QCommandLineParser cli;
    cli.setApplicationDescription(
        QCoreApplication::translate("main",
            "AlignThree - an IDE for executable specifications.\n\n"
            "With no arguments it opens as it was left. Given a solution it opens "
            "that; given a file inside a solution it opens the solution that "
            "contains the file, and then the file."));
    cli.addHelpOption();
    cli.addVersionOption();
    cli.addPositionalArgument(
        QCoreApplication::translate("main", "solution-or-file"),
        QCoreApplication::translate("main",
            "A .sspec solution, or a file inside one."));
    cli.addPositionalArgument(
        QCoreApplication::translate("main", "file"),
        QCoreApplication::translate("main",
            "Optional. A file to open, when the first argument is a solution."));
    cli.process(app);

    const QStringList args = cli.positionalArguments();

    AppSettings settings;
    ThemeManager::apply(&app, settings.darkTheme());

    MainWindow w;
    w.show();

    // After show(), so that anything it needs to report appears over the window
    // rather than on its own, and so a solution that takes a moment to load does
    // not look like a failure to start.
    if (!args.isEmpty())
        w.openFromCommandLine(args.value(0), args.value(1));

    return app.exec();
}
