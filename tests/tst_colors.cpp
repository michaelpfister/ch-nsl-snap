// Tests for choosing colours: colour wheel, colour row and popup.

#include "app/ColorRow.h"
#include "ui/ColorPopup.h"
#include "ui/ColorWheel.h"

#include <QAbstractButton>
#include <QLineEdit>
#include <QSignalSpy>
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
};

QTEST_MAIN(TestColors)
#include "tst_colors.moc"
