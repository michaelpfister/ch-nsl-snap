#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

#include <memory>

class QScreen;
class QWidget;

// ScreenCaptureKit and AppKit behind a plain C++ interface (D-002, D-047),
// so the backend and the shared code need no Objective-C. Implemented in
// MacCapture.mm (Objective-C++).
namespace MacCapture {

// The "Screen Recording" permission (Systemeinstellungen › Datenschutz &
// Sicherheit › Bildschirm- & Systemaudioaufnahme). Without it macOS hands
// out only the desktop picture and no window titles.
bool hasPermission();
// Shows the system's question once and adds the app to the list in the
// settings, switched off. macOS applies a change only after a restart of
// the app.
void requestPermission();

// A window that can be picked, as it was at the freeze.
struct Window
{
    QRect frame;       // in logical desktop pixels (points), as Qt's screens
    QString title;     // the window's title, may be empty
    QString appName;   // the app that owns it, e.g. "Finder"
    QString bundleId;  // e.g. "com.apple.finder"
};

// The frozen screen (D-011): every monitor, and every normal window on its
// own (without shadow, with transparent rounded corners), all taken at the
// same moment. The window pictures stay with macOS until one is asked for.
class Freeze
{
public:
    ~Freeze();

    // Freezes the given monitors and the windows on them, waiting at most
    // `timeoutMs`. Returns nullptr when a monitor could not be captured;
    // `diagnostic` then says why (English, for the log).
    static std::unique_ptr<Freeze> capture(const QList<QScreen *> &screens, int timeoutMs, QString *diagnostic);

    // One picture per screen passed to capture(), in physical pixels.
    const QList<QImage> &screens() const;
    // The windows, topmost first. The app's own windows are left out.
    const QList<Window> &windows() const;
    // The picture of window `index`, in physical pixels with alpha; null if
    // macOS delivered none.
    QImage windowImage(int index) const;

    struct Impl;

private:
    explicit Freeze(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m;
};

// Puts a frameless window above everything, also the menu bar and the
// Dock, on every Space, and makes the app active so it gets the keys
// (the capture overlay). Call after the window is shown.
void raiseAboveAll(QWidget *window);

} // namespace MacCapture
