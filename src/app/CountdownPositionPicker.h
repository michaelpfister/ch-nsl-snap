#pragma once

#include "capture/CountdownOptions.h"

#include <QWidget>

// The setting "where the countdown appears" (F-04): a small drawn monitor
// with five dots (the four corners and the middle). A click on a dot, or
// the arrow keys, choose; the chosen dot is filled with the accent colour.
class CountdownPositionPicker : public QWidget
{
    Q_OBJECT

public:
    explicit CountdownPositionPicker(QWidget *parent = nullptr);

    CountdownPosition position() const { return m_position; }
    void setPosition(CountdownPosition position);

    // The name of a position for the user, e.g. "Oben links".
    static QString positionName(CountdownPosition position);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    // Only on a choice by the user.
    void positionChanged(CountdownPosition position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // The monitor's screen area inside the widget.
    QRectF screenRect() const;
    // The centre of the dot for `position`.
    QPointF dotCenter(CountdownPosition position) const;
    // The dot under `point`, if any.
    bool dotAt(const QPointF &point, CountdownPosition *position) const;
    void choose(CountdownPosition position);

    CountdownPosition m_position = CountdownPosition::Center;
    // The dot under the mouse, for the hover ring; -1 for none.
    int m_hovered = -1;
};
