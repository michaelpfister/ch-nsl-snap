#include "recording/RecordingIndicator.h"

#ifdef NSL_HAVE_WAYLAND_BACKEND
#include "capture/wayland/LayerShell.h"
#endif

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// Windows 10 2004 and later; older SDKs lack the name.
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace {

constexpr int kMargin = 16; // from the monitor's edges
// The colour of a recording, as in most recorders.
const QColor kRecordRed(230, 57, 70);

} // namespace

RecordingIndicator::RecordingIndicator(QWidget *parent)
    : QWidget(parent)
{
    // On top, without frame or taskbar entry, never with the focus (as
    // the countdown, F-04).
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Klicken, um die Videoaufnahme zu beenden"));
    setAccessibleName(tr("Videoaufnahme beenden"));
    // Fixed: tiling compositors float it instead of tiling it.
    setFixedSize(190, 44);
}

void RecordingIndicator::showOn(QScreen *screen)
{
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect area = screen->availableGeometry();
        move(area.right() - width() - kMargin + 1, area.top() + kMargin);
#ifdef NSL_HAVE_WAYLAND_BACKEND
        // Wayland ignores the position: a layer surface in the corner (D-031).
        if (!m_shown)
            LayerShell::makeCornerNotification(this, screen, kMargin);
#endif
    }
    show();
    m_shown = true;
#ifdef Q_OS_WIN
    // Windows leaves the window out of every capture, the video included.
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), WDA_EXCLUDEFROMCAPTURE);
#endif
}

void RecordingIndicator::setElapsed(qint64 ms, qint64 maxMs)
{
    // Repaint only when the shown second changes.
    if (ms / 1000 == m_elapsed / 1000 && maxMs == m_max)
        return;
    m_elapsed = ms;
    m_max = maxMs;
    update();
}

QString RecordingIndicator::formatTime(qint64 ms)
{
    const qint64 seconds = std::max<qint64>(0, ms) / 1000;
    const qint64 hours = seconds / 3600;
    const qint64 minutes = seconds / 60 % 60;
    const QString rest = QStringLiteral("%1").arg(seconds % 60, 2, 10, QLatin1Char('0'));
    if (hours > 0)
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(rest);
    return QStringLiteral("%1:%2").arg(minutes).arg(rest);
}

void RecordingIndicator::paintEvent(QPaintEvent *)
{
    // The look of the countdown: a dark rounded box.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 30, 30, 225));
    painter.drawRoundedRect(QRectF(rect()), height() / 2.0, height() / 2.0);

    // Red dot: recording.
    painter.setBrush(kRecordRed);
    painter.drawEllipse(QRectF(16, height() / 2.0 - 6, 12, 12));

    QFont time = font();
    time.setPixelSize(16);
    time.setBold(true);
    painter.setFont(time);
    painter.setPen(Qt::white);
    QString text = formatTime(m_elapsed);
    if (m_max > 0)
        text += QStringLiteral(" / ") + formatTime(m_max);
    painter.drawText(QRect(38, 0, width() - 38 - 44, height()), Qt::AlignVCenter | Qt::AlignLeft, text);

    // Stop symbol: a white square in a circle on the right.
    const QRectF stop(width() - 40, 6, 32, 32);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 40));
    painter.drawEllipse(stop);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRectF(stop.center() - QPointF(6, 6), QSizeF(12, 12)), 2, 2);
}

void RecordingIndicator::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit stopClicked();
}
