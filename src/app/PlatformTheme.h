#pragma once

#include <QString>
#include <QStringList>

// Keeps Qt from loading its GTK platform theme where GTK would abort the
// app (D-033). Qt uses GTK for file dialogs, icons and colours when
// QT_QPA_PLATFORMTHEME is "gtk3", or by default on GTK-based desktops.
// GTK reads its settings through GSettings, and where the schemas are not
// installed (NixOS without a wrapped binary, minimal window manager
// setups) GLib aborts the whole app instead of reporting an error, at the
// latest when a file dialog opens. There the app asks Qt for the desktop
// portal's theme instead: file dialogs then come from the portal if the
// desktop has one, else from Qt itself.
namespace PlatformTheme {

// Call before the QApplication is created.
void avoidBrokenGtk();

// The pure part, for the tests: whether Qt would load the GTK theme
// (`platformTheme`: QT_QPA_PLATFORMTHEME, `desktop`: XDG_CURRENT_DESKTOP)
// and none of `schemaDirs` holds GTK's file chooser schema.
bool gtkWithoutSchemas(const QString &platformTheme, const QString &desktop, const QStringList &schemaDirs);

// The folders GLib reads compiled schemas from: GSETTINGS_SCHEMA_DIR, then
// glib-2.0/schemas in XDG_DATA_HOME and every XDG_DATA_DIRS folder.
QStringList schemaDirs();

} // namespace PlatformTheme
