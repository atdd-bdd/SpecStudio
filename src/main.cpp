#include "app/MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SpecStudio");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("SpecStudio");

    MainWindow w;
    w.show();

    return app.exec();
}
