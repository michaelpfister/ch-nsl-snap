#pragma once

#include "capture/RegionSelector.h"

#include <QImage>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QRect>

class OverlayWindow;
class QScreen;
class QWidget;

// The capture overlay the app draws itself, where there is no shell
// extension (Windows, D-022; later macOS): one frameless full-screen window
// per monitor showing the frozen screen. Two modes, switched with W and R
// (F-47):
// - Window: the window under the pointer is highlighted; a click, Enter or
//   Space picks it.
// - Region (D-023): two clicks or a drag (setting), with crosshair lines, a
//   magnifier and the pixel coordinates; arrow keys move by one physical
//   pixel (Ctrl: 10), Enter or Space acts as a click, Backspace discards
//   the first corner.
// Escape or a right click cancels. All positions are native (physical)
// desktop pixels.
class CaptureOverlay : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Window, Region };

    struct Screen
    {
        QScreen *screen = nullptr;
        QRect native;    // the monitor in physical desktop pixels
        QImage picture;  // the frozen monitor, native size
    };

    // `frames`: the pickable windows' frames, topmost first; windowPicked()
    // reports an index into this list. Without frames there is no window mode.
    CaptureOverlay(const QList<Screen> &screens, const QList<QRect> &frames, Mode mode,
                   RegionSelector::Mode regionMode, QObject *parent = nullptr);
    ~CaptureOverlay() override;

    void show();

    Mode mode() const { return m_mode; }
    int hoveredWindow() const { return m_hovered; }
    const RegionSelector &region() const { return m_selector; }
    // The selection point: follows the mouse, moved by the arrow keys.
    QPoint cursor() const { return m_cursor; }

    // The overlay windows, one per monitor (for tests).
    QList<QWidget *> windows() const;

signals:
    void windowPicked(int index);
    // The selected region in native pixels, both corner pixels included.
    void regionSelected(const QRect &native);
    void cancelled();

private:
    friend class OverlayWindow;

    // Input from the overlay windows, in native pixels.
    void pointerMoved(const QPoint &native);
    void pointerPressed(const QPoint &native, Qt::MouseButton button);
    void pointerReleased(const QPoint &native, Qt::MouseButton button);
    void keyPressed(int key, Qt::KeyboardModifiers modifiers);

    void setMode(Mode mode);
    void moveCursorBy(int dx, int dy);
    void checkRegionDone();
    void finish();
    void updateAll();
    const Screen *screenAt(const QPoint &native) const;

    QList<Screen> m_screens;
    QList<QRect> m_frames;
    QList<OverlayWindow *> m_windows;
    Mode m_mode;
    RegionSelector m_selector;
    QPoint m_cursor;
    int m_hovered = -1;
    bool m_hintVisible = true;
    bool m_done = false;
};
