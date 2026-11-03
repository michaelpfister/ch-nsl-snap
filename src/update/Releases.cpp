#include "update/Releases.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include <algorithm>

namespace Releases {

QString repository()
{
    return QStringLiteral("michaelpfister/ch-nsl-snap");
}

QUrl apiUrl()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=50").arg(repository()));
}

const Asset *Release::asset(const QString &name) const
{
    for (const Asset &asset : assets) {
        if (asset.name == name)
            return &asset;
    }
    return nullptr;
}

std::optional<QList<Release>> parse(const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray())
        return std::nullopt;

    QList<Release> releases;
    const QJsonArray items = document.array();
    for (const QJsonValue &item : items) {
        const QJsonObject object = item.toObject();
        if (object.value(QLatin1String("draft")).toBool())
            continue;
        const QString tag = object.value(QLatin1String("tag_name")).toString();
        const std::optional<Version> version = Version::parse(tag);
        if (!version)
            continue;

        Release release;
        release.version = *version;
        release.tag = tag;
        release.title = object.value(QLatin1String("name")).toString();
        if (release.title.isEmpty())
            release.title = QStringLiteral("Snapslash %1").arg(version->toString());
        release.notes = object.value(QLatin1String("body")).toString();
        release.page = QUrl(object.value(QLatin1String("html_url")).toString());
        release.published = QDateTime::fromString(object.value(QLatin1String("published_at")).toString(), Qt::ISODate);
        // GitHub's flag counts; a "-beta" tag marked as a full release is
        // still offered only as a pre-release.
        release.prerelease = object.value(QLatin1String("prerelease")).toBool() || version->isPrerelease();

        const QJsonArray assets = object.value(QLatin1String("assets")).toArray();
        for (const QJsonValue &assetValue : assets) {
            const QJsonObject assetObject = assetValue.toObject();
            Asset asset;
            asset.name = assetObject.value(QLatin1String("name")).toString();
            asset.url = QUrl(assetObject.value(QLatin1String("browser_download_url")).toString());
            asset.size = assetObject.value(QLatin1String("size")).toInteger();
            // "sha256:<hex>" since GitHub reports digests (2025).
            const QString digest = assetObject.value(QLatin1String("digest")).toString();
            if (digest.startsWith(QLatin1String("sha256:")))
                asset.sha256 = digest.mid(7).toLower().toLatin1();
            if (!asset.name.isEmpty() && asset.url.isValid())
                release.assets.append(asset);
        }
        releases.append(release);
    }

    std::sort(releases.begin(), releases.end(),
              [](const Release &a, const Release &b) { return a.version > b.version; });
    return releases;
}

QList<Release> offered(const QList<Release> &releases, bool includePrereleases, const Version &installed)
{
    QList<Release> result;
    for (const Release &release : releases) {
        if (!release.prerelease || includePrereleases || release.version == installed)
            result.append(release);
    }
    return result;
}

std::optional<Release> newerThan(const QList<Release> &releases, const Version &installed, bool includePrereleases)
{
    // `releases` is sorted newest first, so the first match is the newest.
    for (const Release &release : offered(releases, includePrereleases, installed)) {
        if (release.version > installed)
            return release;
    }
    return std::nullopt;
}

QHash<QString, QByteArray> parseChecksums(const QByteArray &text)
{
    QHash<QString, QByteArray> sums;
    const QList<QByteArray> lines = text.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        // "<64 hex digits><space><space or *><name>"
        const qsizetype space = line.indexOf(' ');
        if (space != 64)
            continue;
        QByteArray name = line.mid(space + 1).trimmed();
        if (name.startsWith('*')) // binary mode marker
            name = name.mid(1);
        if (!name.isEmpty())
            sums.insert(QString::fromUtf8(name), line.left(64).toLower());
    }
    return sums;
}

QString checksumFileName()
{
    return QStringLiteral("SHA256SUMS.txt");
}

QString portableMarkerName()
{
    return QStringLiteral("snapslash-portable.txt");
}

Installation installation(const QString &applicationDir, bool isWindows, bool hasPortableMarker)
{
    if (isWindows && hasPortableMarker)
        return Installation::PortableWindows;
    if (applicationDir.startsWith(QLatin1String("/nix/store/")))
        return Installation::Nix;
    return Installation::Other;
}

Installation installation()
{
    const QString dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const bool isWindows = true;
#else
    const bool isWindows = false;
#endif
    return installation(dir, isWindows, QFileInfo::exists(dir + QLatin1Char('/') + portableMarkerName()));
}

QString windowsAssetName(const Version &version, const QString &cpuArchitecture)
{
    // Qt says "x86_64" and "arm64"; the release files say "x64" and "arm64".
    const QString arch = cpuArchitecture == QLatin1String("x86_64") ? QStringLiteral("x64") : cpuArchitecture;
    return QStringLiteral("snapslash-%1-windows-%2.zip").arg(version.toString(), arch);
}

QString windowsAssetName(const Version &version)
{
    // The architecture this program was built for, not the machine's: an
    // x64 build running emulated on ARM is replaced by the x64 build.
    return windowsAssetName(version, QSysInfo::buildCpuArchitecture());
}

} // namespace Releases
