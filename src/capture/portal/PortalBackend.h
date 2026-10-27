#pragma once

#include "capture/CaptureBackend.h"
#include "capture/CaptureOverlay.h"

#include <QVariantMap>

class Countdown;

// Capture through the desktop portal (xdg-desktop-portal) where nothing
// better is there: Wayland desktops other than GNOME that do not offer
// wlr-screencopy, e.g. KDE Plasma (D-032). The portal's Screenshot call
// without its own dialog (interactive = false) is the freeze (D-011); the
// user then selects a region or a monitor in the app's own overlay. The
// portal has no window list, so there is no window capture.
class PortalBackend : public CaptureBackend
{
    Q_OBJECT

public:
    explicit PortalBackend(QObject *parent = nullptr);
    ~PortalBackend() override;

    bool isAvailable() const override;
    QString unavailableReason() const override;
    void captureWindow() override;
    void captureRegion() override;
    void captureScreen() override;

    // Public for the tests.
    // The object path the portal uses for a request: the caller's unique
    // bus name without ":" and with "." as "_", and the handle token.
    static QString requestPath(const QString &uniqueName, const QString &token);
    // The portal returns one picture of the whole desktop. Where each
    // monitor (logical rectangles) is in it, in picture pixels.
    static QList<QRect> splitDesktop(const QSize &picture, const QList<QRect> &logical);

private slots:
    // org.freedesktop.portal.Request.Response: 0 = done, 1 = cancelled by
    // the user, 2 = failed; `results` has the picture's "uri".
    void onResponse(uint response, const QVariantMap &results);

private:
    void start(CaptureOverlay::Mode mode);
    void requestScreenshot();
    void listenTo(const QString &path);
    void stopListening();
    void deliverPart(const QRect &native, CaptureResult::Kind kind);
    void finish();

    CaptureOverlay::Mode m_mode = CaptureOverlay::Mode::Region;
    QString m_requestPath;
    QList<CaptureOverlay::Screen> m_screens;
    CaptureOverlay *m_overlay = nullptr;
    Countdown *m_countdown = nullptr;
    bool m_waiting = false;
};
