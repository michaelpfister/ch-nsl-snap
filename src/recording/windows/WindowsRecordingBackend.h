#pragma once

#include "capture/windows/WindowsBackend.h"
#include "capture/windows/WgcCapture.h"
#include "recording/RecordingBackend.h"

#include <memory>
#include <optional>
#include <vector>

// Videos on Windows (F-50, D-039, step 3): the pick is the image backend's
// (WindowsBackend, D-022) in video mode; the frames come from
// Windows.Graphics.Capture as a continuous stream (WgcCapture::Stream),
// with the pointer. A window is cut to its visible frame like a window
// screenshot; several monitors are put side by side. The recording
// indicator is left out of every capture (WDA_EXCLUDEFROMCAPTURE).
class WindowsRecordingBackend : public RecordingBackend
{
    Q_OBJECT

public:
    explicit WindowsRecordingBackend(QObject *parent = nullptr);
    ~WindowsRecordingBackend() override;

    bool isAvailable() const override;
    QString unavailableReason() const override;
    bool supportsWindow() const override { return true; }

    void pick(Source source) override;
    void startStream() override;
    void requestFrame() override;
    void stopStream() override;

    // For the tests: pick() answers with this instead of showing the overlay.
    void setPresetPick(const WindowsBackend::VideoPick &pick) { m_preset = pick; }

private:
    void onPicked(const WindowsBackend::VideoPick &pick);
    void end(const QString &reason);

    WindowsBackend *m_picker = nullptr;
    WindowsBackend::VideoPick m_pick;
    std::optional<WindowsBackend::VideoPick> m_preset;
    std::unique_ptr<WgcCapture::Stream> m_window;
    std::vector<std::unique_ptr<WgcCapture::Stream>> m_monitors;
    QList<QImage> m_monitorImages; // the newest picture of each monitor
    WgcCapture::Pixels m_pixels;   // reused from frame to frame
    bool m_ended = false;
};
