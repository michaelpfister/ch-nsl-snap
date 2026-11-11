// Tests for the list of recent captures (F-52) and the countdown's place
// in the settings (F-04).

#include "app/CountdownPositionPicker.h"
#include "app/RecentCaptures.h"
#include "app/Settings.h"

#include <QDir>
#include <QImage>
#include <QListWidget>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

class TestRecent : public QObject
{
    Q_OBJECT

private:
    // A small PNG in the temporary folder.
    QString makeImage(const QString &name, const QColor &color = Qt::red)
    {
        QImage image(64, 40, QImage::Format_ARGB32);
        image.fill(color);
        const QString path = QDir(m_files.path()).filePath(name);
        if (!image.save(path, "PNG"))
            return {};
        return path;
    }

    QListWidget *list(RecentCaptures &recent) { return recent.findChild<QListWidget *>(); }

private slots:
    void initTestCase()
    {
        // Settings go to a temporary folder, never the user's own.
        QVERIFY(m_settingsDir.isValid());
        QVERIFY(m_files.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("nsl-snap-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_recent"));
    }

    void init()
    {
        QSettings().clear();
    }

    void settingsKeepTheNewestFirst()
    {
        QVERIFY(!Settings::autoSaveCaptures());
        Settings::setAutoSaveCaptures(true);
        QVERIFY(Settings::autoSaveCaptures());

        Settings::addRecentCapture(QStringLiteral("/a.png"));
        Settings::addRecentCapture(QStringLiteral("/b.png"));
        Settings::addRecentCapture(QStringLiteral("/a.png")); // moves up
        QCOMPARE(Settings::recentCaptures(), QStringList({QStringLiteral("/a.png"), QStringLiteral("/b.png")}));
        for (int i = 0; i < Settings::kMaxRecentCaptures + 5; ++i)
            Settings::addRecentCapture(QStringLiteral("/%1.png").arg(i));
        QCOMPARE(Settings::recentCaptures().size(), Settings::kMaxRecentCaptures);
        QCOMPARE(Settings::recentCaptures().first(), QStringLiteral("/%1.png").arg(Settings::kMaxRecentCaptures + 4));

        QVERIFY(!Settings::recentCapturesPinned());
        Settings::setRecentCapturesPinned(true);
        QVERIFY(Settings::recentCapturesPinned());
    }

    void settingsCountdown()
    {
        // Defaults: in the middle, a click cancels (as before).
        CountdownOptions options = Settings::countdownOptions();
        QCOMPARE(options.position, CountdownPosition::Center);
        QVERIFY(options.clickCancels);
        Settings::setCountdownPosition(CountdownPosition::TopRight);
        Settings::setCountdownClickCancels(false);
        options = Settings::countdownOptions();
        QCOMPARE(options.position, CountdownPosition::TopRight);
        QVERIFY(!options.clickCancels);
    }

    void listSkipsMissingFilesAndHidesWhenEmpty()
    {
        RecentCaptures recent;
        QVERIFY(recent.isHidden()); // nothing in it yet
        const QString first = makeImage(QStringLiteral("first.png"));
        const QString second = makeImage(QStringLiteral("second.png"), Qt::blue);
        recent.setPaths({first, QDir(m_files.path()).filePath(QStringLiteral("gone.png")), second});
        QCOMPARE(recent.paths(), QStringList({first, second}));
        QVERIFY(!recent.isHidden());

        QSignalSpy changed(&recent, &RecentCaptures::pathsChanged);
        recent.addCapture(second); // again: moves to the front, not twice
        QCOMPARE(recent.paths(), QStringList({second, first}));
        QCOMPARE(changed.count(), 1);

        // "Liste leeren": empty, hidden, remembered as empty.
        recent.findChildren<QToolButton *>().last()->click();
        QVERIFY(recent.paths().isEmpty());
        QVERIFY(recent.isHidden());
        QCOMPARE(changed.count(), 2);
        QVERIFY(changed.last().first().toStringList().isEmpty());
    }

    void thumbnailsLoadWhenOpened()
    {
        RecentCaptures recent;
        recent.setPaths({makeImage(QStringLiteral("thumb.png"))});
        QListWidget *items = list(recent);
        QVERIFY(items->item(0)->icon().isNull()); // closed: nothing read yet
        recent.setExpanded(true);
        QTRY_VERIFY(!items->item(0)->icon().isNull());
        const QPixmap thumb = items->item(0)->icon().pixmap(QSize(RecentCaptures::kThumbWidth, RecentCaptures::kThumbHeight));
        QCOMPARE(thumb.deviceIndependentSize().toSize(), QSize(RecentCaptures::kThumbWidth, RecentCaptures::kThumbHeight));
    }

    void clickOpensAndClosesUnlessPinned()
    {
        const QString path = makeImage(QStringLiteral("open.png"));
        for (const bool pinned : {false, true}) {
            RecentCaptures recent;
            recent.setPaths({path});
            recent.setPinned(pinned);
            recent.setExpanded(true);
            recent.show();
            QVERIFY(QTest::qWaitForWindowExposed(&recent));
            QSignalSpy open(&recent, &RecentCaptures::openRequested);
            QListWidget *items = list(recent);
            const QRect rect = items->visualItemRect(items->item(0));
            QTest::mouseClick(items->viewport(), Qt::LeftButton, {}, rect.center());
            QCOMPARE(open.count(), 1);
            QCOMPARE(open.first().first().toString(), path);
            QCOMPARE(recent.isExpanded(), pinned);
            QCOMPARE(items->isVisible(), pinned);
        }
    }

    void pinnedStaysOpen()
    {
        RecentCaptures recent;
        QSignalSpy pinned(&recent, &RecentCaptures::pinnedChanged);
        recent.setPinned(true);
        QVERIFY(recent.isExpanded());
        recent.setExpanded(false);
        QVERIFY(recent.isExpanded());
        QCOMPARE(pinned.count(), 0); // only the user's clicks are reported
    }

    void pickerChoosesByClickAndKeys()
    {
        CountdownPositionPicker picker;
        picker.resize(picker.sizeHint());
        picker.show();
        QVERIFY(QTest::qWaitForWindowExposed(&picker));
        QSignalSpy changed(&picker, &CountdownPositionPicker::positionChanged);
        QCOMPARE(picker.position(), CountdownPosition::Center);

        // The top left dot is near the top left corner of the screen area.
        QTest::mouseClick(&picker, Qt::LeftButton, {}, QPoint(picker.width() / 2 - 84 + 18, 6 + 18));
        QCOMPARE(picker.position(), CountdownPosition::TopLeft);
        QCOMPARE(changed.count(), 1);
        // A click beside every dot changes nothing.
        QTest::mouseClick(&picker, Qt::LeftButton, {}, QPoint(picker.width() / 2 - 40, 6 + 30));
        QCOMPARE(changed.count(), 1);

        // Arrows: in reading order, stopping at the ends.
        QTest::keyClick(&picker, Qt::Key_Left);
        QCOMPARE(picker.position(), CountdownPosition::TopLeft);
        QTest::keyClick(&picker, Qt::Key_Right);
        QCOMPARE(picker.position(), CountdownPosition::TopRight);
        QTest::keyClick(&picker, Qt::Key_Down);
        QCOMPARE(picker.position(), CountdownPosition::Center);
        QTest::keyClick(&picker, Qt::Key_Right);
        QTest::keyClick(&picker, Qt::Key_Right);
        QTest::keyClick(&picker, Qt::Key_Right);
        QCOMPARE(picker.position(), CountdownPosition::BottomRight);
        QCOMPARE(changed.count(), 5);
    }

private:
    QTemporaryDir m_settingsDir;
    QTemporaryDir m_files;
};

QTEST_MAIN(TestRecent)
#include "tst_recent.moc"
