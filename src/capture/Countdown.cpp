#include "capture/Countdown.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>

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
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Klicken zum Abbrechen"));
    resize(220, 140);
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &Countdown::tick);
}

Countdown::~Countdown()
{
    stop();
}

void Countdown::start(int seconds)
{
    m_left = seconds;
    m_running = true;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->geometry();
        move(area.center() - rect().center());
    }
    show();
#ifdef Q_OS_WIN
    // Escape from anywhere: the key goes to the window with the focus.
    RegisterHotKey(reinterpret_cast<HWND>(winId()), kEscapeHotkey, MOD_NOREPEAT, VK_ESCAPE);
#endif
    m_tick.start();
}

void Countdown::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_tick.stop();
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
    if (--m_left > 0) {
        update();
        return;
    }
    stop();
    // Gone from the screen before the freeze.
    QTimer::singleShot(kSettleMs, this, &Countdown::finished);
}

void Countdown::paintEvent(QPaintEvent *)
{
    // The look of the GNOME countdown (stylesheet.css): a dark rounded box
    // with the seconds and a hint.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 30, 30, 225));
    painter.drawRoundedRect(QRectF(rect()), 24, 24);

    QFont number = font();
    number.setPixelSize(64);
    number.setBold(true);
    painter.setFont(number);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 8, width(), 90), Qt::AlignCenter, QString::number(m_left));

    QFont hint = font();
    hint.setPixelSize(12);
    painter.setFont(hint);
    painter.setPen(QColor(255, 255, 255, 190));
    painter.drawText(QRect(0, 98, width(), 30), Qt::AlignCenter, tr("Esc oder Klick: Abbrechen"));
}

void Countdown::mousePressEvent(QMouseEvent *)
{
    cancel();
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
