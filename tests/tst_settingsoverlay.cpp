// The settings as an overlay in the window (D-044): the card always fits
// the window, the navigation moves on top when narrow, Escape and a click
// beside the card close it, and the window's shortcuts rest meanwhile.

#include "app/SettingsDialog.h"

#include <QApplication>
#include <QFrame>
#include <QShortcut>
#include <QTest>
#include <QTimer>

#include <functional>

namespace {

void whenOpen(const std::function<void()> &step)
{
    QTimer::singleShot(0, qApp, [step] {
        QTRY_VERIFY(SettingsDialog::isOpen());
        step();
    });
}

QFrame *card(SettingsDialog &dialog)
{
    return dialog.findChild<QFrame *>(QStringLiteral("SettingsCard"));
}

} // namespace

class TestSettingsOverlay : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Settings of the test, not of the user.
        QCoreApplication::setOrganizationName(QStringLiteral("netslash.ch-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_settingsoverlay"));
    }

    void cardFitsTheWindowAndGoesNarrow()
    {
        QWidget window;
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        SettingsDialog dialog(nullptr, true, &window);
        QRect wide, small;
        bool narrowWide = true, narrowSmall = false;
        whenOpen([&] {
            wide = card(dialog)->geometry();
            narrowWide = card(dialog)->property("narrow").toBool();
            window.resize(600, 420);
            QTRY_COMPARE(dialog.size(), window.size());
            small = card(dialog)->geometry();
            narrowSmall = card(dialog)->property("narrow").toBool();
            QTest::keyClick(&dialog, Qt::Key_Escape);
        });
        dialog.exec();
        QVERIFY(!SettingsDialog::isOpen());
        QCOMPARE(wide.size(), QSize(880, 640));
        QVERIFY(QRect(0, 0, 1280, 800).contains(wide));
        QVERIFY(!narrowWide);
        QVERIFY(QRect(0, 0, 600, 420).contains(small));
        QVERIFY(narrowSmall);
    }

    void clickBesideTheCardCloses()
    {
        QWidget window;
        window.resize(1000, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        SettingsDialog dialog(nullptr, true, &window);
        bool insideKeepsItOpen = false;
        whenOpen([&] {
            const QRect c = card(dialog)->geometry();
            QTest::mouseClick(&dialog, Qt::LeftButton, {}, c.center());
            insideKeepsItOpen = SettingsDialog::isOpen();
            QTest::mouseClick(&dialog, Qt::LeftButton, {}, QPoint(5, 5));
        });
        dialog.exec();
        QVERIFY(insideKeepsItOpen);
        QVERIFY(!SettingsDialog::isOpen());
    }

    void shortcutsOfTheWindowRest()
    {
        QWidget window;
        int saves = 0;
        auto *save = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), &window);
        connect(save, &QShortcut::activated, &window, [&saves] { ++saves; });
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::keyClick(&window, Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(saves, 1);
        SettingsDialog dialog(nullptr, true, &window);
        whenOpen([&] {
            QTest::keyClick(&dialog, Qt::Key_S, Qt::ControlModifier);
            dialog.close();
        });
        dialog.exec();
        QCOMPARE(saves, 1);
    }
};

QTEST_MAIN(TestSettingsOverlay)
#include "tst_settingsoverlay.moc"
