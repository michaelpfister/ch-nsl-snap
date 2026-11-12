#pragma once

#include "app/GlobalHotkeys.h"
#include "capture/CountdownOptions.h"
#include "editor/ArrowStyle.h"
#include "editor/FrameStyle.h"
#include "editor/MarkerStyle.h"
#include "editor/RedactStyle.h"
#include "editor/TextStyle.h"

#include <QColor>
#include <QDateTime>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

// User settings, stored with QSettings (~/.config/netslash.ch/Snapslash.conf
// on Linux, the registry on Windows, a plist on macOS).
namespace Settings {

// Copies the settings stored under the former app name "NSL Snap" (D-036),
// once: only when there are none under the current name yet.
void migrateFromFormerName();

// Shows extra controls, e.g. the dim strength and blur sliders (F-27).
bool advancedMode();
void setAdvancedMode(bool enabled);

// Region capture (F-03, D-023): false = two clicks (default), true = drag.
// The timer of the top bar (F-04): seconds, 0 = off. Remembered.
int captureDelay();
void setCaptureDelay(int seconds);

bool regionByDragging();
void setRegionByDragging(bool enabled);

// The countdown of the timer (F-04): where it appears, and whether a click
// on it cancels (else clicks go through it and only Escape cancels).
CountdownOptions countdownOptions();
void setCountdownPosition(CountdownPosition position);
void setCountdownClickCancels(bool enabled);

// Saving (F-41): the folder for Ctrl+S and the save dialog, and the
// pattern for file names (FileNames).
QString saveFolder();
void setSaveFolder(const QString &folder);
QString fileNamePattern();
void setFileNamePattern(const QString &pattern);

// Series of captures (F-52): every capture goes straight into the save
// folder, without opening in the editor, so the next one can follow.
bool autoSaveCaptures();
void setAutoSaveCaptures(bool enabled);

// The list of recent captures at the bottom of the window (F-52): the
// saved files, newest first, at most kMaxRecentCaptures; and whether the
// list is pinned open.
constexpr int kMaxRecentCaptures = 30;
QStringList recentCaptures();
void setRecentCaptures(const QStringList &paths);
// Puts `path` first; it moves up if it is already in the list.
void addRecentCapture(const QString &path);
bool recentCapturesPinned();
void setRecentCapturesPinned(bool pinned);

// Tray icon (F-45): closing the window keeps the app running in the tray.
bool closeToTray();
void setCloseToTray(bool enabled);
// Ask before quitting while the image is neither saved nor copied; off
// after "Beenden und in Zukunft nicht mehr nachfragen".
bool confirmQuitUnsaved();
void setConfirmQuitUnsaved(bool enabled);
// Whether the "still running in the tray" message was shown once.
bool trayHintShown();
void setTrayHintShown(bool shown);

// Numbered frames (F-24): whether a new image goes on with the numbers
// where the previous one stopped, instead of starting at 1 (default off).
bool continueNumbering();
void setContinueNumbering(bool enabled);

// Language of the user interface (D-038): "de", "en", or "" to follow the
// system. Applies at the next start.
QString language();
void setLanguage(const QString &language);

// Updates (F-49, D-037): ask GitHub at start, at most once a day; offer
// pre-releases too. The time of the last check, and the newest version
// found then that is newer than the installed one ("" if none).
bool checkForUpdates();
void setCheckForUpdates(bool enabled);
bool offerPrereleases();
void setOfferPrereleases(bool enabled);
QDateTime lastUpdateCheck();
void setLastUpdateCheck(const QDateTime &time);
QString availableUpdate();
void setAvailableUpdate(const QString &version);
// The version whose note at the bottom of the window was closed (UpdateBar).
QString dismissedUpdate();
void setDismissedUpdate(const QString &version);
// The version whose bar already drew attention (UpdateBar::announce).
QString announcedUpdate();
void setAnnouncedUpdate(const QString &version);

// Global hotkeys (F-42): the key of each action; GlobalHotkeys::defaultKey
// until changed, an empty sequence when switched off.
QKeySequence globalHotkey(GlobalHotkeys::Action action);
void setGlobalHotkey(GlobalHotkeys::Action action, const QKeySequence &key);
// Whether the note about Windows' Print key setting was shown once (D-035).
bool printKeyHintShown();
void setPrintKeyHintShown(bool shown);

// The app version for which the user answered "Später" when asked to set up
// the GNOME extension of the AppImage (D-041); asked again after an update.
QString gnomeExtensionDeclinedFor();
void setGnomeExtensionDeclinedFor(const QString &version);

// Styles for new objects (F-22): the last ones chosen in the panel, so a
// restart keeps the colours, widths and sizes the user works with.
FrameStyle frameStyle();
void setFrameStyle(const FrameStyle &style);
TextStyle textStyle(); // without text
void setTextStyle(const TextStyle &style);
ArrowStyle arrowStyle();
void setArrowStyle(const ArrowStyle &style);
MarkerStyle markerStyle();
void setMarkerStyle(const MarkerStyle &style);
RedactStyle redactStyle();
void setRedactStyle(const RedactStyle &style);
// The text for new replaced texts (F-29), e.g. "Firstname Lastname".
QString replaceText();
void setReplaceText(const QString &text);

// Forgets the styles for new objects and the recent own colours: back to
// the defaults (the other settings stay).
void resetStyles();

// Own colours used last (F-22), newest first, at most kMaxRecentColors.
constexpr int kMaxRecentColors = 8;
QList<QColor> recentColors();
// Puts `color` first; it moves up if it is already in the list.
void addRecentColor(const QColor &color);

} // namespace Settings
