#pragma once

#include <QString>
#include <QStringList>

#include <optional>

// A version number as the release tags carry it (D-037): "1.2.3" or, for a
// pre-release, "1.2.3-beta.1". Compared after the rules of Semantic
// Versioning (semver.org): numbers first, then a pre-release sorts before
// the release of the same number ("1.2.3-beta.1" < "1.2.3").
struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    // The parts after the "-", split at the dots: {"beta", "1"}.
    QStringList prerelease;

    // Accepts a leading "v" ("v1.2.3"). Returns nothing for anything else
    // than three numbers with an optional pre-release part.
    static std::optional<Version> parse(const QString &text);
    // The version of this build (NSL_VERSION, set by CMake).
    static Version current();

    bool isPrerelease() const { return !prerelease.isEmpty(); }
    QString toString() const;

    // < 0, 0 or > 0, like strcmp.
    static int compare(const Version &a, const Version &b);
};

inline bool operator==(const Version &a, const Version &b) { return Version::compare(a, b) == 0; }
inline bool operator!=(const Version &a, const Version &b) { return Version::compare(a, b) != 0; }
inline bool operator<(const Version &a, const Version &b) { return Version::compare(a, b) < 0; }
inline bool operator>(const Version &a, const Version &b) { return Version::compare(a, b) > 0; }
