// Switching between light and dark (D-045) with the whole main window on
// the screen. From the open settings it froze the app for about 40 s on
// Windows: their event filter redrew the navigation icons for every
// widget's palette change, 150 times as long as without the settings.
// Machines differ a lot (a Debug build on the ARM64 runner is about 15
// times slower), so the test compares the two cases instead of a time.

#include "app/MainWindow.h"
#include "app/SettingsDialog.h"
#include "ui/ColorScheme.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QTest>
#include <QTimer>

class TestThemeSwitch : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("netslash.ch-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_themeswitch"));
    }

    void switchWithoutSettings()
    {
        auto *app = qApp;
        MainWindow window;
        Theme::light().apply(*app);
        window.applyTheme(Theme::light());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 2; ++i) {
            const Theme theme = i == 0 ? Theme::darkTheme() : Theme::light();
            theme.apply(*app);
            window.applyTheme(theme);
            QCoreApplication::processEvents();
        }
        m_plainMs = timer.elapsed();
        qInfo("Two switches took %lld ms", m_plainMs);
    }

    void switchInTheSettingsIsNotMuchSlower()
    {
        // As the user does it: the choice in the open settings overlay,
        // with the app following ColorScheme as main() wires it.
        auto *app = qApp;
        MainWindow window;
        const auto follow = [app, &window] {
            const Theme theme = Theme::current();
            theme.apply(*app);
            window.applyTheme(theme);
        };
        ColorScheme::instance()->setChoice(ColorScheme::Choice::Light);
        follow();
        QObject::connect(ColorScheme::instance(), &ColorScheme::changed, &window, follow);
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        SettingsDialog dialog(nullptr, true, &window);
        qint64 ms = -1;
        QTimer::singleShot(0, &dialog, [&] {
            QComboBox *appearance = nullptr;
            for (QComboBox *box : dialog.findChildren<QComboBox *>()) {
                if (box->accessibleName() == QStringLiteral("Erscheinungsbild"))
                    appearance = box;
            }
            QElapsedTimer timer;
            timer.start();
            if (appearance) {
                appearance->setCurrentIndex(2); // Dunkel
                QCoreApplication::processEvents();
                appearance->setCurrentIndex(1); // Hell
                QCoreApplication::processEvents();
            }
            ms = timer.elapsed();
            dialog.close();
        });
        dialog.exec();
        qInfo("Two switches in the settings took %lld ms", ms);
        QVERIFY(ms >= 0);
        // About 2 to 3 times as long now: the settings have many widgets.
        QVERIFY(m_plainMs >= 0);
        QVERIFY2(ms < 10 * m_plainMs + 500,
                 qPrintable(QStringLiteral("%1 ms against %2 ms without the settings").arg(ms).arg(m_plainMs)));
    }

private:
    qint64 m_plainMs = -1; // two switches without the settings
};

QTEST_MAIN(TestThemeSwitch)
#include "tst_themeswitch.moc"
