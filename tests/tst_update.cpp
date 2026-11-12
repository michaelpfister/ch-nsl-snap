// Updates and downgrades (F-49, D-037): versions, GitHub's release list,
// checksums, and on Windows the folder swap with the real PowerShell script.

#include "app/UpdateDialog.h"
#include "update/Releases.h"
#include "update/UpdateChecker.h"
#include "update/UpdateInstaller.h"
#include "update/Version.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

Version v(const char *text)
{
    return Version::parse(QString::fromLatin1(text)).value();
}

// Three releases, a draft and a tag that is no version, as GitHub's API
// answers (shortened). A file, since moc trips over long raw strings.
QByteArray releasesJson()
{
    QFile file(QStringLiteral(NSL_TEST_DATA "/releases.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class TestUpdate : public QObject
{
    Q_OBJECT

private slots:
    void versionsParse()
    {
        QCOMPARE(v("v1.2.3").toString(), QStringLiteral("1.2.3"));
        QCOMPARE(v("0.3.0-beta.1").prerelease, QStringList({QStringLiteral("beta"), QStringLiteral("1")}));
        QVERIFY(v("0.3.0-beta.1").isPrerelease());
        for (const char *bad : {"1.2", "1.2.3.4", "01.2.3", "v1.2.3+build", "nightly", "", "1.2.3-"})
            QVERIFY2(!Version::parse(QString::fromLatin1(bad)), bad);
    }

    void versionsCompareAfterSemver()
    {
        // The order from semver.org, item 11.
        const QList<Version> ordered = {v("1.0.0-alpha"), v("1.0.0-alpha.1"), v("1.0.0-alpha.beta"),
                                        v("1.0.0-beta"), v("1.0.0-beta.2"), v("1.0.0-beta.11"),
                                        v("1.0.0-rc.1"), v("1.0.0"), v("1.0.1"), v("1.1.0"), v("2.0.0")};
        for (qsizetype i = 0; i + 1 < ordered.size(); ++i)
            QVERIFY2(ordered[i] < ordered[i + 1], qPrintable(ordered[i].toString() + " < " + ordered[i + 1].toString()));
        QVERIFY(v("0.10.0") > v("0.9.0")); // numbers, not text
        QCOMPARE(v("v1.2.3"), v("1.2.3"));
    }

    void currentVersionIsValid()
    {
        QVERIFY(Version::parse(QStringLiteral(NSL_VERSION)));
    }

    void releasesParse()
    {
        const auto releases = Releases::parse(releasesJson());
        QVERIFY(releases);
        // Without the draft and the tag that is no version; newest first.
        QCOMPARE(releases->size(), 3);
        QCOMPARE(releases->at(0).tag, QStringLiteral("v0.3.0-beta.1"));
        QCOMPARE(releases->at(1).tag, QStringLiteral("v0.2.0"));
        QCOMPARE(releases->at(2).tag, QStringLiteral("v0.1.0"));
        QCOMPARE(releases->at(2).title, QStringLiteral("Snapslash 0.1.0")); // empty name
        QVERIFY(releases->at(0).prerelease);
        QCOMPARE(releases->at(1).published, QDateTime(QDate(2026, 11, 10), QTime(10, 0), QTimeZone::UTC));

        const Releases::Asset *zip = releases->at(1).asset(QStringLiteral("snapslash-0.2.0-windows-x64.zip"));
        QVERIFY(zip);
        QCOMPARE(zip->size, 1234);
        QCOMPARE(zip->sha256, QByteArray("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
        QVERIFY(releases->at(1).asset(Releases::checksumFileName())->sha256.isEmpty());

        QVERIFY(!Releases::parse("{\"message\": \"Not Found\"}"));
        QVERIFY(!Releases::parse("<html>"));
        QCOMPARE(Releases::parse("[]")->size(), 0);
    }

    void newestUpdateRespectsPrereleases()
    {
        const QList<Releases::Release> releases = *Releases::parse(releasesJson());
        QCOMPARE(Releases::newerThan(releases, v("0.1.0"), false)->tag, QStringLiteral("v0.2.0"));
        QCOMPARE(Releases::newerThan(releases, v("0.1.0"), true)->tag, QStringLiteral("v0.3.0-beta.1"));
        QVERIFY(!Releases::newerThan(releases, v("0.2.0"), false));
        QVERIFY(!Releases::newerThan(releases, v("0.3.0"), true));
        // A beta tester gets the release that follows the beta.
        QVERIFY(!Releases::newerThan(releases, v("0.3.0-beta.1"), false));
    }

    void offeredVersionsForDowngrade()
    {
        const QList<Releases::Release> releases = *Releases::parse(releasesJson());
        QCOMPARE(Releases::offered(releases, false, v("0.2.0")).size(), 2);
        QCOMPARE(Releases::offered(releases, true, v("0.2.0")).size(), 3);
        // The installed pre-release stays in the list, so it can be reinstalled.
        QCOMPARE(Releases::offered(releases, false, v("0.3.0-beta.1")).size(), 3);
    }

    void checksumsParse()
    {
        const QByteArray text = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789  snapslash-0.2.0-windows-x64.zip\n"
                                "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789 *snapslash-0.2.0-windows-arm64.zip\r\n"
                                "not a checksum line\n";
        const QHash<QString, QByteArray> sums = Releases::parseChecksums(text);
        QCOMPARE(sums.size(), 2);
        QCOMPARE(sums.value(QStringLiteral("snapslash-0.2.0-windows-arm64.zip")),
                 QByteArray("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
    }

    void installationKind()
    {
        using Releases::Installation;
        QCOMPARE(Releases::installation(QStringLiteral("C:/Tools/Snapslash"), true, true), Installation::PortableWindows);
        QCOMPARE(Releases::installation(QStringLiteral("C:/repo/build"), true, false), Installation::Other);
        QCOMPARE(Releases::installation(QStringLiteral("/nix/store/abc-snapslash-0.2.0/bin"), false, false), Installation::Nix);
        QCOMPARE(Releases::installation(QStringLiteral("/home/user/ch-nsl-snap/build"), false, false), Installation::Other);
        QCOMPARE(Releases::installation(QStringLiteral("/tmp/.mount_snapslXYZ/usr/bin"), false, false, true),
                 Installation::AppImage);
        QCOMPARE(Releases::linuxAssetName(v("0.3.0-beta.3")), QStringLiteral("snapslash-0.3.0-beta.3-linux-x86_64.AppImage"));
        QCOMPARE(Releases::windowsAssetName(v("0.2.0"), QStringLiteral("x86_64")), QStringLiteral("snapslash-0.2.0-windows-x64.zip"));
        QCOMPARE(Releases::windowsAssetName(v("0.2.0"), QStringLiteral("arm64")), QStringLiteral("snapslash-0.2.0-windows-arm64.zip"));
    }

    void checkOnceADay()
    {
        const QDateTime now(QDate(2026, 11, 3), QTime(12, 0), QTimeZone::UTC);
        QVERIFY(UpdateChecker::isDue(QDateTime(), now));
        QVERIFY(!UpdateChecker::isDue(now.addSecs(-3600), now));
        QVERIFY(UpdateChecker::isDue(now.addDays(-1), now));
        QVERIFY(UpdateChecker::isDue(now.addDays(2), now)); // clock set back
    }

    void instructionsNameTheVersion()
    {
        const Releases::Release release = Releases::parse(releasesJson())->at(1);
        const QString nix = UpdateDialog::instructions(Releases::Installation::Nix, release);
        QVERIFY(nix.contains(QStringLiteral("github:michaelpfister/ch-nsl-snap/v0.2.0")));
        QVERIFY(UpdateDialog::instructions(Releases::Installation::Other, release).contains(QStringLiteral("0.2.0")));
    }

    void missingPackageFails()
    {
        // v0.1.0 has no files at all.
        UpdateInstaller installer;
        QTemporaryDir dir;
        installer.setApplicationDir(dir.filePath(QStringLiteral("Snapslash")));
        QSignalSpy failed(&installer, &UpdateInstaller::failed);
        installer.prepare(Releases::parse(releasesJson())->at(2));
        QCOMPARE(failed.size(), 1);
    }

    void wrongChecksumIsRejected()
    {
        // Downloads work with file:// URLs too, so this needs no network.
        QTemporaryDir dir;
        const QString zipPath = dir.filePath(QStringLiteral("package.zip"));
        QFile zip(zipPath);
        QVERIFY(zip.open(QIODevice::WriteOnly));
        zip.write("not the expected content");
        zip.close();

        Releases::Release release;
        release.version = v("0.2.0");
        Releases::Asset asset;
        asset.name = Releases::windowsAssetName(release.version);
        asset.url = QUrl::fromLocalFile(zipPath);
        asset.sha256 = QCryptographicHash::hash("other content", QCryptographicHash::Sha256).toHex();
        release.assets.append(asset);

        UpdateInstaller installer;
        installer.setApplicationDir(dir.filePath(QStringLiteral("Snapslash")));
        QSignalSpy failed(&installer, &UpdateInstaller::failed);
        QSignalSpy ready(&installer, &UpdateInstaller::ready);
        installer.prepare(release);
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().first().toString().contains(QStringLiteral("Prüfsumme")));
        QCOMPARE(ready.size(), 0);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("Snapslash.update.zip"))));
    }

#ifdef Q_OS_WIN
    // The whole Windows path with the real tar.exe and PowerShell: a ZIP
    // with a "new version" replaces a fake app folder.
    void portableFolderIsReplaced()
    {
        QTemporaryDir dir;
        const QString appDir = dir.filePath(QStringLiteral("Snapslash"));
        QVERIFY(QDir().mkpath(appDir));
        QFile oldFile(appDir + QStringLiteral("/version.txt"));
        QVERIFY(oldFile.open(QIODevice::WriteOnly));
        oldFile.write("old");
        oldFile.close();

        // The package: Snapslash/snapslash.exe and version.txt, zipped by tar.
        const QString source = dir.filePath(QStringLiteral("source"));
        QVERIFY(QDir().mkpath(source + QStringLiteral("/Snapslash")));
        for (const auto &[name, content] : {std::pair{"snapslash.exe", "exe"}, std::pair{"version.txt", "new"}}) {
            QFile file(source + QStringLiteral("/Snapslash/") + QLatin1String(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(content);
        }
        const QString zipPath = dir.filePath(QStringLiteral("package.zip"));
        QProcess tar;
        tar.setWorkingDirectory(source);
        tar.start(qEnvironmentVariable("SystemRoot") + QStringLiteral("\\System32\\tar.exe"),
                  {QStringLiteral("-a"), QStringLiteral("-cf"), QDir::toNativeSeparators(zipPath), QStringLiteral("Snapslash")});
        QVERIFY(tar.waitForFinished(30000));
        QCOMPARE(tar.exitCode(), 0);

        QFile zip(zipPath);
        QVERIFY(zip.open(QIODevice::ReadOnly));
        Releases::Release release;
        release.version = v("0.2.0");
        Releases::Asset asset;
        asset.name = Releases::windowsAssetName(release.version);
        asset.url = QUrl::fromLocalFile(zipPath);
        asset.sha256 = QCryptographicHash::hash(zip.readAll(), QCryptographicHash::Sha256).toHex();
        zip.close();
        release.assets.append(asset);

        UpdateInstaller installer;
        installer.setApplicationDir(appDir);
        QSignalSpy ready(&installer, &UpdateInstaller::ready);
        QSignalSpy failed(&installer, &UpdateInstaller::failed);
        installer.prepare(release);
        QVERIFY2(ready.wait(30000), failed.isEmpty() ? "timeout" : qPrintable(failed.first().first().toString()));
        QVERIFY(QFile::exists(installer.newAppDir() + QStringLiteral("/version.txt")));

        // The script waits for a process that has already ended, then swaps.
        QProcess done;
        done.start(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("exit")});
        QVERIFY(done.waitForFinished(10000));
        const QString script = UpdateInstaller::writeSwapScript(dir.path());
        QVERIFY(!script.isEmpty());
        QProcess swap;
        swap.start(qEnvironmentVariable("SystemRoot") + QStringLiteral("\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"),
                   {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
                    QStringLiteral("Bypass"), QStringLiteral("-File"), QDir::toNativeSeparators(script),
                    QStringLiteral("-ProcessId"), QString::number(done.processId()),
                    QStringLiteral("-AppDir"), QDir::toNativeSeparators(appDir),
                    QStringLiteral("-NewDir"), QDir::toNativeSeparators(installer.newAppDir()),
                    QStringLiteral("-Stage"), QDir::toNativeSeparators(appDir + QStringLiteral(".update"))});
        QVERIFY(swap.waitForFinished(60000));
        QVERIFY2(swap.exitCode() == 0, swap.readAllStandardError().constData());

        QFile result(appDir + QStringLiteral("/version.txt"));
        QVERIFY(result.open(QIODevice::ReadOnly));
        QCOMPARE(result.readAll(), QByteArray("new"));
        QVERIFY(!QFileInfo::exists(appDir + QStringLiteral(".old")));
        QVERIFY(!QFileInfo::exists(appDir + QStringLiteral(".update")));
    }
#endif
};

QTEST_MAIN(TestUpdate)
#include "tst_update.moc"
