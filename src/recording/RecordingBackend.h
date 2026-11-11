#pragma once

#include <QImage>
#include <QObject>
#include <QScreen>
#include <QSize>
#include <QString>

#include <memory>

// Interface for the per-platform video code (F-50, D-039,
// docs/01-architecture.md). A recording has two steps:
//
// 1. pick(): the backend freezes the screen and lets the user pick a window
//    or a monitor in the capture overlay (D-011, D-003). It answers with
//    exactly one of picked(), cancelled() or failed().
// 2. After the countdown, startStream() and then requestFrame() for every
//    frame the video needs; each request is answered with frameReady(),
//    unless a request is still on its way. stopStream() ends it.
//
// Everything else (countdown, timing, encoding, the indicator, saving) is
// shared and lives in RecordingController.
class RecordingBackend : public QObject
{
    Q_OBJECT

public:
    enum class Source { Window, Screen };

    using QObject::QObject;

    // False where this platform has no video backend yet, or the desktop
    // does not allow it; unavailableReason() says why.
    virtual bool isAvailable() const = 0;
    virtual QString unavailableReason() const = 0;
    // Whether single windows can be recorded (on Sway & co. only screens).
    virtual bool supportsWindow() const = 0;

    virtual void pick(Source source) = 0;
    virtual void startStream() = 0;
    virtual void requestFrame() = 0;
    virtual void stopStream() = 0;

    // Where the recording indicator should go, so it stays out of the
    // video if it can (Q-31): a monitor that is not recorded, or else the
    // one that is. nullptr: the primary monitor.
    virtual QScreen *indicatorScreen() const { return nullptr; }

signals:
    // The user picked; `size` is the picked window or monitor in physical
    // pixels, `title` names it (window title, "Bildschirm").
    void picked(RecordingBackend::Source source, const QString &title, const QSize &size);
    void cancelled();
    void failed(const QString &message);

    // A frame of the stream, in physical pixels (any size: a window may be
    // resized while it is recorded).
    void frameReady(const QImage &frame);
    // The stream ended by itself, e.g. the window was closed.
    void streamEnded(const QString &reason);
};

// Returns the video backend for the platform the app runs on.
std::unique_ptr<RecordingBackend> createRecordingBackend(QObject *parent = nullptr);
