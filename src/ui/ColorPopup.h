#pragma once

#include <QColor>
#include <QFrame>

class ColorWheel;
class QLabel;
class QLineEdit;
class QToolButton;

// The popup for a colour of one's own: colour wheel, hex code and an
// eyedropper that takes a colour from the image. Every change is applied
// right away (colorChanged); a click outside closes it.
class ColorPopup : public QFrame
{
    Q_OBJECT

public:
    explicit ColorPopup(QWidget *parent = nullptr);

    // Opens below `anchor` (or above it, if there is no room) with `color`.
    void showAt(QWidget *anchor, const QColor &color);
    QColor color() const { return m_color; }

    // Icon colour of the eyedropper button, from the theme.
    void setIconColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);
    // The eyedropper button: the popup has closed itself, the editor
    // should let the user click into the image.
    void eyedropperRequested();

private:
    // Shows `color` in the wheel, the hex field and the preview.
    void showColor(const QColor &color, bool updateHex = true);
    void userChose(const QColor &color, bool updateHex = true);

    ColorWheel *m_wheel;
    QLineEdit *m_hex;
    QLabel *m_preview;
    QToolButton *m_eyedropper;
    QColor m_color;
};
