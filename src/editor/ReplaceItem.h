#pragma once

#include "editor/RectItem.h"
#include "editor/ReplaceStyle.h"

#include <QFont>

// A replaced text (F-29): the area is filled with the background colour,
// which removes the original text from the image, and a new text is drawn
// on it. Like RedactItem it replaces pixels and stays on whole pixels.
class ReplaceItem : public RectItem
{
public:
    enum { Type = UserType + 3 };

    ReplaceItem(const QRectF &rect, const ReplaceStyle &style, double scale);

    int type() const override { return Type; }
    QString label() const override;

    ReplaceStyle style() const { return m_style; }
    void setStyle(const ReplaceStyle &style);

    enum class Match {
        Ok,
        CutsText,         // the area cuts the original: parts stay visible
        NoPlainBackground // photo or gradient: only the fill colour was taken
    };
    // Fills in background, text colour, size and position from the original
    // text under the area (TextSample). The text itself stays as it is.
    Match matchOriginal();

    // The font the text is drawn with (image pixels).
    QFont font() const;
    // The system UI font, the default for replaced texts: screenshots
    // mostly show applications of the same system.
    static QString defaultFontFamily();

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF textRect() const;

    ReplaceStyle m_style;
};
