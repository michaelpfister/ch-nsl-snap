#pragma once

#include "update/Version.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

// The releases on GitHub and which of them fits this installation (D-037).
// Everything here works on data only; UpdateChecker fetches it.
namespace Releases {

// The public repository whose releases the app offers.
QString repository(); // "owner/name"
// GitHub's list of releases (newest first, one page is plenty).
QUrl apiUrl();

struct Asset
{
    QString name;
    QUrl url; // browser_download_url
    qint64 size = 0;
    // SHA-256 as lower-case hex, from GitHub's "digest" field; empty if
    // GitHub did not report one (then SHA256SUMS.txt is the source).
    QByteArray sha256;
};

struct Release
{
    Version version;
    QString tag;   // "v0.2.0"
    QString title; // "Snapslash 0.2.0"
    QString notes; // Markdown, as written on GitHub
    QUrl page;     // the release page on github.com
    QDateTime published;
    bool prerelease = false;
    QList<Asset> assets;

    const Asset *asset(const QString &name) const;
};

// Parses GitHub's JSON answer. Drafts and tags that are no version are
// left out; the result is sorted newest first. Returns nothing if the
// answer is not a list of releases at all.
std::optional<QList<Release>> parse(const QByteArray &json);

// The releases a user may choose: without pre-releases unless asked for.
// A pre-release that is installed right now stays in the list.
QList<Release> offered(const QList<Release> &releases, bool includePrereleases, const Version &installed);

// The newest offered release that is newer than `installed`, if any.
std::optional<Release> newerThan(const QList<Release> &releases, const Version &installed, bool includePrereleases);

// "SHA256SUMS.txt" as written by sha256sum: "<hex>  <name>" per line.
QHash<QString, QByteArray> parseChecksums(const QByteArray &text);
QString checksumFileName();

// How this copy was installed decides what an update can do.
enum class Installation {
    PortableWindows, // the ZIP from a release: the app replaces its folder
    Nix,             // in the Nix store: Nix updates it, the app explains how
    Other,           // built from source or packaged elsewhere: a hint only
};
Installation installation();
// The same decision for given facts, for the tests.
Installation installation(const QString &applicationDir, bool isWindows, bool hasPortableMarker);
// The file the release ZIP puts next to snapslash.exe.
QString portableMarkerName();

// "snapslash-0.2.0-windows-x64.zip" for this build's CPU (x64 or arm64).
QString windowsAssetName(const Version &version);
QString windowsAssetName(const Version &version, const QString &cpuArchitecture);

} // namespace Releases
