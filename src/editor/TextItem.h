#pragma once

#include "editor/RectItem.h"
#include "editor/TextStyle.h"

#include <QFont>

class QGraphicsTextItem;

// A free text (F-25). The text is laid out and drawn by a child
// QGraphicsTextItem, which also does the editing on the canvas; this item
// draws the optional box, is selected and moved like the other annotations,
// and its rectangle follows the text. It has no resize handles: the size
// comes from the font size in the panel.
class TextItem : public RectItem
{
public:
    enum { Type = UserType + 4 };

    TextItem(const QPointF &topLeft, const TextStyle &style, double scale);

    int type() const override { return Type; }
    QString label() const override;

    TextStyle style() const { return m_style; }
    void setStyle(const TextStyle &style);

    // Editing on the canvas: shows a text cursor and takes the keyboard.
    // The view starts and ends it and turns the result into an undo step.
    void setEditing(bool editing);
    bool isEditing() const { return m_editing; }
    // The text as typed so far (the style's text is updated only by setStyle).
    QString editedText() const;
    // The child that has the keyboard focus while editing.
    QGraphicsItem *editor() const;

    QFont font() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    bool isHiddenHandle(Handle) const override { return true; }

private:
    // Applies the style to the child and fits the rectangle to the text.
    void updateLayout();
    qreal padding() const;

    TextStyle m_style;
    QGraphicsTextItem *m_text;
    bool m_editing = false;
};
