#include "app/MainWindow.h"
#include "app/AppSettings.h"
#include "app/ThemeManager.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SpecStudio");
    // SPECSTUDIO_VERSION comes from project() in the top-level CMakeLists, so
    // the running application and the packaging scripts can never disagree
    // about which release this is.
    app.setApplicationVersion(SPECSTUDIO_VERSION);
    app.setOrganizationName("SpecStudio");

    AppSettings settings;
    ThemeManager::apply(&app, settings.darkTheme());

    MainWindow w;
    w.show();

    return app.exec();
}
