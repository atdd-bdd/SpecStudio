#include "app/MainWindow.h"
#include "app/AppSettings.h"
#include "app/ThemeManager.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SpecStudio");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("SpecStudio");

    AppSettings settings;
    ThemeManager::apply(&app, settings.darkTheme());

    MainWindow w;
    w.show();

    return app.exec();
}
