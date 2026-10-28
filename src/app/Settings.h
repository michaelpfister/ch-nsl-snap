#pragma once

#include "app/GlobalHotkeys.h"
#include "editor/ArrowStyle.h"
#include "editor/FrameStyle.h"
#include "editor/MarkerStyle.h"
#include "editor/RedactStyle.h"
#include "editor/TextStyle.h"

#include <QColor>
#include <QKeySequence>
#include <QList>
#include <QString>

// User settings, stored with QSettings (~/.config/netslash.ch/NSL Snap.conf
// on Linux, the registry on Windows, a plist on macOS).
namespace Settings {

// Shows extra controls, e.g. the dim strength and blur sliders (F-27).
bool advancedMode();
void setAdvancedMode(bool enabled);

// Region capture (F-03, D-023): false = two clicks (default), true = drag.
// The timer of the top bar (F-04): seconds, 0 = off. Remembered.
int captureDelay();
void setCaptureDelay(int seconds);

bool regionByDragging();
void setRegionByDragging(bool enabled);

// Saving (F-41): the folder for Ctrl+S and the save dialog, and the
// pattern for file names (FileNames).
QString saveFolder();
void setSaveFolder(const QString &folder);
QString fileNamePattern();
void setFileNamePattern(const QString &pattern);

// Tray icon (F-45): closing the window keeps the app running in the tray.
bool closeToTray();
void setCloseToTray(bool enabled);
// Whether the "still running in the tray" message was shown once.
bool trayHintShown();
void setTrayHintShown(bool shown);

// Global hotkeys (F-42): the key of each action; GlobalHotkeys::defaultKey
// until changed, an empty sequence when switched off.
QKeySequence globalHotkey(GlobalHotkeys::Action action);
void setGlobalHotkey(GlobalHotkeys::Action action, const QKeySequence &key);
// Whether the note about Windows' Print key setting was shown once (D-031).
bool printKeyHintShown();
void setPrintKeyHintShown(bool shown);

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
