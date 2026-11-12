#pragma once

#include <QString>
#include <QStringList>

// How the AppImage sets itself up for the user who runs it (D-041): a menu
// entry with icon, the `snapslash` command in ~/.local/bin, and on GNOME the
// extension that comes inside the AppImage. Only files of this user change,
// nothing system-wide.
//
// Everything works on the folders in Paths, so the tests can use a
// temporary home folder; currentPaths() gives the real ones.
namespace LinuxIntegration {

struct Paths
{
    QString appImage; // the AppImage file ($APPIMAGE)
    QString shareDir; // the AppImage's usr/share (next to usr/bin)
    QString dataHome; // ~/.local/share ($XDG_DATA_HOME)
    QString binDir;   // ~/.local/bin
};

// True when this program runs from an AppImage: the AppImage runtime sets
// $APPIMAGE to the file's path.
bool isAppImage();
Paths currentPaths();
// GNOME Shell is the desktop ($XDG_CURRENT_DESKTOP).
bool isGnome();

// --- Menu entry, icon, command -------------------------------------------

// The app's id, the name of its desktop entry and icon.
QString appId(); // "ch.netslash.Snapslash"

// The bundled desktop entry with every "Exec=snapslash" pointing at
// `appImage` instead, and a TryExec line, so the entry disappears from the
// menu when the file is gone.
QString desktopEntry(const QString &bundled, const QString &appImage);
// `path` as the program of an Exec line: quoted when needed, and escaped as
// the desktop entry specification asks.
QString execProgram(const QString &path);

struct SetupResult
{
    bool changed = false; // something was written (first start, or the file moved)
    QString error;        // empty when everything is set up
};
// Writes the desktop entry and the icon to dataHome and links
// binDir/snapslash to the AppImage. Files that are already right stay
// untouched. A `snapslash` in binDir that is not a link (a build installed
// from source) is left alone.
SetupResult setUpDesktop(const Paths &paths);

// --- GNOME extension -------------------------------------------------------

QString extensionUuid(); // "nsl-snap@netslash.ch"
QString bundledExtension(const Paths &paths);
QString installedExtension(const Paths &paths);
bool hasBundledExtension(const Paths &paths);

enum class ExtensionState {
    Missing,   // not installed for this user
    Current,   // the same files as the bundled one
    Different, // another version (an older app, or a link to a checkout)
};
ExtensionState extensionState(const Paths &paths);

// Copies the bundled extension over the installed one (a link to a
// checkout is replaced, its target kept). Returns an error text, or an
// empty string on success.
QString installExtension(const Paths &paths);

// GSettings writes a list of strings as "['a', 'b']" (or "@as []").
QStringList parseStringList(const QString &text);
QString formatStringList(const QStringList &list);

// Turns the extension on in org.gnome.shell (enabled-extensions, and off the
// disabled list; user extensions on). GNOME Shell loads it at the next
// login. Uses the system's `gsettings`. Returns an error text or "".
QString enableExtension();

} // namespace LinuxIntegration
