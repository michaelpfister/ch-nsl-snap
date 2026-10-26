#pragma once

#include "editor/MarkerStyle.h"
#include "editor/RectItem.h"

// A text marker (F-28): a rectangle of felt-pen colour over a text. It is
// multiplied with the image, so black text stays black and white turns
// into the marker colour. On dark backgrounds it shows little, like a real
// highlighter.
class MarkerItem : public RectItem
{
public:
    enum { Type = UserType + 5 };

    MarkerItem(const QRectF &rect, const MarkerStyle &style, double scale);

    int type() const override { return Type; }
    QString label() const override;

    MarkerStyle style() const { return m_style; }
    void setStyle(const MarkerStyle &style);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    MarkerStyle m_style;
};
