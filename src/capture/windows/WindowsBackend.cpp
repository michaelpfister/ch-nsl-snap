#include "capture/windows/WindowsBackend.h"

#include "capture/Countdown.h"
#include "capture/PickGeometry.h"
#include "capture/windows/WgcCapture.h"
#include "capture/windows/WindowsHotkeys.h"

#include <QGuiApplication>
#include <QPainter>
#include <QOperatingSystemVersion>
#include <QScreen>
// The monitors' native (physical) geometry is only available from Qt's
// platform layer.
#include <qpa/qplatformscreen.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
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
    , m_hotkeys(new WindowsHotkeys(this))
{
    connect(m_hotkeys, &WindowsHotkeys::pressed, this, &CaptureBackend::hotkeyPressed);
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

void WindowsBackend::captureWindow()
{
    start(CaptureOverlay::Mode::Window);
}

void WindowsBackend::captureRegion()
{
    start(CaptureOverlay::Mode::Region);
}

void WindowsBackend::captureScreen()
{
    start(CaptureOverlay::Mode::Screen);
}

void WindowsBackend::captureActiveWindow()
{
    start(CaptureOverlay::Mode::Window, Direct::ActiveWindow);
}

void WindowsBackend::captureScreenAtPointer()
{
    start(CaptureOverlay::Mode::Screen, Direct::PointerScreen);
}

void WindowsBackend::setHotkeyOptions(const HotkeyOptions &options)
{
    // The app captures itself on Windows; only the keys are needed here.
    const QStringList failed = m_hotkeys->setHotkeys(options.hotkeys);
    QStringList problems = failed;
    bool plainPrint = false;
    for (HotkeyAction action : {HotkeyAction::Last, HotkeyAction::Screen, HotkeyAction::Window, HotkeyAction::Region}) {
        const QKeySequence key = options.hotkeys.key(action);
        plainPrint = plainPrint || (!key.isEmpty() && key[0] == QKeyCombination(Qt::Key_Print));
    }
    if (options.hotkeys.enabled && plainPrint && WindowsHotkeys::printOpensSnippingTool())
        problems << tr("Windows öffnet bei Print das Snipping Tool. In den Windows-Einstellungen "
                       "(Barrierefreiheit › Tastatur) «Bildschirmaufnahme mit der Druck-Taste öffnen» ausschalten.");
    m_hotkeyProblem = problems.join(u'\n');
}

QString WindowsBackend::hotkeyProblem() const
{
    return m_hotkeyProblem;
}

void WindowsBackend::start(CaptureOverlay::Mode mode, Direct direct)
{
    if (m_overlay || m_countdown)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }
    // The active window when the key was pressed; with a countdown the
    // user may switch windows meanwhile, so it is taken again at the end.
    m_targetWindow = GetForegroundWindow();
    if (delay() <= 0) {
        freezeAndPick(mode, direct);
        return;
    }
    // The countdown first (F-04); it is off the screen before the freeze.
    m_countdown = new Countdown;
    connect(m_countdown, &Countdown::finished, this, [this, mode, direct] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        m_targetWindow = GetForegroundWindow();
        freezeAndPick(mode, direct);
    });
    connect(m_countdown, &Countdown::cancelled, this, [this] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        emit cancelled();
    });
    m_countdown->start(delay());
}

void WindowsBackend::freezeAndPick(CaptureOverlay::Mode mode, Direct direct)
{
    // Freeze first (D-011): every window on its own, and the monitors for
    // the overlay's background and for region captures.
    m_freeze = WgcCapture::Freeze::capture();
    m_screens.clear();
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect native = screen->handle()->geometry();
        m_screens.append({screen, native, grabMonitor(native)});
    }
    QList<QRect> frames;
    for (const WgcCapture::Window &window : m_freeze->windows())
        frames << toQRect(window.frame);
    if (frames.isEmpty() && mode == CaptureOverlay::Mode::Window) {
        finish();
        emit failed(tr("Es wurde kein Fenster gefunden, das aufgenommen werden kann."));
        return;
    }

    // Hotkeys: the active window or the monitor under the pointer, without
    // the overlay. If there is none (e.g. the desktop is active), the
    // overlay is shown as usual.
    if (direct == Direct::ActiveWindow) {
        const auto &windows = m_freeze->windows();
        for (std::size_t i = 0; i < windows.size(); ++i) {
            if (windows[i].hwnd == static_cast<HWND>(m_targetWindow)) {
                onWindowPicked(int(i));
                return;
            }
        }
    } else if (direct == Direct::PointerScreen) {
        POINT cursor;
        if (GetCursorPos(&cursor)) {
            for (const CaptureOverlay::Screen &screen : std::as_const(m_screens)) {
                if (screen.native.contains(QPoint(cursor.x, cursor.y))) {
                    deliverPart(screen.native, CaptureResult::Kind::Screen);
                    return;
                }
            }
        }
    }

    m_overlay = new CaptureOverlay(m_screens, frames, mode,
                                   regionByDragging() ? RegionSelector::Mode::Drag : RegionSelector::Mode::Clicks, this);
    connect(m_overlay, &CaptureOverlay::windowPicked, this, &WindowsBackend::onWindowPicked);
    connect(m_overlay, &CaptureOverlay::regionSelected, this,
            [this](const QRect &native) { deliverPart(native, CaptureResult::Kind::Region); });
    connect(m_overlay, &CaptureOverlay::screenSelected, this,
            [this](const QRect &native) { deliverPart(native, CaptureResult::Kind::Screen); });
    connect(m_overlay, &CaptureOverlay::cancelled, this, [this] {
        finish();
        emit cancelled();
    });
    m_overlay->show();
}

void WindowsBackend::onWindowPicked(int index)
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

void WindowsBackend::deliverPart(const QRect &native, CaptureResult::Kind kind)
{
    double scale = 1.0;
    const QImage image = CaptureOverlay::compose(m_screens, native, &scale);
    finish();

    CaptureResult result;
    result.kind = kind;
    result.image = image;
    result.scale = scale;
    result.regionRect = native;
    emit captured(result);
}

void WindowsBackend::finish()
{
    // The frozen frames hold GPU memory; free them right away.
    m_freeze.reset();
    m_screens.clear();
    if (m_overlay) {
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
}
