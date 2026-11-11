#pragma once

#include <QString>
#include <QVariantMap>

// The hints along the top of the capture overlay and under the countdown,
// in the app's language (D-038). The app's own overlay draws them; the GNOME
// extension gets them with every capture, so it holds no texts of its own.
namespace CaptureHints {

QString window();
QString screens(); // with more than one monitor
QString screen();  // with one monitor
QString regionFirstCorner();
QString regionSecondCorner();
QString regionDrag();
// `clickCancels`: "Esc oder Klick", else only "Esc" (F-04).
QString countdown(bool clickCancels = true);
// Picking for a video (F-50): no region.
QString videoWindow();
QString videoScreens();
QString videoScreen();

// All of them for the extension's Capture option "hints" (a{sv} of strings), keyed as
// overlay.js and countdown.js expect.
QVariantMap forExtension(bool countdownClickCancels = true);

} // namespace CaptureHints
