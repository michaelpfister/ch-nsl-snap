#include "capture/windows/WindowsBackend.h"

#include "capture/PickGeometry.h"
#include "capture/windows/WgcCapture.h"
#include "capture/windows/WindowPicker.h"

#include <QGuiApplication>
#include <QOperatingSystemVersion>
#include <QScreen>
// The monitors' native (physical) geometry is only available from Qt's
// platform layer.
#include <qpa/qplatformscreen.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>

namespace {

QRect toQRect(const WgcCapture::Rect &r)
{
    return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

// A picture of one monitor as it is on the screen right now (without the
// pointer), for the overlay.
QImage grabMonitor(const QRect &native)
{
    QImage image(native.size(), QImage::Format_RGB32);
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = native.width();
    info.bmiHeader.biHeight = -native.height(); // top-down rows
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap && bits) {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        // CAPTUREBLT includes layered (translucent) windows.
        BitBlt(memory, 0, 0, native.width(), native.height(), screen, native.x(), native.y(), SRCCOPY | CAPTUREBLT);
        SelectObject(memory, previous);
        image = QImage(static_cast<const uchar *>(bits), native.width(), native.height(), native.width() * 4,
                       QImage::Format_RGB32).copy();
    }
    if (bitmap)
        DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return image;
}

} // namespace

double WindowsBackend::cornerRadius(const WgcCapture::Window &window)
{
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11 || window.maximized)
        return 0;
    switch (window.cornerPreference) {
    case 1: return 0; // DWMWCP_DONOTROUND
    case 3: return 4; // DWMWCP_ROUNDSMALL
    default: return 8; // DWMWCP_DEFAULT, DWMWCP_ROUND: Windows 11's 8 px
    }
}

WindowsBackend::WindowsBackend(QObject *parent)
    : CaptureBackend(parent)
{
}

WindowsBackend::~WindowsBackend() = default;

bool WindowsBackend::isAvailable() const
{
    return WgcCapture::isSupported();
}

QString WindowsBackend::unavailableReason() const
{
    return tr("Die Fensteraufnahme braucht Windows 10 ab Version 1903 oder Windows 11.");
}

void WindowsBackend::captureRegion()
{
    emit failed(tr("Die Bereichsaufnahme folgt unter Windows."));
}

void WindowsBackend::captureWindow()
{
    if (m_picker)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }

    // Freeze first (D-011): every window on its own, and the monitors as
    // the overlay's background.
    m_freeze = WgcCapture::Freeze::capture();
    QList<WindowPicker::Screen> screens;
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect native = screen->handle()->geometry();
        screens.append({screen, native, grabMonitor(native)});
    }
    QList<QRect> frames;
    for (const WgcCapture::Window &window : m_freeze->windows())
        frames << toQRect(window.frame);
    if (frames.isEmpty()) {
        m_freeze.reset();
        emit failed(tr("Es wurde kein Fenster gefunden, das aufgenommen werden kann."));
        return;
    }

    m_picker = new WindowPicker(screens, frames, this);
    connect(m_picker, &WindowPicker::picked, this, &WindowsBackend::onPicked);
    connect(m_picker, &WindowPicker::cancelled, this, [this] {
        finish();
        emit cancelled();
    });
    m_picker->show();
}

void WindowsBackend::onPicked(int index)
{
    const WgcCapture::Window window = m_freeze->windows()[std::size_t(index)];
    WgcCapture::Pixels pixels;
    const bool ok = m_freeze->pixels(std::size_t(index), pixels);
    finish();
    if (!ok || pixels.width <= 0 || pixels.height <= 0) {
        emit failed(tr("Das Fenster konnte nicht gelesen werden."));
        return;
    }

    // BGRA with premultiplied alpha is Qt's ARGB32_Premultiplied on
    // little-endian machines (all Windows machines).
    const QImage whole(pixels.bgra.data(), pixels.width, pixels.height, pixels.width * 4,
                       QImage::Format_ARGB32_Premultiplied);
    const QRect frame = PickGeometry::frameInCapture(whole.size(), toQRect(window.windowRect), toQRect(window.frame));

    CaptureResult result;
    result.kind = CaptureResult::Kind::Window;
    result.image = whole.copy(frame).convertToFormat(QImage::Format_ARGB32);
    result.title = QString::fromStdWString(window.title);
    result.scale = window.dpi / 96.0;
    // The image is already the frame; the shared core only trims the soft
    // edge and cleans the corners (D-009, D-010). Rounded up, so the frame
    // covers every image pixel.
    const QRect logical(0, 0, int(std::ceil(result.image.width() / result.scale)),
                        int(std::ceil(result.image.height() / result.scale)));
    result.frameRect = logical;
    result.bufferRect = logical;
    result.cornerRadius = cornerRadius(window);
    emit captured(result);
}

void WindowsBackend::finish()
{
    // The frozen frames hold GPU memory; free them right away.
    m_freeze.reset();
    if (m_picker) {
        m_picker->deleteLater();
        m_picker = nullptr;
    }
}
