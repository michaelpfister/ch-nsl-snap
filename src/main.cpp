#include "app/MainWindow.h"
#include "app/PlatformTheme.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    // Before Qt loads its platform theme (D-033).
    PlatformTheme::avoidBrokenGtk();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NSL Snap"));
    QApplication::setOrganizationDomain(QStringLiteral("netslash.ch"));
    QApplication::setOrganizationName(QStringLiteral("netslash.ch")); // QSettings path
    // Lets GNOME match the window to a .desktop file of the same name later.
    QGuiApplication::setDesktopFileName(QStringLiteral("ch.netslash.Snap"));

    MainWindow window;
    const auto applyTheme = [&app, &window] {
        const Theme theme = Theme::current();
        theme.apply(app);
        window.applyTheme(theme);
    };
    applyTheme();
    // Follow the system when the user switches between light and dark.
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &window, applyTheme);

    window.show();
    return app.exec();
}
