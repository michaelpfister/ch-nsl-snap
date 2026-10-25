#pragma once

#include "editor/RectItem.h"
#include "editor/RedactStyle.h"

#include <QPixmap>

// A pixelated or blurred area (F-26). It replaces the image pixels under
// it, so the export never contains the original there (D-012). The
// rectangle is kept on whole image pixels, so no original pixel shows at
// the edges.
class RedactItem : public RectItem
{
public:
    enum { Type = UserType + 2 };

    RedactItem(const QRectF &rect, const RedactStyle &style, double scale);

    int type() const override { return Type; }
    QString label() const override;

    RedactStyle style() const { return m_style; }
    void setStyle(const RedactStyle &style);
    void setRect(const QRectF &rect) override;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // The obscured pixels for an image region; public for the tests.
    static QImage obscure(const QImage &image, const QRect &region, const RedactStyle &style, double scale);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QPixmap obscured() const;

    RedactStyle m_style;
    // Cache: recomputed when the area, the style or the image changes.
    mutable QPixmap m_cache;
    mutable QRect m_cacheRegion;
    mutable RedactStyle m_cacheStyle;
    mutable qint64 m_cacheImage = 0;
};
