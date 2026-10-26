#include "capture/CaptureOverlay.h"

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
#include <cmath>

namespace {

// The highlight of the GNOME overlay (stylesheet.css), so both look alike.
const QColor kHighlight(53, 132, 228, 242);
const QColor kHighlightFill(53, 132, 228, 38);
constexpr qreal kHighlightWidth = 3;
constexpr qreal kHighlightRadius = 8; // Windows 11 window corners
// Region mode: what is not captured is darkened, as on GNOME.
const QColor kShade(0, 0, 0, 115);

// The magnifier (D-023): an odd number of pixels, so the selection point
// is the middle one, each shown as a square of kZoom screen pixels.
constexpr int kMagnifierPixels = 15;
constexpr int kZoom = 11;
constexpr int kMagnifierSize = kMagnifierPixels * kZoom;
constexpr int kMagnifierGap = 24; // between the pointer and the magnifier

} // namespace

// One monitor of the overlay.
class OverlayWindow : public QWidget
{
public:
    OverlayWindow(CaptureOverlay *overlay, const CaptureOverlay::Screen &screen, bool primary)
        : m_overlay(overlay)
        , m_native(screen.native)
        , m_picture(screen.picture)
        , m_primary(primary)
    {
        // Above everything, without frame or taskbar entry.
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
        setFocusPolicy(Qt::StrongFocus);
        if (screen.screen) {
            setScreen(screen.screen);
            setGeometry(screen.screen->geometry());
        }
        // Physical pixels per logical pixel of this monitor.
        m_dpr = double(m_native.width()) / std::max(1, width());
    }

    const QRect &native() const { return m_native; }

    QPointF toLocal(const QPoint &native) const
    {
        return QPointF(native - m_native.topLeft()) / m_dpr;
    }
    QPoint toNative(const QPointF &local) const
    {
        return PickGeometry::localToNative(local, m_native, m_dpr);
    }

    // The real pointer was moved by the arrow keys; the move event it
    // causes is rounded to logical pixels and must not undo the step.
    void ignoreMoveAt(const QPoint &global) { m_ignoreMoveAt = global; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        // Native size into the logical rectangle: one picture pixel per
        // physical screen pixel.
        painter.drawImage(QRectF(rect()), m_picture);
        if (m_overlay->mode() == CaptureOverlay::Mode::Window)
            paintWindowHighlight(painter);
        else
            paintRegion(painter);
        if (m_primary && m_overlay->m_hintVisible)
            paintHint(painter);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->globalPosition().toPoint() == m_ignoreMoveAt)
            return;
        m_ignoreMoveAt = QPoint(-1000000, -1000000);
        m_overlay->pointerMoved(toNative(event->position()));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_overlay->pointerPressed(toNative(event->position()), event->button());
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_overlay->pointerReleased(toNative(event->position()), event->button());
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        m_overlay->keyPressed(event->key(), event->modifiers());
    }

private:
    void paintWindowHighlight(QPainter &painter)
    {
        const int index = m_overlay->hoveredWindow();
        if (index < 0)
            return;
        const QRectF frame = PickGeometry::nativeToLocal(m_overlay->m_frames[index], m_native, m_dpr);
        if (!frame.intersects(rect()))
            return;
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal inset = kHighlightWidth / 2;
        painter.setPen(QPen(kHighlight, kHighlightWidth));
        painter.setBrush(kHighlightFill);
        painter.drawRoundedRect(frame.adjusted(inset, inset, -inset, -inset), kHighlightRadius, kHighlightRadius);
        painter.restore();
    }

    void paintRegion(QPainter &painter)
    {
        const RegionSelector &selector = m_overlay->region();
        const QRect region = selector.region();

        // Darken what is not captured; everything while there is no region.
        QPainterPath shade;
        shade.addRect(QRectF(rect()));
        QRectF local;
        if (!region.isEmpty()) {
            local = PickGeometry::nativeToLocal(region, m_native, m_dpr);
            QPainterPath cut;
            cut.addRect(local);
            shade = shade.subtracted(cut);
        }
        painter.fillPath(shade, kShade);
        if (!region.isEmpty()) {
            QPen outline(kHighlight, 1);
            outline.setCosmetic(true);
            painter.setPen(outline);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(local);
        }

        const QPoint cursor = m_overlay->cursor();
        if (!m_native.contains(cursor))
            return;
        // Crosshair lines across the whole monitor through the middle of
        // the selection pixel: a light and a dark line, visible anywhere.
        const QPointF centre = toLocal(cursor) + QPointF(0.5, 0.5) / m_dpr;
        painter.setPen(QPen(QColor(0, 0, 0, 150), 1));
        painter.drawLine(QPointF(centre.x() + 1, 0), QPointF(centre.x() + 1, height()));
        painter.drawLine(QPointF(0, centre.y() + 1), QPointF(width(), centre.y() + 1));
        painter.setPen(QPen(QColor(255, 255, 255, 220), 1));
        painter.drawLine(QPointF(centre.x(), 0), QPointF(centre.x(), height()));
        painter.drawLine(QPointF(0, centre.y()), QPointF(width(), centre.y()));

        paintMagnifier(painter, centre, cursor, selector);
    }

    void paintMagnifier(QPainter &painter, const QPointF &centre, const QPoint &cursor, const RegionSelector &selector)
    {
        const QString label = selector.hasStart()
            ? QStringLiteral("%1, %2 · %3 × %4").arg(cursor.x()).arg(cursor.y())
                  .arg(selector.region().width()).arg(selector.region().height())
            : QStringLiteral("%1, %2").arg(cursor.x()).arg(cursor.y());
        const QFontMetrics metrics(painter.font());
        const int labelHeight = metrics.height() + 6;

        // Below right of the pointer; flipped where it would leave the monitor.
        QPointF pos = centre + QPointF(kMagnifierGap, kMagnifierGap);
        if (pos.x() + kMagnifierSize > width())
            pos.setX(centre.x() - kMagnifierGap - kMagnifierSize);
        if (pos.y() + kMagnifierSize + labelHeight > height())
            pos.setY(centre.y() - kMagnifierGap - kMagnifierSize - labelHeight);
        const QRectF box(pos, QSizeF(kMagnifierSize, kMagnifierSize));

        painter.save();
        painter.fillRect(box, QColor(20, 20, 20));
        // The pixels around the selection point, as hard squares.
        const QPoint inPicture = cursor - m_native.topLeft();
        const int half = kMagnifierPixels / 2;
        const QRect source(inPicture - QPoint(half, half), QSize(kMagnifierPixels, kMagnifierPixels));
        const QRect inside = source.intersected(m_picture.rect());
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(QRectF(pos.x() + (inside.x() - source.x()) * kZoom, pos.y() + (inside.y() - source.y()) * kZoom,
                                 inside.width() * kZoom, inside.height() * kZoom),
                          m_picture, inside);
        // The middle row and column, and the selection pixel itself.
        painter.setPen(QPen(QColor(255, 255, 255, 140), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(pos.x(), pos.y() + half * kZoom, kMagnifierSize, kZoom));
        painter.drawRect(QRectF(pos.x() + half * kZoom, pos.y(), kZoom, kMagnifierSize));
        const QRectF pixel(pos.x() + half * kZoom, pos.y() + half * kZoom, kZoom, kZoom);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(pixel.adjusted(-1, -1, 1, 1));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(pixel);
        painter.drawRect(box);

        // Coordinates (and the size once the first corner is set), below.
        const QRectF labelBox(box.left(), box.bottom() + 2, std::max<qreal>(metrics.horizontalAdvance(label) + 12, 0),
                              labelHeight);
        painter.fillRect(labelBox, QColor(20, 20, 20, 230));
        painter.setPen(Qt::white);
        painter.drawText(labelBox, Qt::AlignCenter, label);
        painter.restore();
    }

    void paintHint(QPainter &painter)
    {
        QString text;
        const RegionSelector &selector = m_overlay->region();
        if (m_overlay->mode() == CaptureOverlay::Mode::Window)
            text = tr("Fenster anklicken · R: Bereich · Esc: Abbrechen");
        else if (selector.hasStart())
            text = tr("Gegenüberliegende Ecke anklicken · Rücktaste: neu beginnen · Esc: Abbrechen");
        else if (selector.mode() == RegionSelector::Mode::Drag)
            text = tr("Bereich aufziehen · Pfeiltasten und Enter: pixelgenau · W: Fenster · Esc: Abbrechen");
        else
            text = tr("Erste Ecke anklicken · Pfeiltasten: 1 Pixel · W: Fenster · Esc: Abbrechen");

        painter.save();
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
        painter.restore();
    }

    CaptureOverlay *m_overlay;
    QRect m_native;
    QImage m_picture;
    double m_dpr = 1.0;
    bool m_primary = false;
    QPoint m_ignoreMoveAt = QPoint(-1000000, -1000000);
};

CaptureOverlay::CaptureOverlay(const QList<Screen> &screens, const QList<QRect> &frames, Mode mode,
                               RegionSelector::Mode regionMode, QObject *parent)
    : QObject(parent)
    , m_screens(screens)
    , m_frames(frames)
    , m_mode(frames.isEmpty() ? Mode::Region : mode)
    , m_selector(regionMode)
{
    for (const Screen &screen : screens)
        m_windows << new OverlayWindow(this, screen, screen.screen == QGuiApplication::primaryScreen());
}

CaptureOverlay::~CaptureOverlay()
{
    qDeleteAll(m_windows);
}

QList<QWidget *> CaptureOverlay::windows() const
{
    QList<QWidget *> result;
    for (OverlayWindow *window : m_windows)
        result << window;
    return result;
}

void CaptureOverlay::show()
{
    for (OverlayWindow *window : std::as_const(m_windows))
        window->show();
    // Keyboard input goes to the overlay under the pointer, or the first.
    OverlayWindow *active = m_windows.isEmpty() ? nullptr : m_windows.first();
    for (OverlayWindow *window : std::as_const(m_windows)) {
        if (window->geometry().contains(QCursor::pos()))
            active = window;
    }
    if (!active)
        return;
    active->raise();
    active->activateWindow();
    active->setFocus();
    // Start at the pointer, without waiting for a move.
    pointerMoved(active->toNative(QPointF(active->mapFromGlobal(QCursor::pos()))));
}

const CaptureOverlay::Screen *CaptureOverlay::screenAt(const QPoint &native) const
{
    for (const Screen &screen : m_screens) {
        if (screen.native.contains(native))
            return &screen;
    }
    return nullptr;
}

void CaptureOverlay::updateAll()
{
    for (OverlayWindow *window : std::as_const(m_windows))
        window->update();
}

void CaptureOverlay::pointerMoved(const QPoint &native)
{
    if (m_done)
        return;
    m_cursor = native;
    if (m_mode == Mode::Window)
        m_hovered = PickGeometry::topmostAt(m_frames, native);
    else
        m_selector.move(native);
    updateAll();
}

void CaptureOverlay::pointerPressed(const QPoint &native, Qt::MouseButton button)
{
    if (m_done)
        return;
    if (button != Qt::LeftButton) {
        finish();
        emit cancelled();
        return;
    }
    m_cursor = native;
    if (m_mode == Mode::Window) {
        m_hovered = PickGeometry::topmostAt(m_frames, native);
        if (m_hovered >= 0) {
            finish();
            emit windowPicked(m_hovered);
        }
        return;
    }
    m_selector.press(native);
    checkRegionDone();
    updateAll();
}

void CaptureOverlay::pointerReleased(const QPoint &native, Qt::MouseButton button)
{
    if (m_done || button != Qt::LeftButton || m_mode != Mode::Region)
        return;
    m_cursor = native;
    m_selector.release(native);
    checkRegionDone();
    updateAll();
}

void CaptureOverlay::keyPressed(int key, Qt::KeyboardModifiers modifiers)
{
    if (m_done)
        return;
    const int step = (modifiers & Qt::ControlModifier) ? 10 : 1;
    switch (key) {
    case Qt::Key_Escape:
        finish();
        emit cancelled();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (m_mode == Mode::Window) {
            if (m_hovered >= 0) {
                finish();
                emit windowPicked(m_hovered);
            }
            return;
        }
        m_selector.click(m_cursor);
        checkRegionDone();
        break;
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
        m_selector.discard();
        break;
    case Qt::Key_W:
        if (!m_frames.isEmpty())
            setMode(Mode::Window);
        break;
    case Qt::Key_R:
        setMode(Mode::Region);
        break;
    case Qt::Key_F1:
        m_hintVisible = !m_hintVisible;
        break;
    case Qt::Key_Left: moveCursorBy(-step, 0); return;
    case Qt::Key_Right: moveCursorBy(step, 0); return;
    case Qt::Key_Up: moveCursorBy(0, -step); return;
    case Qt::Key_Down: moveCursorBy(0, step); return;
    default:
        return;
    }
    updateAll();
}

void CaptureOverlay::setMode(Mode mode)
{
    if (mode == m_mode)
        return;
    m_mode = mode;
    m_selector = RegionSelector(m_selector.mode());
    m_hovered = mode == Mode::Window ? PickGeometry::topmostAt(m_frames, m_cursor) : -1;
}

void CaptureOverlay::moveCursorBy(int dx, int dy)
{
    // One physical pixel per key press, on the monitors only.
    const QPoint target = m_cursor + QPoint(dx, dy);
    if (!screenAt(target))
        return;
    pointerMoved(target);
    // Move the real pointer along, so the next mouse move continues here.
    for (OverlayWindow *window : std::as_const(m_windows)) {
        if (!window->native().contains(target))
            continue;
        const QPoint global = window->mapToGlobal(window->toLocal(target).toPoint());
        window->ignoreMoveAt(global);
        QCursor::setPos(window->screen(), global);
    }
}

void CaptureOverlay::checkRegionDone()
{
    if (m_selector.state() != RegionSelector::State::Done)
        return;
    finish();
    emit regionSelected(m_selector.region());
}

void CaptureOverlay::finish()
{
    if (m_done)
        return;
    m_done = true;
    for (OverlayWindow *window : std::as_const(m_windows))
        window->hide();
}
