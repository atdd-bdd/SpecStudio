#include "app/MainWindow.h"
#include "app/AppSettings.h"
#include "app/ThemeManager.h"
#include <QApplication>

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

    AppSettings settings;
    ThemeManager::apply(&app, settings.darkTheme());

    MainWindow w;
    w.show();

    return app.exec();
}
