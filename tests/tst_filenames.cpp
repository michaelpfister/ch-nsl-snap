// Tests for the file names of saved images (F-41) and the platform theme
// check that keeps the file dialogs from aborting the app (D-033).

#include "app/PlatformTheme.h"
#include "app/FileNames.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestFileNames : public QObject
{
    Q_OBJECT

private slots:
    void defaultPatternLikeBefore()
    {
        const QDateTime time(QDate(2026, 10, 26), QTime(14, 5, 9));
        QCOMPARE(FileNames::fromPattern(FileNames::defaultPattern(), QStringLiteral("Home"), time),
                 QStringLiteral("2026-10-26_14-05-09_Home.png"));
    }

    void placeholdersAndText()
    {
        const QDateTime time(QDate(2026, 1, 2), QTime(3, 4, 5));
        QCOMPARE(FileNames::fromPattern(QStringLiteral("Anleitung {Titel} {datum}"), QStringLiteral("Einstellungen"), time),
                 QStringLiteral("Anleitung_Einstellungen_2026-01-02.png"));
    }

    void unsafeCharactersAreReplaced()
    {
        const QDateTime time(QDate(2026, 1, 2), QTime(3, 4, 5));
        // Slashes, colons and the like would make folders or fail on Windows.
        QCOMPARE(FileNames::fromPattern(QStringLiteral("{Titel}"), QStringLiteral("a/b: c*d?"), time),
                 QStringLiteral("a_b_c_d.png"));
        QCOMPARE(FileNames::fromPattern(QStringLiteral("../{Titel}"), QStringLiteral("x"), time),
                 QStringLiteral("x.png"));
    }

    void emptyGivesAName()
    {
        const QDateTime time(QDate(2026, 1, 2), QTime(3, 4, 5));
        QCOMPARE(FileNames::fromPattern(QStringLiteral("{Titel}"), QString(), time), QStringLiteral("screenshot.png"));
        QCOMPARE(FileNames::fromPattern(QString(), QStringLiteral("Home"), time),
                 QStringLiteral("2026-01-02_03-04-05_Home.png")); // empty pattern: the default
        QCOMPARE(FileNames::fromPattern(QStringLiteral("Bild.png"), QString(), time), QStringLiteral("Bild.png"));
    }

    void gtkThemeNeedsItsSchema()
    {
        QTemporaryDir empty;
        QTemporaryDir withSchema;
        QVERIFY(empty.isValid() && withSchema.isValid());
        QFile compiled(withSchema.filePath(QStringLiteral("gschemas.compiled")));
        QVERIFY(compiled.open(QIODevice::WriteOnly));
        compiled.write(QByteArrayLiteral("GVariant\0\0org.gtk.Settings.FileChooser\0\0"));
        compiled.close();
        const QString none;

        // GTK asked for, no schema anywhere (NixOS without a wrapped binary).
        QVERIFY(PlatformTheme::gtkWithoutSchemas(QStringLiteral("gtk3"), QStringLiteral("Hyprland"),
                                                 {empty.path(), QStringLiteral("/nonexistent")}));
        // Qt picks GTK by itself on GTK-based desktops.
        QVERIFY(PlatformTheme::gtkWithoutSchemas(none, QStringLiteral("ubuntu:GNOME"), {empty.path()}));
        // The schema in any of the folders: GTK is fine.
        QVERIFY(!PlatformTheme::gtkWithoutSchemas(QStringLiteral("gtk3"), none, {empty.path(), withSchema.path()}));
        // No GTK at all: nothing to check.
        QVERIFY(!PlatformTheme::gtkWithoutSchemas(none, QStringLiteral("Hyprland"), {empty.path()}));
        QVERIFY(!PlatformTheme::gtkWithoutSchemas(QStringLiteral("xdgdesktopportal"), QStringLiteral("GNOME"),
                                                  {empty.path()}));
        QVERIFY(!PlatformTheme::gtkWithoutSchemas(QStringLiteral("kde:gtk3"), none, {empty.path()}));
    }
};

QTEST_MAIN(TestFileNames)
#include "tst_filenames.moc"
