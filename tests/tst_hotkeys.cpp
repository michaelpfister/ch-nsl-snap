// Tests for the global hotkeys (F-42, D-035): key conversion, registration
// with Windows, keys taken by another program, and the stored settings.

#include "app/GlobalHotkeys.h"
#include "app/Settings.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTest>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// A key nobody else uses, so the tests do not collide with the desktop.
const QKeySequence kRareKey(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_F24);

} // namespace

class TestHotkeys : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Own settings, so the tests never change the user's.
        QCoreApplication::setOrganizationName(QStringLiteral("netslash.ch-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_hotkeys"));
        QSettings().clear();
    }

    void cleanupTestCase()
    {
        QSettings().clear();
    }

    void defaultsLikeWinSnap()
    {
        QCOMPARE(GlobalHotkeys::defaultKey(GlobalHotkeys::Action::LastMode), QKeySequence(Qt::Key_Print));
        QCOMPARE(GlobalHotkeys::defaultKey(GlobalHotkeys::Action::Screen), QKeySequence(Qt::SHIFT | Qt::Key_Print));
        QCOMPARE(GlobalHotkeys::defaultKey(GlobalHotkeys::Action::ActiveWindow), QKeySequence(Qt::ALT | Qt::Key_Print));
        QCOMPARE(GlobalHotkeys::defaultKey(GlobalHotkeys::Action::Region),
                 QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Print));
    }

    void settingsKeepKeysAndOff()
    {
        const auto action = GlobalHotkeys::Action::Region;
        QCOMPARE(Settings::globalHotkey(action), GlobalHotkeys::defaultKey(action));
        Settings::setGlobalHotkey(action, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_R));
        QCOMPARE(Settings::globalHotkey(action), QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_R));
        // Switched off is not the same as never set.
        Settings::setGlobalHotkey(action, QKeySequence());
        QVERIFY(Settings::globalHotkey(action).isEmpty());
    }

    void toNative()
    {
#ifdef Q_OS_WIN
        unsigned modifiers = 0;
        unsigned virtualKey = 0;
        QVERIFY(GlobalHotkeys::toNative(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Print), modifiers, virtualKey));
        QCOMPARE(modifiers, unsigned(MOD_CONTROL | MOD_ALT));
        QCOMPARE(virtualKey, unsigned(VK_SNAPSHOT));
        QVERIFY(GlobalHotkeys::toNative(QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_S), modifiers, virtualKey));
        QCOMPARE(modifiers, unsigned(MOD_WIN | MOD_SHIFT));
        QCOMPARE(virtualKey, unsigned('S'));
        QVERIFY(GlobalHotkeys::toNative(QKeySequence(Qt::Key_F13), modifiers, virtualKey));
        QCOMPARE(virtualKey, unsigned(VK_F13));
        // Two keys in a row cannot be a hotkey.
        QVERIFY(!GlobalHotkeys::toNative(QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_C), modifiers, virtualKey));
#else
        QSKIP("Global hotkeys are registered by the GNOME extension (D-011).");
#endif
    }

    void takenByAnotherProgram()
    {
        if (!GlobalHotkeys::isSupported())
            QSKIP("No global hotkeys on this platform.");
        GlobalHotkeys first;
        QVERIFY(first.setKey(GlobalHotkeys::Action::LastMode, kRareKey));
        QVERIFY(!first.isTaken(GlobalHotkeys::Action::LastMode));

        // A second registration of the same key fails, as it does when
        // another program has it.
        GlobalHotkeys second;
        QSignalSpy changed(&second, &GlobalHotkeys::changed);
        QVERIFY(!second.setKey(GlobalHotkeys::Action::Screen, kRareKey));
        QVERIFY(second.isTaken(GlobalHotkeys::Action::Screen));
        QCOMPARE(second.key(GlobalHotkeys::Action::Screen), kRareKey); // stays assigned
        QCOMPARE(changed.count(), 1);

        // Suspended keys are free for others, and registered again after.
        first.setSuspended(true);
        QVERIFY(second.setKey(GlobalHotkeys::Action::Screen, kRareKey));
        second.setKey(GlobalHotkeys::Action::Screen, QKeySequence());
        first.setSuspended(false);
        QVERIFY(!first.isTaken(GlobalHotkeys::Action::LastMode));
    }

    void hotkeyMessageTriggers()
    {
#ifdef Q_OS_WIN
        // The way Windows delivers a hotkey registered without a window: a
        // message in the thread's queue, which Qt hands to the filter
        // without a result pointer (once a crash).
        GlobalHotkeys hotkeys;
        QVERIFY(hotkeys.setKey(GlobalHotkeys::Action::Screen, kRareKey));
        QSignalSpy triggered(&hotkeys, &GlobalHotkeys::triggered);
        const WPARAM screenId = 0x4E10 + WPARAM(GlobalHotkeys::Action::Screen); // kFirstId in GlobalHotkeys.cpp
        QVERIFY(PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY, screenId, 0));
        QTRY_COMPARE(triggered.count(), 1);
        QCOMPARE(triggered.first().first().value<GlobalHotkeys::Action>(), GlobalHotkeys::Action::Screen);
#else
        QSKIP("Global hotkeys are registered by the GNOME extension (D-011).");
#endif
    }

    void switchedOffIsNotTaken()
    {
        GlobalHotkeys hotkeys;
        QVERIFY(hotkeys.setKey(GlobalHotkeys::Action::Region, QKeySequence()));
        QVERIFY(!hotkeys.isTaken(GlobalHotkeys::Action::Region));
    }
};

QTEST_MAIN(TestHotkeys)
#include "tst_hotkeys.moc"
