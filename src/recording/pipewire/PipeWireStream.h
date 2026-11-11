#pragma once

#include <QImage>
#include <QRect>
#include <QString>

#include <cstdint>
#include <memory>

// Reads a video stream from PipeWire (F-50, D-039): on GNOME the stream of
// a Mutter screen cast, later the ScreenCast portal's too. The app asks
// only for frames in shared memory (no GPU buffers), in one of four 32-bit
// formats, so a copy into a QImage is all it takes.
//
// PipeWire calls back on its own thread (pw_thread_loop); each new frame
// is copied under a mutex, and takeLatest() hands the newest one to the
// app's thread. A frame with a crop region (Mutter's window streams are as
// large as the monitor, the window in a corner) is cut to that region.
class PipeWireStream
{
public:
    PipeWireStream();
    ~PipeWireStream();

    // Connects to the node `nodeId` of the user's PipeWire daemon. `fd`:
    // a connection the ScreenCast portal hands out, or -1 for the default
    // socket. False if PipeWire is not reachable.
    bool start(uint32_t nodeId, int fd = -1);
    void stop();

    // The newest frame since the last call; false if none came since.
    bool takeLatest(QImage &out);
    // The stream failed or was closed by the other side.
    bool hasEnded() const;
    QString lastError() const;

    // A frame as a QImage: the bytes at `data` in the SPA video format
    // `spaFormat` (BGRx, BGRA, RGBx or RGBA), cut to `crop` if it is valid.
    // Null for other formats. Public for the tests.
    static QImage toImage(const uchar *data, int width, int height, int stride, uint32_t spaFormat,
                          const QRect &crop = QRect());

    struct Impl;

private:
    std::unique_ptr<Impl> d;
};
