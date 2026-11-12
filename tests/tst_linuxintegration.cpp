// The AppImage's self-setup (D-041): menu entry, icon, the `snapslash`
// link and the GNOME extension, all in a temporary home folder.

#include "app/LinuxIntegration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace LinuxIntegration;

namespace {

void write(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// A fake AppImage: the file itself and its usr/share as the build puts it.
Paths fakeAppImage(const QString &root)
{
    Paths paths;
    paths.appImage = root + QStringLiteral("/Downloads/snapslash-0.3.0-linux-x86_64.AppImage");
    paths.shareDir = root + QStringLiteral("/AppDir/usr/share");
    paths.dataHome = root + QStringLiteral("/home/.local/share");
    paths.binDir = root + QStringLiteral("/home/.local/bin");
    write(paths.appImage, "ELF");
    // The real desktop entry of the app.
    write(paths.shareDir + QStringLiteral("/applications/ch.netslash.Snapslash.desktop"),
          read(QStringLiteral(NSL_SOURCE_DIR "/resources/ch.netslash.Snapslash.desktop")));
    write(paths.shareDir + QStringLiteral("/icons/hicolor/scalable/apps/ch.netslash.Snapslash.svg"), "<svg/>");
    const QString extension = paths.shareDir + QStringLiteral("/snapslash/gnome-extension/nsl-snap@netslash.ch");
    write(extension + QStringLiteral("/metadata.json"), "{\"uuid\": \"nsl-snap@netslash.ch\"}");
    write(extension + QStringLiteral("/extension.js"), "// new");
    write(extension + QStringLiteral("/schemas/gschemas.compiled"), "GVariant");
    return paths;
}

} // namespace

class TestLinuxIntegration : public QObject
{
    Q_OBJECT

private slots:
    void execQuoting()
    {
        QCOMPARE(execProgram(QStringLiteral("/home/anna/Apps/snapslash.AppImage")),
                 QStringLiteral("/home/anna/Apps/snapslash.AppImage"));
        // Spaces need quotes; a % is a field code and is doubled.
        QCOMPARE(execProgram(QStringLiteral("/home/anna/Meine Apps/s.AppImage")),
                 QStringLiteral("\"/home/anna/Meine Apps/s.AppImage\""));
        QCOMPARE(execProgram(QStringLiteral("/tmp/100%/s")), QStringLiteral("/tmp/100%%/s"));
        // Inside quotes $ gets a backslash, which the string escape doubles.
        QCOMPARE(execProgram(QStringLiteral("/tmp/a$b/s")), QStringLiteral("\"/tmp/a\\\\$b/s\""));
    }

    void desktopEntryPointsAtTheAppImage()
    {
        const QString bundled = QString::fromUtf8(read(QStringLiteral(NSL_SOURCE_DIR "/resources/ch.netslash.Snapslash.desktop")));
        const QString entry = desktopEntry(bundled, QStringLiteral("/opt/Snap Slash/s.AppImage"));
        QVERIFY(!entry.contains(QStringLiteral("Exec=snapslash")));
        QVERIFY(entry.contains(QStringLiteral("\nExec=\"/opt/Snap Slash/s.AppImage\"\n")));
        QVERIFY(entry.contains(QStringLiteral("\nExec=\"/opt/Snap Slash/s.AppImage\" --capture-window --open\n")));
        // TryExec once, in the main group.
        QCOMPARE(entry.count(QStringLiteral("TryExec=")), 1);
        QVERIFY(entry.indexOf(QStringLiteral("TryExec=/opt/Snap Slash/s.AppImage"))
                < entry.indexOf(QStringLiteral("[Desktop Action")));
        // A TryExec in the bundled entry is replaced, not doubled.
        const QString withTryExec = QString(bundled).replace(QStringLiteral("Exec=snapslash\n"),
                                                             QStringLiteral("Exec=snapslash\nTryExec=snapslash\n"));
        QCOMPARE(desktopEntry(withTryExec, QStringLiteral("/a/s")).count(QStringLiteral("TryExec=")), 1);
    }

    void setUpWritesEntryIconAndLink()
    {
        QTemporaryDir root;
        const Paths paths = fakeAppImage(root.path());

        SetupResult result = setUpDesktop(paths);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.changed);
        const QString entry = paths.dataHome + QStringLiteral("/applications/ch.netslash.Snapslash.desktop");
        QVERIFY(read(entry).contains("Exec=" + paths.appImage.toUtf8() + "\n"));
        QCOMPARE(read(paths.dataHome + QStringLiteral("/icons/hicolor/scalable/apps/ch.netslash.Snapslash.svg")),
                 QByteArray("<svg/>"));
        const QFileInfo link(paths.binDir + QStringLiteral("/snapslash"));
        QVERIFY(link.isSymLink());
        QCOMPARE(link.symLinkTarget(), paths.appImage);

        // The second start finds everything in place.
        result = setUpDesktop(paths);
        QVERIFY(result.error.isEmpty());
        QVERIFY(!result.changed);

        // The file was moved: entry and link follow it.
        Paths moved = paths;
        moved.appImage = root.path() + QStringLiteral("/Apps/snapslash.AppImage");
        write(moved.appImage, "ELF");
        result = setUpDesktop(moved);
        QVERIFY(result.changed);
        QVERIFY(read(entry).contains("Exec=" + moved.appImage.toUtf8() + "\n"));
        QCOMPARE(QFileInfo(moved.binDir + QStringLiteral("/snapslash")).symLinkTarget(), moved.appImage);
    }

    void staleIconCacheIsRefreshed()
    {
        // Another app left an icon cache in the user's hicolor folder; our
        // icon comes later, into a subfolder, which the cache does not know.
        QTemporaryDir root;
        const Paths paths = fakeAppImage(root.path());
        const QString theme = paths.dataHome + QStringLiteral("/icons/hicolor");
        QVERIFY(QDir().mkpath(theme + QStringLiteral("/scalable/apps")));
        write(theme + QStringLiteral("/icon-theme.cache"), "an older app's cache");
        QTest::qWait(30); // so the icon is clearly newer than the cache
        const QString icon = theme + QStringLiteral("/scalable/apps/ch.netslash.Snapslash.svg");
        write(icon, "<svg/>");
        QVERIFY(iconCacheIsStale(theme, icon));

        QVERIFY(setUpDesktop(paths).error.isEmpty());
        // Rebuilt (gtk-update-icon-cache), or the folder made newer.
        QVERIFY(!iconCacheIsStale(theme, icon));
    }

    void buildFromSourceKeepsItsCommand()
    {
        QTemporaryDir root;
        const Paths paths = fakeAppImage(root.path());
        // `cmake --install --prefix ~/.local` put a real program there.
        write(paths.binDir + QStringLiteral("/snapslash"), "self-built");
        QVERIFY(setUpDesktop(paths).error.isEmpty());
        QCOMPARE(read(paths.binDir + QStringLiteral("/snapslash")), QByteArray("self-built"));
    }

    void extensionInstallAndState()
    {
        QTemporaryDir root;
        const Paths paths = fakeAppImage(root.path());
        QVERIFY(hasBundledExtension(paths));
        QCOMPARE(extensionState(paths), ExtensionState::Missing);

        QCOMPARE(installExtension(paths), QString());
        QCOMPARE(extensionState(paths), ExtensionState::Current);
        QCOMPARE(read(installedExtension(paths) + QStringLiteral("/schemas/gschemas.compiled")), QByteArray("GVariant"));

        // An older app's extension, with a file the new one no longer has.
        write(installedExtension(paths) + QStringLiteral("/extension.js"), "// old");
        write(installedExtension(paths) + QStringLiteral("/gone.js"), "");
        QCOMPARE(extensionState(paths), ExtensionState::Different);
        QCOMPARE(installExtension(paths), QString());
        QCOMPARE(extensionState(paths), ExtensionState::Current);
        QVERIFY(!QFileInfo::exists(installedExtension(paths) + QStringLiteral("/gone.js")));
        QVERIFY(!QFileInfo::exists(installedExtension(paths) + QStringLiteral(".new")));
    }

    void extensionReplacesALinkButNotItsTarget()
    {
        QTemporaryDir root;
        const Paths paths = fakeAppImage(root.path());
        // scripts/dev-install.sh links the checkout's extension.
        const QString checkout = root.path() + QStringLiteral("/checkout/nsl-snap@netslash.ch");
        write(checkout + QStringLiteral("/metadata.json"), "{}");
        QDir().mkpath(QFileInfo(installedExtension(paths)).absolutePath());
        QVERIFY(QFile::link(checkout, installedExtension(paths)));
        QCOMPARE(extensionState(paths), ExtensionState::Different);

        QCOMPARE(installExtension(paths), QString());
        QVERIFY(!QFileInfo(installedExtension(paths)).isSymLink());
        QCOMPARE(extensionState(paths), ExtensionState::Current);
        QCOMPARE(read(checkout + QStringLiteral("/metadata.json")), QByteArray("{}"));
    }

    void gsettingsLists()
    {
        QCOMPARE(parseStringList(QStringLiteral("@as []")), QStringList());
        QCOMPARE(parseStringList(QStringLiteral("['ding@rastersoft.com', 'ubuntu-dock@ubuntu.com']")),
                 (QStringList{QStringLiteral("ding@rastersoft.com"), QStringLiteral("ubuntu-dock@ubuntu.com")}));
        QCOMPARE(formatStringList({QStringLiteral("a@b"), QStringLiteral("nsl-snap@netslash.ch")}),
                 QStringLiteral("['a@b', 'nsl-snap@netslash.ch']"));
        QCOMPARE(formatStringList({}), QStringLiteral("[]"));
    }
};

QTEST_GUILESS_MAIN(TestLinuxIntegration)
#include "tst_linuxintegration.moc"
