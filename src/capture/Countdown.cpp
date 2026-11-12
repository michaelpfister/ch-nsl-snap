#include "capture/Countdown.h"

#include "capture/CaptureHints.h"
#include "ui/LogoMark.h"

#ifdef NSL_HAVE_WAYLAND_BACKEND
#include "capture/wayland/LayerShell.h"
#endif

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// After the display is hidden, wait this long (ms) before the freeze, so the
// compositor has drawn a frame without it.
constexpr int kSettleMs = 150;
// The pill: ring on the left, text to its right (mockup "Timer").
constexpr int kPad = 10;
constexpr int kRing = 56;
constexpr int kGap = 12;
constexpr int kHeight = kRing + 2 * kPad;

// Blue, and over the last half of the time slowly red.
QColor ringColor(qreal rest)
{
    const qreal t = std::clamp((0.5 - rest) / 0.5, 0.0, 1.0);
    const QColor blue(76, 141, 255);
    const QColor red(240, 82, 82);
    return QColor::fromRgbF(blue.redF() + (red.redF() - blue.redF()) * t,
                            blue.greenF() + (red.greenF() - blue.greenF()) * t,
                            blue.blueF() + (red.blueF() - blue.blueF()) * t);
}
#ifdef Q_OS_WIN
constexpr int kEscapeHotkey = 0x4E53; // any id, unique within this window
#endif

} // namespace

Countdown::Countdown(QWidget *parent)
    : QWidget(parent)
{
    // On top, without frame or taskbar entry, and never with the focus.
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    // Fixed: tiling Wayland compositors (Sway, Hyprland) float windows
    // that cannot be resized, instead of tiling them across the monitor.
    fitToText();
    m_mark = new LogoAnimator(this);
    connect(m_mark, &LogoAnimator::changed, this, qOverload<>(&QWidget::update));
    connect(m_mark, &LogoAnimator::finished, this, &Countdown::onMarkFinished);
    m_tick.setInterval(1000);
    setOptions({});
    connect(&m_tick, &QTimer::timeout, this, &Countdown::tick);
}

Countdown::~Countdown()
{
    stop();
}

bool Countdown::hasGlobalEscape()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void Countdown::fitToText()
{
    // As wide as the longer of title and hint; the title for two digits.
    QFont title = font();
    title.setPixelSize(14);
    title.setWeight(QFont::DemiBold);
    QFont hint = font();
    hint.setPixelSize(12);
    const int text = std::max(QFontMetrics(title).horizontalAdvance(CaptureHints::countdownTitle()),
                              QFontMetrics(hint).horizontalAdvance(CaptureHints::countdown(m_options.clickCancels)));
    setFixedSize(kPad + kRing + kGap + text + 2 * kPad, kHeight);
}

void Countdown::setOptions(const CountdownOptions &options)
{
    m_options = options;
    // Without a global Escape the click is the only way to cancel.
    if (!hasGlobalEscape())
        m_options.clickCancels = true;
    // Click-through: the window takes no input at all, so a click lands in
    // the window below it (Qt::WindowTransparentForInput; on Wayland an
    // empty input region). Only before the first show.
    if (!m_shown)
        setWindowFlag(Qt::WindowTransparentForInput, !m_options.clickCancels);
    setAttribute(Qt::WA_TransparentForMouseEvents, !m_options.clickCancels);
    if (!m_shown)
        fitToText();
    if (m_options.clickCancels) {
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("Klicken zum Abbrechen"));
    } else {
        unsetCursor();
        setToolTip(QString());
    }
}

void Countdown::start(int seconds)
{
    m_left = seconds;
    m_total = std::max(1, seconds);
    m_running = true;
    // Empty, wound up first (D-043): the mark turns back while the ring fills.
    m_mark->setPose(LogoMark::timer(0));
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        move(CountdownOptions::place(m_options.position, screen->geometry(), screen->availableGeometry(), size()));
#ifdef NSL_HAVE_WAYLAND_BACKEND
        // Wayland ignores the position: a layer surface in the middle or
        // a corner of the monitor where the compositor offers one (D-031).
        // Only before the first show; the widget keeps its role afterwards.
        if (!m_shown)
            LayerShell::makeNotification(this, screen, m_options.position, CountdownOptions::kMargin);
#endif
    }
    show();
    m_shown = true;
#ifdef Q_OS_WIN
    // Escape from anywhere: the key goes to the window with the focus.
    RegisterHotKey(reinterpret_cast<HWND>(winId()), kEscapeHotkey, MOD_NOREPEAT, VK_ESCAPE);
#endif
    m_phase = Phase::WindUp;
    m_mark->play(LogoAnimator::windUpSteps());
}

void Countdown::onMarkFinished()
{
    // The animation leads, the seconds only orient (D-043): each phase
    // starts when the mark has finished the one before.
    if (!m_running)
        return;
    switch (m_phase) {
    case Phase::WindUp:
        m_phase = Phase::Ticking;
        m_tick.start();
        break;
    case Phase::Ticking:
        break;
    case Phase::LastTurn: {
        // The ring is gone: the corners snap like a shutter.
        m_phase = Phase::Snap;
        LogoPose shut = LogoMark::timer(0);
        shut.c1Offset = QPointF(6, 6);
        shut.c2Offset = QPointF(-6, -6);
        m_mark->play({{shut, 120, QEasingCurve::InOutQuad}, {LogoMark::timer(0), 160, QEasingCurve::OutCubic}});
        break;
    }
    case Phase::Snap:
        stop();
        // Gone from the screen before the freeze.
        QTimer::singleShot(kSettleMs, this, &Countdown::finished);
        break;
    }
}

void Countdown::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_tick.stop();
    m_mark->stop();
#ifdef Q_OS_WIN
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), kEscapeHotkey);
#endif
    hide();
}

void Countdown::cancel()
{
    if (!m_running)
        return;
    stop();
    emit cancelled();
}

void Countdown::tick()
{
    m_left = std::max(0, m_left - 1);
    if (m_left == 0) {
        // The last turn runs to its end, then the snap (onMarkFinished).
        m_tick.stop();
        m_phase = Phase::LastTurn;
    }
    m_mark->play(LogoAnimator::timerSteps(qreal(m_left) / m_total));
}

void Countdown::paintEvent(QPaintEvent *)
{
    // The look of the GNOME countdown (countdown.js, stylesheet.css): a dark
    // pill with the ring and the mark, the title and the hint.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(22, 24, 28, 235));
    painter.drawRoundedRect(QRectF(rect()), 16, 16);

    // The ring: a grey track, and the rest of the time from the top,
    // clockwise; it empties counterclockwise, as the mark turns.
    const LogoPose &pose = m_mark->pose();
    const QRectF ring(kPad + 2.5, kPad + 2.5, kRing - 5, kRing - 5);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0x3A, 0x3F, 0x46), 4));
    painter.drawEllipse(ring);
    if (pose.ring > 0) {
        painter.setPen(QPen(ringColor(pose.ring), 4, Qt::SolidLine, Qt::RoundCap));
        // Qt counts in 1/16 degree, counterclockwise from three o'clock.
        painter.drawArc(ring, 90 * 16, -qRound(pose.ring * 360 * 16));
    }
    const qreal mark = 28;
    LogoMark::paint(painter, QRectF(kPad + (kRing - mark) / 2, kPad + (kRing - mark) / 2, mark, mark), pose,
                    QColor(0xE9, 0xEC, 0xEF), 5);

    const int textLeft = kPad + kRing + kGap;
    QFont title = font();
    title.setPixelSize(14);
    title.setWeight(QFont::DemiBold);
    painter.setFont(title);
    painter.setPen(QColor(0xE9, 0xEC, 0xEF));
    painter.drawText(QRect(textLeft, kPad + 8, width() - textLeft, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     CaptureHints::countdownTitle());
    QFont hint = font();
    hint.setPixelSize(12);
    painter.setFont(hint);
    painter.setPen(QColor(0xA2, 0xA9, 0xB3));
    painter.drawText(QRect(textLeft, kPad + 30, width() - textLeft, 18), Qt::AlignLeft | Qt::AlignVCenter,
                     CaptureHints::countdown(m_options.clickCancels));
}

void Countdown::mousePressEvent(QMouseEvent *event)
{
    if (m_options.clickCancels)
        cancel();
    else
        QWidget::mousePressEvent(event);
}

void Countdown::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        cancel();
    else
        QWidget::keyPressEvent(event);
}

bool Countdown::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<const MSG *>(message);
    if (eventType == "windows_generic_MSG" && msg->message == WM_HOTKEY && msg->wParam == WPARAM(kEscapeHotkey)) {
        cancel();
        *result = 0;
        return true;
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
