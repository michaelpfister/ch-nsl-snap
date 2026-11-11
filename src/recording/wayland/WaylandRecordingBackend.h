#pragma once

#include "capture/wayland/WaylandBackend.h"
#include "recording/RecordingBackend.h"

class WaylandStream;

// Videos on Hyprland, Sway and other wlroots desktops (F-50, D-039, step
// 1): the pick is the image backend's (WaylandBackend, D-031) in video
// mode, the frames come from WaylandStream. Single windows only on
// Hyprland.
class WaylandRecordingBackend : public RecordingBackend
{
    Q_OBJECT

public:
    explicit WaylandRecordingBackend(QObject *parent = nullptr);
    ~WaylandRecordingBackend() override;

    bool isAvailable() const override;
    QString unavailableReason() const override;
    bool supportsWindow() const override;

    void pick(Source source) override;
    void startStream() override;
    void requestFrame() override;
    void stopStream() override;
    QScreen *indicatorScreen() const override;

private:
    void onPicked(const WaylandBackend::VideoPick &pick);

    WaylandBackend *m_picker = nullptr;
    WaylandStream *m_stream = nullptr;
    WaylandBackend::VideoPick m_pick;
};
