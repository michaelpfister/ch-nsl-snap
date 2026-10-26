#pragma once

#include "capture/CaptureBackend.h"

#include <QVariantMap>

// Talks to the NSL Snap GNOME Shell extension over D-Bus
// (gnome-extension/nsl-snap@netslash.ch, docs/02-decisions.md D-003).
class GnomeShellBackend : public CaptureBackend
{
    Q_OBJECT

public:
    explicit GnomeShellBackend(QObject *parent = nullptr);

    bool isAvailable() const override;
    QString unavailableReason() const override;
    void captureWindow() override;
    void captureRegion() override;
    void captureScreen() override;

    // Turns the extension's reply (PNG bytes and info dictionary) into a
    // result; public for the tests.
    static CaptureResult resultFromReply(const QByteArray &png, const QVariantMap &info);

private:
    // `mode` is "window", "region" or "screen".
    void capture(const QString &mode);
    // API version of the running extension, 0 if unknown.
    uint extensionVersion() const;

    bool m_busy = false;
};
