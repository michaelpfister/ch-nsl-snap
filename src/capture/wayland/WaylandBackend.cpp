#include "capture/wayland/WaylandBackend.h"

#include "capture/Countdown.h"
#include "capture/PickGeometry.h"
#include "capture/wayland/WaylandFreeze.h"

#include <QGuiApplication>
#include <QScreen>

#include <cmath>
#include <cstdlib>

namespace {

// Hyprland may first ask the user whether NSL Snap may capture the screen
// (ecosystem:enforce_permissions); give them time to answer.
constexpr int kFreezeTimeoutMs = 20000;
// Up to this many image pixels the picture counts as the window itself.
constexpr int kSizeTolerance = 2;

} // namespace

WaylandBackend::WaylandBackend(QObject *parent)
    : CaptureBackend(parent)
{
}

WaylandBackend::~WaylandBackend() = default;

bool WaylandBackend::isSupported()
{
    return WaylandFreeze::canCaptureScreens();
}

bool WaylandBackend::isAvailable() const
{
    return isSupported();
}

QString WaylandBackend::unavailableReason() const
{
    return tr("Dieser Wayland-Desktop erlaubt keine Bildschirmaufnahme (wlr-screencopy fehlt).");
}

void WaylandBackend::captureWindow()
{
    start(CaptureOverlay::Mode::Window);
}

void WaylandBackend::captureRegion()
{
    start(CaptureOverlay::Mode::Region);
}

void WaylandBackend::captureScreen()
{
    start(CaptureOverlay::Mode::Screen);
}

void WaylandBackend::start(CaptureOverlay::Mode mode)
{
    if (m_overlay || m_countdown)
        return;
    if (!isAvailable()) {
        emit failed(unavailableReason());
        return;
    }
    if (mode == CaptureOverlay::Mode::Window && !(HyprlandIpc::isRunning() && WaylandFreeze::canCaptureWindows())) {
        emit failed(tr("Fenster lassen sich auf diesem Desktop nicht einzeln aufnehmen; das geht nur unter GNOME "
                       "und Hyprland. Bereich und Bildschirm funktionieren."));
        return;
    }
    if (delay() <= 0) {
        freezeAndPick(mode);
        return;
    }
    // The countdown first (F-04); it is off the screen before the freeze.
    m_countdown = new Countdown;
    connect(m_countdown, &Countdown::finished, this, [this, mode] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        freezeAndPick(mode);
    });
    connect(m_countdown, &Countdown::cancelled, this, [this] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        emit cancelled();
    });
    m_countdown->start(delay());
}

void WaylandBackend::freezeAndPick(CaptureOverlay::Mode mode)
{
    // Which windows are visible, and where (Hyprland only); asked right
    // before the freeze, so the list matches the pictures.
    QList<HyprlandIpc::Window> windows;
    if (HyprlandIpc::isRunning() && WaylandFreeze::canCaptureWindows())
        windows = HyprlandIpc::visibleWindows(HyprlandIpc::request("j/clients"), HyprlandIpc::request("j/monitors"));
    QList<uint32_t> handles;
    for (const HyprlandIpc::Window &window : std::as_const(windows))
        handles.append(window.handle);

    // Freeze first (D-011): the monitors and every window at once.
    const QList<QScreen *> screens = QGuiApplication::screens();
    const WaylandFreeze::Pictures pictures = WaylandFreeze::capture(screens, handles, kFreezeTimeoutMs);

    QList<QRect> logical;
    QList<QSize> physical;
    for (qsizetype i = 0; i < screens.size(); ++i) {
        if (pictures.screens[i].isNull()) {
            emit failed(tr("Der Bildschirm konnte nicht aufgenommen werden. Unter Hyprland mit "
                           "«ecosystem:enforce_permissions» braucht NSL Snap die Berechtigung «screencopy»."));
            return;
        }
        logical << screens[i]->geometry();
        physical << pictures.screens[i].size();
    }
    const QList<QRect> native = PickGeometry::nativeLayout(logical, physical);
    m_screens.clear();
    for (qsizetype i = 0; i < screens.size(); ++i)
        m_screens.append({screens[i], native[i], pictures.screens[i]});

    // The windows' frames in the app's native coordinates (topmost first),
    // over the monitor Hyprland says the window is on.
    QList<QRect> frames;
    m_windows.clear();
    m_windowPictures.clear();
    m_windowScales.clear();
    for (qsizetype i = 0; i < windows.size(); ++i) {
        if (pictures.windows[i].isNull())
            continue;
        for (qsizetype s = 0; s < screens.size(); ++s) {
            if (screens[s]->name() != windows[i].monitor)
                continue;
            frames << PickGeometry::logicalToNative(windows[i].rect, logical[s], native[s]);
            m_windows << windows[i];
            m_windowPictures << pictures.windows[i];
            m_windowScales << double(native[s].width()) / std::max(1, logical[s].width());
            break;
        }
    }
    if (frames.isEmpty() && mode == CaptureOverlay::Mode::Window) {
        finish();
        emit failed(tr("Es wurde kein Fenster gefunden, das aufgenommen werden kann."));
        return;
    }

    m_overlay = new CaptureOverlay(m_screens, frames, mode,
                                   regionByDragging() ? RegionSelector::Mode::Drag : RegionSelector::Mode::Clicks, this);
    connect(m_overlay, &CaptureOverlay::windowPicked, this, &WaylandBackend::onWindowPicked);
    connect(m_overlay, &CaptureOverlay::regionSelected, this,
            [this](const QRect &rect) { deliverPart(rect, CaptureResult::Kind::Region); });
    connect(m_overlay, &CaptureOverlay::screenSelected, this,
            [this](const QRect &rect) { deliverPart(rect, CaptureResult::Kind::Screen); });
    connect(m_overlay, &CaptureOverlay::cancelled, this, [this] {
        finish();
        emit cancelled();
    });
    m_overlay->show();
}

QRect WaylandBackend::frameInPicture(const QImage &picture, const QSize &windowSize, double scale)
{
    const QRect whole(0, 0, int(std::ceil(picture.width() / scale)), int(std::ceil(picture.height() / scale)));
    const int marginX = picture.width() - int(std::lround(windowSize.width() * scale));
    const int marginY = picture.height() - int(std::lround(windowSize.height() * scale));
    if (std::abs(marginX) <= kSizeTolerance && std::abs(marginY) <= kSizeTolerance)
        return whole;
    if (!picture.hasAlphaChannel())
        return whole;

    // Bounding box of the fully opaque pixels: the window without its shadow.
    const QImage argb = picture.convertToFormat(QImage::Format_ARGB32);
    int left = argb.width(), top = argb.height(), right = -1, bottom = -1;
    for (int y = 0; y < argb.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) < 255)
                continue;
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    if (right < 0)
        return whole;
    // In logical pixels, rounded outwards.
    return QRect(QPoint(int(std::floor(left / scale)), int(std::floor(top / scale))),
                 QPoint(int(std::ceil((right + 1) / scale)) - 1, int(std::ceil((bottom + 1) / scale)) - 1));
}

void WaylandBackend::onWindowPicked(int index)
{
    const HyprlandIpc::Window window = m_windows[index];
    const QImage picture = m_windowPictures[index];
    const double scale = m_windowScales[index];
    finish();

    CaptureResult result;
    result.kind = CaptureResult::Kind::Window;
    result.image = picture;
    result.title = window.title;
    result.appId = window.appClass;
    result.scale = scale;
    // The shared core trims the soft edge and cleans the corners (D-009,
    // D-010) inside the frame.
    result.bufferRect = QRect(0, 0, int(std::ceil(picture.width() / scale)), int(std::ceil(picture.height() / scale)));
    result.frameRect = frameInPicture(picture, window.rect.size(), scale);
    emit captured(result);
}

void WaylandBackend::deliverPart(const QRect &native, CaptureResult::Kind kind)
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

void WaylandBackend::finish()
{
    m_screens.clear();
    m_windows.clear();
    m_windowPictures.clear();
    m_windowScales.clear();
    if (m_overlay) {
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
}
