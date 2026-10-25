#pragma once

#include <QGraphicsItem>

class EditorScene;

// Draws the dimmed background (F-27) between the base image and the
// annotations: a blurred copy of the image plus a dark veil, both clipped
// to everything outside the frames. Ignores the mouse.
class DimItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 100 };

    explicit DimItem(EditorScene *scene);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    EditorScene *m_scene;
};
