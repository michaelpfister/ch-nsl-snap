#include "app/CommandLine.h"
#include "app/Language.h"
#include "app/MainWindow.h"
#include "app/PlatformTheme.h"
#include "app/Settings.h"
#include "app/SingleInstance.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_WIN
#define NOMINMAX // keep std::min and std::max usable
#include <windows.h>

#include <cstdio>
#endif

namespace {

// On Windows the app is a GUI program (no console window of its own). Started
// from a terminal, it borrows that terminal for the command line's output
// (F-48): --help, the saved path and errors. Started from Explorer or the
// Start menu there is no terminal and nothing happens.
void attachToParentConsole()
{
#ifdef Q_OS_WIN
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    // Point the C streams, which QTextStream(stdout) uses, at the terminal.
    FILE *stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
#endif
}

// Prints the answer of a request for scripts: the saved path on stdout,
// errors on stderr.
void print(const SingleInstance::Answer &answer)
{
    if (answer.outcome == CommandLine::Outcome::Saved)
        QTextStream(stdout) << answer.detail << Qt::endl;
    else if (answer.outcome == CommandLine::Outcome::Failed && !answer.detail.isEmpty())
        QTextStream(stderr) << "snapslash: " << answer.detail << Qt::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    attachToParentConsole();

    // The command line first (F-48, D-034), under a plain QCoreApplication:
    // help and wrong arguments must work without a display.
    CommandLine::Parsed parsed;
    {
        QCoreApplication parser(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("snapslash"));
        QCoreApplication::setApplicationVersion(QStringLiteral(NSL_VERSION));
        parsed = CommandLine::parse(QCoreApplication::arguments());
        if (!parsed.error.isEmpty()) {
            QTextStream(stderr) << "snapslash: " << parsed.error << "\n\n" << CommandLine::helpText();
            return CommandLine::exitCode(CommandLine::Outcome::Failed);
        }
        if (parsed.showHelp) {
            QTextStream(stdout) << CommandLine::helpText();
            return 0;
        }
        if (parsed.showVersion) {
            QTextStream(stdout) << "Snapslash " << NSL_VERSION << '\n';
            return 0;
        }
    }

    // Before Qt loads its platform theme (D-033).
    PlatformTheme::avoidBrokenGtk();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Snapslash"));
    QApplication::setApplicationVersion(QStringLiteral(NSL_VERSION));
    QApplication::setOrganizationDomain(QStringLiteral("netslash.ch"));
    QApplication::setOrganizationName(QStringLiteral("netslash.ch")); // QSettings path
    // Lets the desktop match the window to resources/ch.netslash.Snapslash.desktop
    // (Wayland), which also gives it its icon there (D-036).
    QGuiApplication::setDesktopFileName(QStringLiteral("ch.netslash.Snapslash"));
    // The icon on Windows and X11, which take it from the window.
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/logo.svg")));
    // German or English (D-038), before any window or message is made.
    Language::install(app, Language::resolve(Settings::language(), QLocale::system().uiLanguages()));

    // Snapslash already runs: it does the work, this start only waits.
    if (const auto answer = SingleInstance::forward(QApplication::arguments().mid(1))) {
        print(*answer);
        return CommandLine::exitCode(answer->outcome);
    }

    Settings::migrateFromFormerName();
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
        const CommandLine::Parsed request = CommandLine::parse(QStringList{QStringLiteral("snapslash")} + arguments);
        if (!request.error.isEmpty() || request.showHelp || request.showVersion) {
            reply({CommandLine::Outcome::Failed, request.error});
            return;
        }
        window.runRequest(request.request, reply);
    });

    if (!parsed.request.capture && !parsed.request.record) {
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
