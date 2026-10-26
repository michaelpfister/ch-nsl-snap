#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QRect>

class PickerOverlay;
class QScreen;

// The capture overlay on Windows (D-022), drawn by the app because there is
// no shell extension: one frameless full-screen window per monitor showing
// the frozen screen. The window under the pointer is highlighted; a click,
// Enter or Space picks it; Escape or a right click cancels (F-47).
class WindowPicker : public QObject
{
    Q_OBJECT

public:
    struct Screen
    {
        QScreen *screen = nullptr;
        QRect native;    // the monitor in physical desktop pixels
        QImage picture;  // the frozen monitor, native size
    };

    // `frames` are the pickable windows' frames in native pixels, topmost
    // first; picked() reports an index into this list.
    WindowPicker(const QList<Screen> &screens, const QList<QRect> &frames, QObject *parent = nullptr);
    ~WindowPicker() override;

    void show();
    int hovered() const { return m_hovered; }

signals:
    void picked(int index);
    void cancelled();

private:
    friend class PickerOverlay;
    void hoverAt(const QPoint &native);
    // -1 cancels.
    void finish(int index);

    QList<QRect> m_frames;
    QList<PickerOverlay *> m_overlays;
    int m_hovered = -1;
    bool m_done = false;
};
