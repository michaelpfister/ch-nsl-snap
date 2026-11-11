#include "update/Version.h"

#include <QRegularExpression>

#include <algorithm>

std::optional<Version> Version::parse(const QString &text)
{
    // Three numbers without leading zeros, then optionally "-" and dot-separated
    // identifiers of letters, digits and hyphens. Build metadata ("+...") is
    // not used by our tags and not accepted.
    static const QRegularExpression pattern(QStringLiteral(
        R"(^v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$)"));
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch())
        return std::nullopt;

    Version version;
    bool ok = true;
    version.major = match.captured(1).toInt(&ok);
    if (ok)
        version.minor = match.captured(2).toInt(&ok);
    if (ok)
        version.patch = match.captured(3).toInt(&ok);
    if (!ok) // a number too large for an int
        return std::nullopt;
    if (match.hasCaptured(4))
        version.prerelease = match.captured(4).split(QLatin1Char('.'));
    return version;
}

Version Version::current()
{
    // CMake writes a valid version; a broken one would show as 0.0.0.
    return parse(QStringLiteral(NSL_VERSION)).value_or(Version{});
}

QString Version::toString() const
{
    QString text = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
    if (isPrerelease())
        text += QLatin1Char('-') + prerelease.join(QLatin1Char('.'));
    return text;
}

int Version::compare(const Version &a, const Version &b)
{
    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    // Same numbers: the release is newer than any of its pre-releases.
    if (a.isPrerelease() != b.isPrerelease())
        return a.isPrerelease() ? -1 : 1;

    // Both pre-releases: part by part. Numeric parts compare as numbers and
    // sort before text parts; text compares in ASCII order.
    for (qsizetype i = 0; i < std::min(a.prerelease.size(), b.prerelease.size()); ++i) {
        bool aNumeric = false;
        bool bNumeric = false;
        const qlonglong aNumber = a.prerelease[i].toLongLong(&aNumeric);
        const qlonglong bNumber = b.prerelease[i].toLongLong(&bNumeric);
        if (aNumeric && bNumeric) {
            if (aNumber != bNumber)
                return aNumber < bNumber ? -1 : 1;
        } else if (aNumeric != bNumeric) {
            return aNumeric ? -1 : 1;
        } else if (const int text = QString::compare(a.prerelease[i], b.prerelease[i], Qt::CaseSensitive); text != 0) {
            return text < 0 ? -1 : 1;
        }
    }
    // "beta" < "beta.1": fewer parts sort first.
    if (a.prerelease.size() != b.prerelease.size())
        return a.prerelease.size() < b.prerelease.size() ? -1 : 1;
    return 0;
}
