// Tests for choosing colours: colour wheel, colour row and popup.

#include "app/ColorRow.h"
#include "app/Settings.h"
#include "ui/ColorPopup.h"
#include "ui/ColorWheel.h"

#include <QAbstractButton>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTest>

#include <cmath>
#include <numbers>

namespace {

// A point on the wheel's circle at `degrees` (0 = right, counter-clockwise).
QPoint at(const ColorWheel &wheel, qreal radius, qreal degrees)
{
    const qreal a = degrees * std::numbers::pi / 180;
    const QPointF c = QRectF(wheel.rect()).center();
    return (c + QPointF(std::cos(a) * radius, -std::sin(a) * radius)).toPoint();
}

} // namespace

class TestColors : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Settings go to a temporary folder, never the user's own (an
        // ini file on every platform, also instead of the Windows registry).
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("nsl-snap-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_colors"));
    }

    void init()
    {
        QSettings().clear();
    }


    void stylesSurviveARestart()
    {
        // Defaults without stored values.
        QCOMPARE(Settings::frameStyle().color, FrameStyle().color);
        QCOMPARE(Settings::replaceText(), QStringLiteral("Firstname Lastname"));

        FrameStyle frame;
        frame.color = QColor(0x293049);
        frame.width = 5;
        frame.rounded = false;
        frame.badge = BadgePosition::BottomLeft;
        Settings::setFrameStyle(frame);
        QCOMPARE(Settings::frameStyle(), frame);

        TextStyle text;
        text.color = Qt::white;
        text.fontSize = 22;
        text.bold = false;
        text.box = TextBox::Filled;
        Settings::setTextStyle(text);
        QCOMPARE(Settings::textStyle(), text);

        ArrowStyle arrow;
        arrow.color = QColor(0x0090FF);
        arrow.width = 9;
        Settings::setArrowStyle(arrow);
        QCOMPARE(Settings::arrowStyle(), arrow);

        MarkerStyle marker;
        marker.color = QColor(0x7DE3F4);
        Settings::setMarkerStyle(marker);
        QCOMPARE(Settings::markerStyle(), marker);

        RedactStyle redact;
        redact.mode = RedactMode::Blur;
        redact.level = RedactLevel::Strong;
        Settings::setRedactStyle(redact);
        QCOMPARE(Settings::redactStyle(), redact);

        Settings::setReplaceText(QStringLiteral("Max Muster"));
        QCOMPARE(Settings::replaceText(), QStringLiteral("Max Muster"));
    }

    void plainFramesKeepTheNumberCorner()
    {
        FrameStyle style;
        style.badge = BadgePosition::BottomRight;
        Settings::setFrameStyle(style);
        style.badge = BadgePosition::None; // a plain frame drawn later
        Settings::setFrameStyle(style);
        QCOMPARE(Settings::frameStyle().badge, BadgePosition::BottomRight);
    }

    void brokenSettingsFallBack()
    {
        QSettings settings;
        settings.setValue("styles/frame/color", QStringLiteral("no colour"));
        settings.setValue("styles/text/box", 42);
        settings.setValue("styles/arrow/width", 500);
        QCOMPARE(Settings::frameStyle().color, FrameStyle().color);
        QCOMPARE(Settings::textStyle().box, TextBox::None);
        QCOMPARE(Settings::arrowStyle().width, 12);
    }

    void recentColorsNewestFirst()
    {
        QVERIFY(Settings::recentColors().isEmpty());
        for (int i = 0; i < 10; ++i)
            Settings::addRecentColor(QColor(i, 0, 0));
        QList<QColor> colors = Settings::recentColors();
        QCOMPARE(colors.size(), Settings::kMaxRecentColors);
        QCOMPARE(colors.first(), QColor(9, 0, 0));
        // Used again: it moves to the front, no duplicate.
        Settings::addRecentColor(QColor(5, 0, 0));
        colors = Settings::recentColors();
        QCOMPARE(colors.first(), QColor(5, 0, 0));
        QCOMPARE(colors.count(QColor(5, 0, 0)), 1);
        QCOMPARE(colors.size(), Settings::kMaxRecentColors);
    }

    void resetForgetsOnlyStyles()
    {
        FrameStyle frame;
        frame.color = QColor(0x293049);
        Settings::setFrameStyle(frame);
        Settings::addRecentColor(QColor(0x293049));
        Settings::setAdvancedMode(true);
        Settings::resetStyles();
        QCOMPARE(Settings::frameStyle().color, FrameStyle().color);
        QVERIFY(Settings::recentColors().isEmpty());
        QVERIFY(Settings::advancedMode()); // other settings stay
    }

    void popupOffersRecentColors()
    {
        QWidget anchor;
        anchor.resize(30, 30);
        anchor.show();
        ColorPopup popup(&anchor);
        popup.setRecentColors({QColor(0x293049), QColor(0x123456)});
        popup.showAt(&anchor, QColor(Qt::red));
        QSignalSpy changed(&popup, &ColorPopup::colorChanged);
        QToolButton *recent = nullptr;
        for (QToolButton *button : popup.findChildren<QToolButton *>()) {
            if (button->toolTip() == QStringLiteral("123456"))
                recent = button;
        }
        QVERIFY(recent);
        QTest::mouseClick(recent, Qt::LeftButton);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(popup.color(), QColor(0x123456));
    }

    void standardColorsAreNotRecent()
    {
        QVERIFY(ColorRow::isStandard(QColor(Qt::white)));
        QVERIFY(ColorRow::isStandard(QColor(0xFFE629))); // marker yellow
        QVERIFY(!ColorRow::isStandard(QColor(0x293049)));
    }

    void wheelKeepsTheColor()
    {
        ColorWheel wheel;
        wheel.setColor(QColor(0x293049));
        QCOMPARE(wheel.color().name(), QStringLiteral("#293049"));
    }

    void ringPicksTheHue()
    {
        ColorWheel wheel;
        wheel.resize(200, 200);
        wheel.setColor(QColor(Qt::red));
        QSignalSpy changed(&wheel, &ColorWheel::colorChanged);
        // The ring lies between radius 83 and 99; 120° is green.
        QTest::mouseClick(&wheel, Qt::LeftButton, {}, at(wheel, 91, 120));
        QCOMPARE(changed.count(), 1);
        QVERIFY(std::abs(wheel.color().hueF() - 1.0 / 3) < 0.02);
        QCOMPARE(wheel.color().saturationF(), 1.0f);
    }

    void triangleCornersAreHueWhiteAndBlack()
    {
        ColorWheel wheel;
        wheel.resize(200, 200);
        wheel.setColor(QColor(Qt::blue));
        const qreal hue = 240; // the hue corner points to the hue on the ring
        // Slightly beyond each corner: clamped onto it.
        QTest::mouseClick(&wheel, Qt::LeftButton, {}, at(wheel, 80, hue));
        QCOMPARE(wheel.color().name(), QStringLiteral("#0000ff"));
        QTest::mouseClick(&wheel, Qt::LeftButton, {}, at(wheel, 80, hue + 120));
        QCOMPARE(wheel.color().name(), QStringLiteral("#ffffff"));
        QTest::mouseClick(&wheel, Qt::LeftButton, {}, at(wheel, 80, hue + 240));
        QCOMPARE(wheel.color().name(), QStringLiteral("#000000"));
        // Black keeps the hue: back to the hue corner gives blue again.
        QTest::mouseClick(&wheel, Qt::LeftButton, {}, at(wheel, 80, hue));
        QCOMPARE(wheel.color().name(), QStringLiteral("#0000ff"));
    }

    void rowShowsOwnColorsInTheCustomButton()
    {
        ColorRow row;
        auto *custom = qobject_cast<QAbstractButton *>(row.customButton());
        row.setColor(QColor(0x293049));
        QVERIFY(custom->isChecked());
        row.setColor(QColor(Qt::white)); // a standard colour (F-25)
        QVERIFY(!custom->isChecked());
        QSignalSpy requested(&row, &ColorRow::customRequested);
        QTest::mouseClick(custom, Qt::LeftButton);
        QCOMPARE(requested.count(), 1);
        QVERIFY(!custom->isChecked()); // a click alone does not change the colour
        QCOMPARE(row.color(), QColor(Qt::white));
    }

    void popupTakesAHexCode()
    {
        QWidget anchor;
        anchor.resize(30, 30);
        anchor.show();
        ColorPopup popup(&anchor);
        popup.showAt(&anchor, QColor(Qt::red));
        QSignalSpy changed(&popup, &ColorPopup::colorChanged);
        auto *hex = popup.findChild<QLineEdit *>();
        QCOMPARE(hex->text(), QStringLiteral("FF0000"));
        hex->selectAll();
        QTest::keyClicks(hex, QStringLiteral("29304"));
        QCOMPARE(changed.count(), 0); // incomplete
        QTest::keyClicks(hex, QStringLiteral("9"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(popup.color(), QColor(0x293049));
    }

private:
    QTemporaryDir m_settingsDir;
};

QTEST_MAIN(TestColors)
#include "tst_colors.moc"
