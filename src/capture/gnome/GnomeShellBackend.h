#pragma once

#include "capture/CaptureBackend.h"

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

private:
    bool m_busy = false;
};
