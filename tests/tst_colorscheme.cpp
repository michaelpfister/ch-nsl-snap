// Light or dark (D-045): the choice in the settings over the system's, and
// the desktop portal's values.

#include "ui/ColorScheme.h"

#include <QSignalSpy>
#include <QTest>

class TestColorScheme : public QObject
{
    Q_OBJECT

private slots:
    void portalValues()
    {
        QCOMPARE(ColorScheme::darkFromPortalValue(1), std::optional<bool>(true));
        QCOMPARE(ColorScheme::darkFromPortalValue(2), std::optional<bool>(false));
        QCOMPARE(ColorScheme::darkFromPortalValue(0), std::optional<bool>(false));
        QCOMPARE(ColorScheme::darkFromPortalValue(7), std::nullopt);
    }

    void storedNames()
    {
        for (auto choice : {ColorScheme::Choice::System, ColorScheme::Choice::Light, ColorScheme::Choice::Dark})
            QCOMPARE(ColorScheme::fromString(ColorScheme::toString(choice)), choice);
        QCOMPARE(ColorScheme::fromString(QStringLiteral("something else")), ColorScheme::Choice::System);
    }

    void choiceWinsOverTheSystem()
    {
        ColorScheme *scheme = ColorScheme::instance();
        QSignalSpy changed(scheme, &ColorScheme::changed);
        scheme->setChoice(ColorScheme::Choice::Dark);
        QVERIFY(scheme->isDark());
        scheme->setChoice(ColorScheme::Choice::Light);
        QVERIFY(!scheme->isDark());
        // Dark → light: two changes; the same choice again: none.
        QCOMPARE(changed.size(), 2);
        scheme->setChoice(ColorScheme::Choice::Light);
        QCOMPARE(changed.size(), 2);
        scheme->setChoice(ColorScheme::Choice::System);
    }
};

QTEST_MAIN(TestColorScheme)
#include "tst_colorscheme.moc"
