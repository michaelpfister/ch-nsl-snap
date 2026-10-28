#include "app/CommandLine.h"
#include "app/MainWindow.h"
#include "app/PlatformTheme.h"
#include "app/SingleInstance.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>

namespace {

// Prints the answer of a request for scripts: the saved path on stdout,
// errors on stderr.
void print(const SingleInstance::Answer &answer)
{
    if (answer.outcome == CommandLine::Outcome::Saved)
        QTextStream(stdout) << answer.detail << Qt::endl;
    else if (answer.outcome == CommandLine::Outcome::Failed && !answer.detail.isEmpty())
        QTextStream(stderr) << "nsl-snap: " << answer.detail << Qt::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    // The command line first (F-48, D-034), under a plain QCoreApplication:
    // help and wrong arguments must work without a display.
    CommandLine::Parsed parsed;
    {
        QCoreApplication parser(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("nsl-snap"));
        QCoreApplication::setApplicationVersion(QStringLiteral(NSL_VERSION));
        parsed = CommandLine::parse(QCoreApplication::arguments());
        if (!parsed.error.isEmpty()) {
            QTextStream(stderr) << "nsl-snap: " << parsed.error << "\n\n" << CommandLine::helpText();
            return CommandLine::exitCode(CommandLine::Outcome::Failed);
        }
        if (parsed.showHelp) {
            QTextStream(stdout) << CommandLine::helpText();
            return 0;
        }
        if (parsed.showVersion) {
            QTextStream(stdout) << "NSL Snap " << NSL_VERSION << '\n';
            return 0;
        }
    }

    // Before Qt loads its platform theme (D-033).
    PlatformTheme::avoidBrokenGtk();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NSL Snap"));
    QApplication::setApplicationVersion(QStringLiteral(NSL_VERSION));
    QApplication::setOrganizationDomain(QStringLiteral("netslash.ch"));
    QApplication::setOrganizationName(QStringLiteral("netslash.ch")); // QSettings path
    // Lets GNOME match the window to a .desktop file of the same name later.
    QGuiApplication::setDesktopFileName(QStringLiteral("ch.netslash.Snap"));

    // NSL Snap already runs: it does the work, this start only waits.
    if (const auto answer = SingleInstance::forward(QApplication::arguments().mid(1))) {
        print(*answer);
        return CommandLine::exitCode(answer->outcome);
    }

    MainWindow window;
    const auto applyTheme = [&app, &window] {
        const Theme theme = Theme::current();
        theme.apply(app);
        window.applyTheme(theme);
    };
    applyTheme();
    // Follow the system when the user switches between light and dark.
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &window, applyTheme);

    // Later starts hand their command line to this one.
    SingleInstance instance;
    instance.listen();
    instance.setHandler([&window](const QStringList &arguments, const SingleInstance::Reply &reply) {
        const CommandLine::Parsed request = CommandLine::parse(QStringList{QStringLiteral("nsl-snap")} + arguments);
        if (!request.error.isEmpty() || request.showHelp || request.showVersion) {
            reply({CommandLine::Outcome::Failed, request.error});
            return;
        }
        window.runRequest(request.request, reply);
    });

    if (!parsed.request.capture) {
        window.show();
        return app.exec();
    }
    // Started for one capture: without --open the app ends with it. The
    // request starts once the event loop runs, so exit() always reaches it.
    const CommandLine::Request request = parsed.request;
    QTimer::singleShot(0, &window, [&window, request] {
        window.runRequest(request, [open = request.open](const SingleInstance::Answer &answer) {
            print(answer);
            if (!open || answer.outcome != CommandLine::Outcome::Opened)
                QApplication::exit(CommandLine::exitCode(answer.outcome));
        });
    });
    return app.exec();
}
