#pragma once

#include <QColor>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;

// A row of round colour buttons: the standard colours (F-22) and, last, a
// button for a colour of one's own (rainbow ring), which opens the colour
// popup. Used for frames, arrows, texts and markers.
class ColorRow : public QWidget
{
    Q_OBJECT

public:
    // Standard: frames, arrows, texts. Marker: light felt-pen colours (F-28).
    enum class Palette { Standard, Marker };

    explicit ColorRow(Palette palette = Palette::Standard, QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    // Shows `color` without emitting colorChosen: a standard colour checks
    // its button, any other one is shown in the custom button.
    void setColor(const QColor &color);

    // True for a colour of one of the palettes; the others are own colours.
    static bool isStandard(const QColor &color);

    // The custom button, where the colour popup opens.
    QWidget *customButton() const;

signals:
    // A standard colour was clicked.
    void colorChosen(const QColor &color);
    // The custom button was clicked.
    void customRequested();

private:
    QButtonGroup *m_buttons;
    QAbstractButton *m_custom;
    QColor m_color;
};
