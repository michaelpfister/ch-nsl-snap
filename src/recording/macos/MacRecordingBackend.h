#pragma once

#include "capture/macos/MacBackend.h"
#include "recording/RecordingBackend.h"

#include <memory>
#include <optional>
#include <vector>

namespace MacCapture { class Stream; }

// Videos on macOS (F-50, D-049): the pick is the image backend's
// (MacBackend, D-047) in video mode; the frames come from a ScreenCaptureKit
// stream with the pointer, one per window or monitor. Several monitors are
// put side by side. The app's own windows, the recording indicator among
// them, are left out of monitor recordings.
class MacRecordingBackend : public RecordingBackend
{
    Q_OBJECT

public:
    explicit MacRecordingBackend(QObject *parent = nullptr);
    ~MacRecordingBackend() override;

    bool isAvailable() const override { return true; }
    QString unavailableReason() const override { return {}; }
    bool supportsWindow() const override { return true; }

    void pick(Source source) override;
    void startStream() override;
    void requestFrame() override;
    void stopStream() override;

private:
    void onPicked(const MacBackend::VideoPick &pick);
    void end(const QString &reason);

    MacBackend *m_picker = nullptr;
    MacBackend::VideoPick m_pick;
    std::unique_ptr<MacCapture::Stream> m_window;
    std::vector<std::unique_ptr<MacCapture::Stream>> m_monitors;
    QList<QImage> m_monitorImages; // the newest picture of each monitor
    bool m_ended = false;
};
