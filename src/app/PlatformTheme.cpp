#include "app/PlatformTheme.h"

#include <QDir>
#include <QFile>

namespace PlatformTheme {

namespace {

// The schema GTK 3's file chooser reads; GLib aborts without it.
const QByteArray kFileChooserSchema = QByteArrayLiteral("org.gtk.Settings.FileChooser");

// Desktops on which Qt picks the GTK theme by itself (Qt's
// QGenericUnixTheme::themeNames()).
const QStringList kGtkDesktops{QStringLiteral("GNOME"), QStringLiteral("X-CINNAMON"), QStringLiteral("PANTHEON"),
                               QStringLiteral("UNITY"), QStringLiteral("MATE"), QStringLiteral("XFCE"),
                               QStringLiteral("LXDE")};

bool wouldLoadGtk(const QString &platformTheme, const QString &desktop)
{
    // The variable may hold a list, e.g. "gtk3:kde"; Qt takes the first it can load.
    const QString first = platformTheme.section(QLatin1Char(':'), 0, 0).trimmed();
    if (!first.isEmpty())
        return first.compare(QLatin1String("gtk3"), Qt::CaseInsensitive) == 0;
    for (const QString &name : desktop.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        if (kGtkDesktops.contains(name, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool hasFileChooserSchema(const QString &dir)
{
    // The compiled schemas are one database file per folder; schema ids
    // are stored as plain strings in it.
    QFile file(QDir(dir).filePath(QStringLiteral("gschemas.compiled")));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return file.readAll().contains(kFileChooserSchema);
}

} // namespace

QStringList schemaDirs()
{
    QStringList dirs;
    const QString explicitDirs = qEnvironmentVariable("GSETTINGS_SCHEMA_DIR");
    if (!explicitDirs.isEmpty())
        dirs << explicitDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts);

    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty())
        dataHome = QDir::homePath() + QStringLiteral("/.local/share");
    QString dataDirs = qEnvironmentVariable("XDG_DATA_DIRS");
    if (dataDirs.isEmpty())
        dataDirs = QStringLiteral("/usr/local/share:/usr/share"); // GLib's default
    for (const QString &dir : QStringList{dataHome} + dataDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts))
        dirs << dir + QStringLiteral("/glib-2.0/schemas");
    return dirs;
}

bool gtkWithoutSchemas(const QString &platformTheme, const QString &desktop, const QStringList &schemaDirs)
{
    if (!wouldLoadGtk(platformTheme, desktop))
        return false;
    for (const QString &dir : schemaDirs) {
        if (hasFileChooserSchema(dir))
            return false;
    }
    return true;
}

void avoidBrokenGtk()
{
#if defined(Q_OS_LINUX)
    if (!gtkWithoutSchemas(qEnvironmentVariable("QT_QPA_PLATFORMTHEME"), qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                           schemaDirs()))
        return;
    qInfo("GTK's settings schemas are not installed; using the desktop portal's theme instead of GTK's.");
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
#endif
}

} // namespace PlatformTheme
