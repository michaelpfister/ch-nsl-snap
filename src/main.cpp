#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NSL Snap"));
    QApplication::setOrganizationDomain(QStringLiteral("netslash.ch"));
    // Lets GNOME match the window to a .desktop file of the same name later.
    QGuiApplication::setDesktopFileName(QStringLiteral("ch.netslash.Snap"));

    MainWindow window;
    window.show();
    return app.exec();
}
