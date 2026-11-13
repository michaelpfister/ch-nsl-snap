#include "capture/macos/MacBackend.h"

#include "capture/Countdown.h"
#include "capture/PickGeometry.h"
#include "capture/macos/MacCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtDebug>

#include <algorithm>
#include <cmath>

namespace {

// How long ScreenCaptureKit may take for the whole freeze.
constexpr int kFreezeTimeoutMs = 5000;

// The index of the screen that holds most of `rect`, or -1.
qsizetype screenOf(const QRect &rect, const QList<QRect> &screens)
{
    qsizetype best = -1;
    qint64 bestArea = 0;
    for (qsizetype i = 0; i < screens.size(); ++i) {
        const QRect part = rect.intersected(screens[i]);
        const qint64 area = qint64(part.width()) * part.height();
        if (area > bestArea) {
            best = i;
            bestArea = area;
        }
    }
    return best;
}

} // namespace

MacBackend::MacBackend(QObject *parent)
    : CaptureBackend(parent)
{
}

MacBackend::~MacBackend() = default;

void MacBackend::captureWindow()
{
    start(CaptureOverlay::Mode::Window);
}

void MacBackend::captureActiveWindow()
{
    start(CaptureOverlay::Mode::Window, true);
}

void MacBackend::captureRegion()
{
    start(CaptureOverlay::Mode::Region);
}

void MacBackend::captureScreen()
{
    start(CaptureOverlay::Mode::Screen);
}

bool MacBackend::checkPermission()
{
    // Without the permission macOS shows only the desktop picture. Asking
    // shows the system's question once and puts the app into the list.
    if (MacCapture::hasPermission())
        return true;
    MacCapture::requestPermission();
    emit failed(tr("Snapslash braucht die Berechtigung zur Bildschirmaufnahme. Bitte in den Systemeinstellungen "
                   "unter «Datenschutz & Sicherheit › Bildschirm- & Systemaudioaufnahme» einschalten und "
                   "Snapslash danach neu starten."));
    return false;
}

void MacBackend::pickForVideo(bool window)
{
    if (m_overlay || m_countdown)
        return;
    if (!checkPermission())
        return;
    m_forVideo = true;
    freezeAndPick(window ? CaptureOverlay::Mode::Window : CaptureOverlay::Mode::Screen, false);
}

void MacBackend::start(CaptureOverlay::Mode mode, bool activeWindow)
{
    if (m_overlay || m_countdown)
        return;
    m_forVideo = false;
    if (!checkPermission())
        return;
    if (delay() <= 0) {
        freezeAndPick(mode, activeWindow);
        return;
    }
    // The countdown first (F-04); it is off the screen before the freeze.
    m_countdown = new Countdown;
    m_countdown->setOptions(countdownOptions());
    connect(m_countdown, &Countdown::finished, this, [this, mode, activeWindow] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        freezeAndPick(mode, activeWindow);
    });
    connect(m_countdown, &Countdown::cancelled, this, [this] {
        m_countdown->deleteLater();
        m_countdown = nullptr;
        emit cancelled();
    });
    m_countdown->start(delay());
}

void MacBackend::freezeAndPick(CaptureOverlay::Mode mode, bool activeWindow)
{
    // Freeze first (D-011): the monitors and every window at once.
    const QList<QScreen *> screens = QGuiApplication::screens();
    QString diagnostic;
    m_freeze = MacCapture::Freeze::capture(screens, kFreezeTimeoutMs, &diagnostic);
    if (!m_freeze) {
        qWarning("Freeze failed: %s", qPrintable(diagnostic));
        finish();
        emit failed(tr("Der Bildschirm konnte nicht aufgenommen werden."));
        return;
    }

    // Qt's screens are in points (logical pixels); the pictures are in
    // pixels. The overlay works in pixels, so the app lays out the monitors
    // in pixels itself, as on Wayland (D-031).
    QList<QRect> logical;
    QList<QSize> physical;
    for (qsizetype i = 0; i < screens.size(); ++i) {
        logical << screens[i]->geometry();
        physical << m_freeze->screens()[i].size();
    }
    const QList<QRect> native = PickGeometry::nativeLayout(logical, physical);
    m_screens.clear();
    for (qsizetype i = 0; i < screens.size(); ++i)
        m_screens.append({screens[i], native[i], m_freeze->screens()[i]});

    // The windows' frames in those pixels, over the monitor that holds most
    // of each window. The list keeps every window, so the overlay's index
    // is the freeze's index; a window on no monitor gets an empty frame.
    m_frames.clear();
    for (const MacCapture::Window &window : m_freeze->windows()) {
        const qsizetype s = screenOf(window.frame, logical);
        m_frames << (s < 0 ? QRect() : PickGeometry::logicalToNative(window.frame, logical[s], native[s]));
    }

    if (activeWindow && !m_freeze->windows().isEmpty()) {
        // The app itself is hidden, so the topmost window is the one the
        // user worked in.
        onWindowPicked(0);
        return;
    }
    if (m_freeze->windows().isEmpty() && mode == CaptureOverlay::Mode::Window) {
        finish();
        emit failed(tr("Es wurde kein Fenster gefunden, das aufgenommen werden kann."));
        return;
    }

    m_overlay = new CaptureOverlay(m_screens, m_frames, mode,
                                   regionByDragging() ? RegionSelector::Mode::Drag : RegionSelector::Mode::Clicks, this);
    m_overlay->setForVideo(m_forVideo);
    connect(m_overlay, &CaptureOverlay::windowPicked, this, &MacBackend::onWindowPicked);
    connect(m_overlay, &CaptureOverlay::regionSelected, this,
            [this](const QRect &rect) { deliverPart(rect, CaptureResult::Kind::Region); });
    connect(m_overlay, &CaptureOverlay::screenSelected, this, [this](const QRect &rect) {
        if (m_forVideo)
            deliverVideoScreens(rect);
        else
            deliverPart(rect, CaptureResult::Kind::Screen);
    });
    connect(m_overlay, &CaptureOverlay::cancelled, this, [this] {
        finish();
        emit cancelled();
    });
    m_overlay->show();
}

void MacBackend::onWindowPicked(int index)
{
    const MacCapture::Window window = m_freeze->windows()[index];
    if (m_forVideo) {
        // The stream starts after the countdown; the frozen picture is not needed.
        VideoPick pick;
        pick.windowId = window.id;
        pick.title = window.title.isEmpty() ? window.appName : window.title;
        pick.size = m_frames.value(index).size();
        finish();
        emit videoPicked(pick);
        return;
    }
    const QImage image = m_freeze->windowImage(index);
    finish();
    if (image.isNull()) {
        emit failed(tr("Das Fenster konnte nicht gelesen werden."));
        return;
    }

    CaptureResult result;
    result.kind = CaptureResult::Kind::Window;
    // ScreenCaptureKit delivers just the window: no shadow, no margin, and
    // the rounded corners already transparent. The shared core only trims
    // the soft edge and cleans the corners (D-009, D-010).
    result.image = image.convertToFormat(QImage::Format_ARGB32);
    result.title = window.title.isEmpty() ? window.appName : window.title;
    result.appId = window.bundleId;
    result.scale = window.frame.width() > 0 ? double(image.width()) / window.frame.width() : 1.0;
    // Rounded up, so the frame covers every image pixel.
    const QRect logical(0, 0, int(std::ceil(image.width() / result.scale)),
                        int(std::ceil(image.height() / result.scale)));
    result.frameRect = logical;
    result.bufferRect = logical;
    emit captured(result);
}

void MacBackend::deliverPart(const QRect &native, CaptureResult::Kind kind)
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

void MacBackend::deliverVideoScreens(const QRect &native)
{
    // One monitor, or all of them (Enter): those inside the rectangle.
    VideoPick pick;
    for (const CaptureOverlay::Screen &screen : std::as_const(m_screens)) {
        if (native.contains(screen.native)) {
            pick.screens << screen.screen;
            pick.natives << screen.native;
        }
    }
    pick.size = native.size();
    finish();
    if (pick.screens.isEmpty()) {
        emit cancelled();
        return;
    }
    emit videoPicked(pick);
}

void MacBackend::finish()
{
    // The window pictures are large; free them right away.
    m_freeze.reset();
    m_screens.clear();
    m_frames.clear();
    if (m_overlay) {
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }
}
