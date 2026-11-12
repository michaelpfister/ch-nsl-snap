// The app's own dialog (AppDialog): buttons, keys, the checkbox, the
// window behind it, and the frameless window when no window is visible.

#include "ui/AppDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QPushButton>
#include <QShortcut>
#include <QTest>
#include <QTimer>

#include <functional>

namespace {

// Runs `step` once the dialog's event loop is running.
void whenOpen(const std::function<void()> &step)
{
    QTimer::singleShot(0, qApp, [step] {
        QTRY_VERIFY(AppDialog::isOpen());
        step();
    });
}

QWidget *focused()
{
    return QApplication::focusWidget();
}

QFrame *cardIn(QWidget *window)
{
    return window->findChild<QFrame *>(QStringLiteral("DialogCard"));
}

} // namespace

class TestAppDialog : public QObject
{
    Q_OBJECT

private slots:
    void clickReturnsTheButton()
    {
        QWidget window;
        window.resize(800, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Beenden?"), QStringLiteral("Text"));
        dialog.addButton(QStringLiteral("Abbrechen"), AppDialog::Role::Cancel);
        QPushButton *quit = dialog.addButton(QStringLiteral("Beenden"), AppDialog::Role::Destructive);
        bool insideWindow = false;
        whenOpen([&] {
            // The card sits in the window itself, over a layer as large as it.
            QFrame *card = cardIn(&window);
            insideWindow = card && card->isVisible();
            QTest::mouseClick(quit, Qt::LeftButton);
        });
        QCOMPARE(dialog.exec(), quit);
        QVERIFY(insideWindow);
        QVERIFY(!AppDialog::isOpen());
    }

    void enterNeverPressesTheDestructiveButton()
    {
        QWidget window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Verwerfen?"));
        QPushButton *cancel = dialog.addButton(QStringLiteral("Abbrechen"), AppDialog::Role::Cancel);
        dialog.addButton(QStringLiteral("Verwerfen"), AppDialog::Role::Destructive);
        whenOpen([&] {
            QCOMPARE(focused(), cancel);
            QTest::keyClick(focused(), Qt::Key_Return);
        });
        QCOMPARE(dialog.exec(), cancel);
    }

    void enterPressesThePrimaryAndEscapeCancels()
    {
        QWidget window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        {
            AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Einrichten?"));
            dialog.addButton(QStringLiteral("Später"), AppDialog::Role::Cancel);
            QPushButton *setUp = dialog.addButton(QStringLiteral("Einrichten"), AppDialog::Role::Primary);
            whenOpen([] { QTest::keyClick(focused(), Qt::Key_Enter); });
            QCOMPARE(dialog.exec(), setUp);
        }
        {
            AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Einrichten?"));
            QPushButton *later = dialog.addButton(QStringLiteral("Später"), AppDialog::Role::Cancel);
            dialog.addButton(QStringLiteral("Einrichten"), AppDialog::Role::Primary);
            whenOpen([] { QTest::keyClick(focused(), Qt::Key_Escape); });
            QCOMPARE(dialog.exec(), later);
        }
    }

    void tabStaysInTheCard()
    {
        QWidget window;
        auto *behind = new QPushButton(QStringLiteral("Behind"), &window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Beenden?"));
        QCheckBox *option = dialog.setCheckBox(QStringLiteral("Nicht mehr nachfragen"));
        QPushButton *cancel = dialog.addButton(QStringLiteral("Abbrechen"), AppDialog::Role::Cancel);
        QPushButton *quit = dialog.addButton(QStringLiteral("Beenden"), AppDialog::Role::Destructive);
        QList<QWidget *> visited;
        whenOpen([&] {
            for (int i = 0; i < 4; ++i) {
                QTest::keyClick(focused(), Qt::Key_Tab);
                visited.append(focused());
            }
            QTest::keyClick(focused(), Qt::Key_Escape);
        });
        QCOMPARE(dialog.exec(), cancel);
        // From "Abbrechen": "Beenden", then round to the checkbox, never behind.
        QCOMPARE(visited, (QList<QWidget *>{quit, option, cancel, quit}));
        QVERIFY(!visited.contains(behind));
    }

    void checkBoxIsReadAfterwards()
    {
        QWidget window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Beenden?"));
        QCheckBox *option = dialog.setCheckBox(QStringLiteral("Nicht mehr nachfragen"));
        dialog.addButton(QStringLiteral("Abbrechen"), AppDialog::Role::Cancel);
        QPushButton *quit = dialog.addButton(QStringLiteral("Beenden"), AppDialog::Role::Destructive);
        whenOpen([&] {
            QTest::mouseClick(option, Qt::LeftButton, {}, QPoint(8, option->height() / 2));
            QTest::mouseClick(quit, Qt::LeftButton);
        });
        QCOMPARE(dialog.exec(), quit);
        QVERIFY(option->isChecked());
    }

    void shortcutsBehindTheCardDoNothing()
    {
        QWidget window;
        int saves = 0;
        auto *save = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), &window);
        connect(save, &QShortcut::activated, &window, [&saves] { ++saves; });
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        // Without a dialog the shortcut works (so the test can see it).
        QTest::keyClick(&window, Qt::Key_S, Qt::ControlModifier);
        QCOMPARE(saves, 1);

        AppDialog dialog(&window, AppDialog::Kind::Question, QStringLiteral("Beenden?"));
        QPushButton *cancel = dialog.addButton(QStringLiteral("Abbrechen"), AppDialog::Role::Cancel);
        whenOpen([&] {
            QTest::keyClick(focused(), Qt::Key_S, Qt::ControlModifier);
            QTest::keyClick(focused(), Qt::Key_Escape);
        });
        QCOMPARE(dialog.exec(), cancel);
        QCOMPARE(saves, 1);
    }

    void withoutAVisibleWindowItIsAWindowOfItsOwn()
    {
        QWidget hidden; // the main window while the app is in the tray
        AppDialog dialog(&hidden, AppDialog::Kind::Warning, QStringLiteral("Bild speichern"), QStringLiteral("Fehler"));
        QPushButton *ok = dialog.addButton(QStringLiteral("OK"), AppDialog::Role::Primary);
        bool frameless = false;
        whenOpen([&] {
            for (QWidget *top : QApplication::topLevelWidgets()) {
                if (top->isVisible() && top->findChild<QFrame *>(QStringLiteral("DialogCard")))
                    frameless = top->windowFlags().testFlag(Qt::FramelessWindowHint);
            }
            // A message: Escape closes it. (Offscreen, the new window is not
            // active, so the key goes to the button, which passes it on.)
            QTest::keyClick(ok, Qt::Key_Escape);
        });
        QCOMPARE(dialog.exec(), ok);
        QVERIFY(frameless);
    }
};

QTEST_MAIN(TestAppDialog)
#include "tst_appdialog.moc"
