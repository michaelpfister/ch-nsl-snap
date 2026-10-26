#pragma once

#include <QObject>

#include <functional>

// The app on the session bus (Linux): name ch.netslash.Snap, object
// /ch/netslash/Snap, interface ch.netslash.Snap.App1. The GNOME extension
// calls CaptureReady after a hotkey capture (F-42, D-031); D-Bus
// activation starts the app for it if it is not running. A second start
// of the app calls Activate on the first one and ends (one instance).
class AppService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "ch.netslash.Snap.App1")

public:
    explicit AppService(QObject *parent = nullptr);

    // Puts the service on the bus. False if another instance owns the name;
    // it has been asked to show its window then.
    bool registerOrActivateOther();

    // Hides the app's window before a hotkey capture; true if it was
    // shown (the extension then waits a moment before the freeze).
    std::function<bool()> hideForCapture;

public slots:
    // The D-Bus methods: scriptable slots are exported.
    Q_SCRIPTABLE void CaptureReady();
    Q_SCRIPTABLE void Activate();
    // A hotkey capture is about to start, or was cancelled (D-011: the app
    // is not in the frozen picture).
    Q_SCRIPTABLE bool HideForCapture();
    Q_SCRIPTABLE void CaptureCancelled();

signals:
    void captureReady();
    void activateRequested();
    void captureCancelled();
};
