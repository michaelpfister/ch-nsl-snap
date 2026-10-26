#include "app/MainWindow.h"
#include "ui/Theme.h"

#ifdef NSL_HAVE_GNOME_BACKEND
#include "app/AppService.h"
#endif

#include <QApplication>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NSL Snap"));
    QApplication::setOrganizationDomain(QStringLiteral("netslash.ch"));
    QApplication::setOrganizationName(QStringLiteral("netslash.ch")); // QSettings path
    // Lets GNOME match the window to a .desktop file of the same name later.
    QGuiApplication::setDesktopFileName(QStringLiteral("ch.netslash.Snap"));

#ifdef NSL_HAVE_GNOME_BACKEND
    // One instance: a second start shows the first one's window and ends.
    AppService service;
    if (!service.registerOrActivateOther())
        return 0;
#endif

    MainWindow window;
#ifdef NSL_HAVE_GNOME_BACKEND
    window.connectAppService(&service);
#endif
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
