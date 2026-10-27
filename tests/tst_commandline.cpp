// Tests for the command line (F-48) and the hand-over to a running app
// (D-034).

#include "app/CommandLine.h"
#include "app/SingleInstance.h"

#include <QTemporaryDir>
#include <QTest>

#include <thread>

namespace {

CommandLine::Parsed parse(const QStringList &arguments)
{
    return CommandLine::parse(QStringList{QStringLiteral("nsl-snap")} + arguments);
}

} // namespace

class TestCommandLine : public QObject
{
    Q_OBJECT

private slots:
    void noArgumentsStartsTheApp()
    {
        const CommandLine::Parsed parsed = parse({});
        QVERIFY(parsed.error.isEmpty());
        QVERIFY(!parsed.request.capture);
        QVERIFY(!parsed.request.open);
        QCOMPARE(parsed.request.delay, 0);
    }

    void captureModes()
    {
        QCOMPARE(parse({QStringLiteral("--capture-window")}).request.capture, CaptureResult::Kind::Window);
        QCOMPARE(parse({QStringLiteral("--capture-region")}).request.capture, CaptureResult::Kind::Region);
        QCOMPARE(parse({QStringLiteral("--capture-screen")}).request.capture, CaptureResult::Kind::Screen);
    }

    void openAndDelay()
    {
        const CommandLine::Parsed parsed =
            parse({QStringLiteral("--capture-window"), QStringLiteral("--open"), QStringLiteral("--delay"), QStringLiteral("3")});
        QVERIFY(parsed.error.isEmpty());
        QVERIFY(parsed.request.open);
        QCOMPARE(parsed.request.delay, 3);
        QCOMPARE(parse({QStringLiteral("--capture-region"), QStringLiteral("--delay=10")}).request.delay, 10);
    }

    void wrongArguments()
    {
        QVERIFY(!parse({QStringLiteral("--capture-window"), QStringLiteral("--capture-region")}).error.isEmpty());
        QVERIFY(!parse({QStringLiteral("--delay"), QStringLiteral("61")}).error.isEmpty());
        QVERIFY(!parse({QStringLiteral("--delay"), QStringLiteral("-1")}).error.isEmpty());
        QVERIFY(!parse({QStringLiteral("--delay"), QStringLiteral("soon")}).error.isEmpty());
        QVERIFY(!parse({QStringLiteral("--capture-everything")}).error.isEmpty());
        QVERIFY(!parse({QStringLiteral("screenshot.png")}).error.isEmpty());
    }

    void helpAndVersion()
    {
        QVERIFY(parse({QStringLiteral("--help")}).showHelp);
        QVERIFY(parse({QStringLiteral("-h")}).showHelp);
        QVERIFY(parse({QStringLiteral("--version")}).showVersion);
        QVERIFY(CommandLine::helpText().contains(QLatin1String("--capture-region")));
    }

    void exitCodes()
    {
        QCOMPARE(CommandLine::exitCode(CommandLine::Outcome::Saved), 0);
        QCOMPARE(CommandLine::exitCode(CommandLine::Outcome::Opened), 0);
        QCOMPARE(CommandLine::exitCode(CommandLine::Outcome::Cancelled), 1);
        QCOMPARE(CommandLine::exitCode(CommandLine::Outcome::Failed), 2);
    }

    void nobodyListening()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!SingleInstance::forward({QStringLiteral("--capture-region")}, dir.filePath(QStringLiteral("none.sock"))));
    }

    void handOverToTheRunningApp()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString name = dir.filePath(QStringLiteral("snap.sock"));
        SingleInstance running;
        QVERIFY(running.listen(name));

        // The running app answers later, as after the user has picked.
        QStringList received;
        SingleInstance::Reply pending;
        running.setHandler([&](const QStringList &arguments, const SingleInstance::Reply &reply) {
            received = arguments;
            pending = reply;
        });

        // forward() blocks until the answer, so the second start runs in
        // a thread of its own while this one serves the request.
        std::optional<SingleInstance::Answer> answer;
        std::thread second([&] {
            answer = SingleInstance::forward({QStringLiteral("--capture-region"), QStringLiteral("--delay"), QStringLiteral("3")},
                                             name);
        });
        QTRY_VERIFY(pending);
        QCOMPARE(received, QStringList({QStringLiteral("--capture-region"), QStringLiteral("--delay"), QStringLiteral("3")}));
        pending({CommandLine::Outcome::Saved, QStringLiteral("/home/user/Pictures/NSL Snap/Bereich.png")});
        // Keep this thread's event loop running until the answer is out.
        QTRY_VERIFY(answer.has_value());
        second.join();
        QCOMPARE(answer->outcome, CommandLine::Outcome::Saved);
        QCOMPARE(answer->detail, QStringLiteral("/home/user/Pictures/NSL Snap/Bereich.png"));
    }

    void staleSocketIsReplaced()
    {
        // An app that crashed leaves its socket file behind.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString name = dir.filePath(QStringLiteral("snap.sock"));
        {
            SingleInstance crashed;
            QVERIFY(crashed.listen(name));
        }
        SingleInstance next;
        QVERIFY(next.listen(name));
    }
};

QTEST_GUILESS_MAIN(TestCommandLine)
#include "tst_commandline.moc"
