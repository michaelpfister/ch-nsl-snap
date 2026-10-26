#pragma once

#include "capture/CaptureBackend.h"

#include <QVariantMap>

class QDBusMessage;

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
    void setHotkeyOptions(const HotkeyOptions &options) override;
    QString hotkeyProblem() const override;
    void takeHotkeyCapture() override;

    // The options of Configure for `options` (F-42); public for the tests.
    static QVariantMap configureOptions(const HotkeyOptions &options);

    // Turns the extension's reply (PNG bytes and info dictionary) into a
    // result; public for the tests.
    static CaptureResult resultFromReply(const QByteArray &png, const QVariantMap &info);

private:
    // `mode` is "window", "region" or "screen".
    void capture(const QString &mode);
    // API version of the running extension, 0 if unknown.
    uint extensionVersion() const;

    // Emits captured() or failed() for a reply of Capture or TakeCapture.
    void handleCaptureReply(const QDBusMessage &message, bool fromHotkey);

    bool m_busy = false;
};
