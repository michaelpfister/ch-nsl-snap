#include "capture/windows/WindowPicker.h"

#include "capture/PickGeometry.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWidget>

#include <algorithm>

namespace {

// The highlight of the GNOME overlay (stylesheet.css), so both look alike.
const QColor kHighlight(53, 132, 228, 242);
const QColor kHighlightFill(53, 132, 228, 38);
constexpr qreal kHighlightWidth = 3;
constexpr qreal kHighlightRadius = 8; // Windows 11 window corners

} // namespace

// One monitor of the picker.
class PickerOverlay : public QWidget
{
public:
    PickerOverlay(WindowPicker *picker, const WindowPicker::Screen &screen, bool primary)
        : m_picker(picker)
        , m_native(screen.native)
        , m_picture(screen.picture)
        , m_primary(primary)
    {
        // Above everything, without frame or taskbar entry.
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
        setFocusPolicy(Qt::StrongFocus);
        setScreen(screen.screen);
        setGeometry(screen.screen->geometry());
        // The picture has physical pixels; drawn at this ratio it fills the
        // monitor exactly, pixel for pixel.
        m_dpr = double(m_native.width()) / std::max(1, screen.screen->geometry().width());
        m_picture.setDevicePixelRatio(m_dpr);
    }

    void setHintVisible(bool visible)
    {
        m_hintVisible = visible;
        update();
    }
    bool hintVisible() const { return m_hintVisible; }

    // Highlights the window under a global (logical) pointer position.
    void hoverAtGlobal(const QPoint &global)
    {
        m_picker->hoverAt(PickGeometry::localToNative(QPointF(mapFromGlobal(global)), m_native, m_dpr));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.drawImage(QPointF(0, 0), m_picture);

        const int index = m_picker->hovered();
        if (index >= 0) {
            const QRectF frame = PickGeometry::nativeToLocal(m_picker->m_frames[index], m_native, m_dpr);
            if (frame.intersects(rect())) {
                painter.setRenderHint(QPainter::Antialiasing);
                const qreal inset = kHighlightWidth / 2;
                painter.setPen(QPen(kHighlight, kHighlightWidth));
                painter.setBrush(kHighlightFill);
                painter.drawRoundedRect(frame.adjusted(inset, inset, -inset, -inset), kHighlightRadius,
                                        kHighlightRadius);
            }
        }

        if (m_primary && m_hintVisible) {
            // The hint pill of the GNOME overlay.
            const QString text = tr("Fenster anklicken · Esc: Abbrechen");
            QFont font = painter.font();
            font.setPointSizeF(10);
            painter.setFont(font);
            const QFontMetrics metrics(font);
            const QSize size(metrics.horizontalAdvance(text) + 32, metrics.height() + 16);
            const QRectF pill(QPointF((width() - size.width()) / 2.0, 24), QSizeF(size));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(30, 30, 30, 230));
            painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
            painter.setPen(Qt::white);
            painter.drawText(pill, Qt::AlignCenter, text);
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        m_picker->hoverAt(PickGeometry::localToNative(event->position(), m_native, m_dpr));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            m_picker->finish(-1);
            return;
        }
        m_picker->hoverAt(PickGeometry::localToNative(event->position(), m_native, m_dpr));
        if (m_picker->hovered() >= 0)
            m_picker->finish(m_picker->hovered());
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Escape:
            m_picker->finish(-1);
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_picker->hovered() >= 0)
                m_picker->finish(m_picker->hovered());
            break;
        case Qt::Key_F1:
            for (PickerOverlay *overlay : std::as_const(m_picker->m_overlays))
                overlay->setHintVisible(!overlay->hintVisible());
            break;
        default:
            QWidget::keyPressEvent(event);
        }
    }

private:
    WindowPicker *m_picker;
    QRect m_native;
    QImage m_picture;
    double m_dpr = 1.0;
    bool m_primary = false;
    bool m_hintVisible = true;
};

WindowPicker::WindowPicker(const QList<Screen> &screens, const QList<QRect> &frames, QObject *parent)
    : QObject(parent)
    , m_frames(frames)
{
    for (const Screen &screen : screens)
        m_overlays << new PickerOverlay(this, screen, screen.screen == QGuiApplication::primaryScreen());
}

WindowPicker::~WindowPicker()
{
    qDeleteAll(m_overlays);
}

void WindowPicker::show()
{
    for (PickerOverlay *overlay : std::as_const(m_overlays))
        overlay->show();
    // Keyboard input (Escape) goes to the overlay under the pointer, or the first.
    PickerOverlay *active = m_overlays.isEmpty() ? nullptr : m_overlays.first();
    for (PickerOverlay *overlay : std::as_const(m_overlays)) {
        if (overlay->geometry().contains(QCursor::pos()))
            active = overlay;
    }
    if (active) {
        active->raise();
        active->activateWindow();
        active->setFocus();
        // Highlight the window under the pointer right away, without a move.
        active->hoverAtGlobal(QCursor::pos());
    }
}

void WindowPicker::hoverAt(const QPoint &native)
{
    const int index = PickGeometry::topmostAt(m_frames, native);
    if (index == m_hovered)
        return;
    m_hovered = index;
    for (PickerOverlay *overlay : std::as_const(m_overlays))
        overlay->update();
}

void WindowPicker::finish(int index)
{
    if (m_done)
        return;
    m_done = true;
    for (PickerOverlay *overlay : std::as_const(m_overlays))
        overlay->hide();
    if (index >= 0)
        emit picked(index);
    else
        emit cancelled();
}
