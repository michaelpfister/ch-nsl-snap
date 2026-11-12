#include "app/LinuxIntegration.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>

#include <optional>


namespace LinuxIntegration {

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Writes `data` unless the file already holds it. QSaveFile writes to a
// temporary file and renames it, so a crash never leaves half a file.
// Returns false on an error; sets `changed` when it wrote.
bool writeIfDifferent(const QString &path, const QByteArray &data, bool &changed)
{
    if (QFileInfo::exists(path) && readAll(path) == data)
        return true;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(data);
    if (!file.commit())
        return false;
    changed = true;
    return true;
}

// The files below `dir`, as paths relative to it, sorted.
QStringList filesBelow(const QString &dir)
{
    QStringList files;
    QDirIterator it(dir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext())
        files.append(QDir(dir).relativeFilePath(it.next()));
    files.sort();
    return files;
}

bool copyTree(const QString &from, const QString &to)
{
    for (const QString &file : filesBelow(from)) {
        const QString target = to + QLatin1Char('/') + file;
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(from + QLatin1Char('/') + file, target))
            return false;
    }
    return true;
}

// Removes a file, a link (never what it points to) or a folder.
bool removePath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isSymLink() || info.isFile())
        return QFile::remove(path);
    if (info.isDir())
        return QDir(path).removeRecursively();
    return true; // nothing there
}

// Runs the system's gsettings; returns its output, or nothing on failure.
std::optional<QString> gsettings(const QStringList &arguments, QString &error)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), arguments);
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        error = QCoreApplication::translate("LinuxIntegration", "«gsettings %1» ist fehlgeschlagen: %2")
                    .arg(arguments.join(QLatin1Char(' ')),
                         QString::fromLocal8Bit(process.readAllStandardError()).trimmed()
                             + (process.error() == QProcess::FailedToStart ? process.errorString() : QString()));
        return std::nullopt;
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

} // namespace

bool isAppImage()
{
    const QString path = qEnvironmentVariable("APPIMAGE");
    return !path.isEmpty() && QFileInfo(path).isFile();
}

Paths currentPaths()
{
    Paths paths;
    paths.appImage = qEnvironmentVariable("APPIMAGE");
    // The program is usr/bin/snapslash inside the AppImage.
    paths.shareDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share"));
    // The XDG base directories, as the desktop reads them.
    paths.dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (paths.dataHome.isEmpty() || QDir::isRelativePath(paths.dataHome))
        paths.dataHome = QDir::homePath() + QStringLiteral("/.local/share");
    paths.binDir = QDir::homePath() + QStringLiteral("/.local/bin");
    return paths;
}

bool isGnome()
{
    // "GNOME", "ubuntu:GNOME", "Zorin:GNOME", ...
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':')).contains(QLatin1String("GNOME"));
}

QString appId()
{
    return QStringLiteral("ch.netslash.Snapslash");
}

QString execProgram(const QString &path)
{
    // Desktop entry specification, "The Exec key": arguments with reserved
    // characters go in double quotes, inside which ", `, $ and \ get a
    // backslash. "%" is a field code and is doubled.
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    bool needsQuotes = false;
    for (const QChar c : path)
        needsQuotes = needsQuotes || reserved.contains(c);
    QString program = path;
    program.replace(QLatin1Char('%'), QStringLiteral("%%"));
    if (needsQuotes) {
        QString quoted;
        for (const QChar c : std::as_const(program)) {
            if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$') || c == QLatin1Char('\\'))
                quoted += QLatin1Char('\\');
            quoted += c;
        }
        program = QLatin1Char('"') + quoted + QLatin1Char('"');
    }
    // Then the general rule for string values: a backslash is written twice.
    program.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    return program;
}

QString desktopEntry(const QString &bundled, const QString &appImage)
{
    const QString program = execProgram(appImage);
    QString tryExec = appImage;
    tryExec.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    QStringList lines;
    bool mainGroup = false;
    for (const QString &line : bundled.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1Char('[')))
            mainGroup = line == QLatin1String("[Desktop Entry]");
        if (line == QLatin1String("Exec=snapslash") || line.startsWith(QLatin1String("Exec=snapslash "))) {
            lines.append(QStringLiteral("Exec=") + program + line.mid(QStringLiteral("Exec=snapslash").size()));
            // Only the main group knows TryExec.
            if (mainGroup)
                lines.append(QStringLiteral("TryExec=") + tryExec);
            continue;
        }
        if (line.startsWith(QLatin1String("TryExec=")))
            continue;
        lines.append(line);
    }
    return lines.join(QLatin1Char('\n'));
}

SetupResult setUpDesktop(const Paths &paths)
{
    SetupResult result;
    const QString entryName = appId() + QStringLiteral(".desktop");
    const QString iconPath = QStringLiteral("/icons/hicolor/scalable/apps/") + appId() + QStringLiteral(".svg");

    const QByteArray bundled = readAll(paths.shareDir + QStringLiteral("/applications/") + entryName);
    if (bundled.isEmpty()) {
        result.error = QCoreApplication::translate("LinuxIntegration", "Der Menüeintrag fehlt im AppImage.");
        return result;
    }
    // The icon first: the desktop reads the entry as soon as it appears.
    const QByteArray icon = readAll(paths.shareDir + iconPath);
    if (!icon.isEmpty() && !writeIfDifferent(paths.dataHome + iconPath, icon, result.changed))
        result.error = QCoreApplication::translate("LinuxIntegration", "Das Symbol lässt sich nicht nach %1 schreiben.").arg(paths.dataHome);
    const QByteArray entry = desktopEntry(QString::fromUtf8(bundled), paths.appImage).toUtf8();
    if (!writeIfDifferent(paths.dataHome + QStringLiteral("/applications/") + entryName, entry, result.changed))
        result.error = QCoreApplication::translate("LinuxIntegration", "Der Menüeintrag lässt sich nicht nach %1 schreiben.").arg(paths.dataHome);

    // ~/.local/bin/snapslash -> the AppImage, for key bindings and scripts.
    const QString link = paths.binDir + QStringLiteral("/snapslash");
    const QFileInfo linkInfo(link);
    if (linkInfo.isSymLink()) {
        if (linkInfo.symLinkTarget() == QFileInfo(paths.appImage).absoluteFilePath())
            return result;
        QFile::remove(link); // an AppImage that was moved or replaced
    } else if (linkInfo.exists()) {
        return result; // a build installed from source: not ours to replace
    }
    if (!QDir().mkpath(paths.binDir) || !QFile::link(paths.appImage, link))
        result.error = QCoreApplication::translate("LinuxIntegration", "Der Befehl «snapslash» lässt sich nicht in %1 anlegen.").arg(paths.binDir);
    else
        result.changed = true;
    return result;
}

QString extensionUuid()
{
    return QStringLiteral("nsl-snap@netslash.ch");
}

QString bundledExtension(const Paths &paths)
{
    return paths.shareDir + QStringLiteral("/snapslash/gnome-extension/") + extensionUuid();
}

QString installedExtension(const Paths &paths)
{
    return paths.dataHome + QStringLiteral("/gnome-shell/extensions/") + extensionUuid();
}

bool hasBundledExtension(const Paths &paths)
{
    return QFileInfo::exists(bundledExtension(paths) + QStringLiteral("/metadata.json"));
}

ExtensionState extensionState(const Paths &paths)
{
    const QString installed = installedExtension(paths);
    if (!QFileInfo::exists(installed + QStringLiteral("/metadata.json")))
        return ExtensionState::Missing;
    // A link (scripts/dev-install.sh) is a developer's checkout: never the
    // AppImage's copy, even when the files happen to match.
    if (QFileInfo(installed).isSymLink())
        return ExtensionState::Different;
    const QString bundled = bundledExtension(paths);
    const QStringList files = filesBelow(bundled);
    if (files != filesBelow(installed))
        return ExtensionState::Different;
    for (const QString &file : files) {
        if (readAll(bundled + QLatin1Char('/') + file) != readAll(installed + QLatin1Char('/') + file))
            return ExtensionState::Different;
    }
    return ExtensionState::Current;
}

QString installExtension(const Paths &paths)
{
    if (!hasBundledExtension(paths))
        return QCoreApplication::translate("LinuxIntegration", "Die GNOME-Erweiterung fehlt im AppImage.");
    // Copied next to the old one first, then swapped: a failed copy leaves
    // the installed extension as it was.
    const QString target = installedExtension(paths);
    const QString fresh = target + QStringLiteral(".new");
    removePath(fresh);
    if (!copyTree(bundledExtension(paths), fresh)) {
        removePath(fresh);
        return QCoreApplication::translate("LinuxIntegration", "Die Erweiterung lässt sich nicht nach %1 kopieren.").arg(QFileInfo(target).absolutePath());
    }
    if (!removePath(target) || !QDir().rename(fresh, target)) {
        removePath(fresh);
        return QCoreApplication::translate("LinuxIntegration", "Die bisherige Erweiterung in %1 lässt sich nicht ersetzen.").arg(target);
    }
    return {};
}

QStringList parseStringList(const QString &text)
{
    // "['a@b', 'c']", or "@as []" for an empty list. Extension ids hold no
    // quotes, so the items are what stands between single quotes.
    QStringList list;
    static const QRegularExpression item(QStringLiteral("'([^']*)'"));
    for (auto it = item.globalMatch(text); it.hasNext();)
        list.append(it.next().captured(1));
    return list;
}

QString formatStringList(const QStringList &list)
{
    QStringList quoted;
    for (const QString &entry : list)
        quoted.append(QLatin1Char('\'') + entry + QLatin1Char('\''));
    return QLatin1Char('[') + quoted.join(QStringLiteral(", ")) + QLatin1Char(']');
}

QString enableExtension()
{
    const QString schema = QStringLiteral("org.gnome.shell");
    const QString uuid = extensionUuid();
    QString error;

    const auto enabled = gsettings({QStringLiteral("get"), schema, QStringLiteral("enabled-extensions")}, error);
    if (!enabled)
        return error;
    QStringList list = parseStringList(*enabled);
    if (!list.contains(uuid)) {
        list.append(uuid);
        if (!gsettings({QStringLiteral("set"), schema, QStringLiteral("enabled-extensions"), formatStringList(list)}, error))
            return error;
    }
    // Switched off once in the Extensions app: on again, the user asked for it.
    const auto disabled = gsettings({QStringLiteral("get"), schema, QStringLiteral("disabled-extensions")}, error);
    if (disabled) {
        QStringList off = parseStringList(*disabled);
        if (off.removeAll(uuid) > 0)
            gsettings({QStringLiteral("set"), schema, QStringLiteral("disabled-extensions"), formatStringList(off)}, error);
    }
    // The Extensions app's main switch: without it no user extension runs.
    const auto userOff = gsettings({QStringLiteral("get"), schema, QStringLiteral("disable-user-extensions")}, error);
    if (userOff && *userOff == QLatin1String("true"))
        gsettings({QStringLiteral("set"), schema, QStringLiteral("disable-user-extensions"), QStringLiteral("false")}, error);
    return {};
}

} // namespace LinuxIntegration
