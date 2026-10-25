#pragma once

#include "editor/DimSettings.h"
#include "editor/FrameStyle.h"
#include "editor/RedactStyle.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QUndoCommand>

class EditorScene;
class FrameItem;
class RectItem;
class RedactItem;
class QGraphicsItem;

// Undo commands for the editor (D-012). Each command changes the scene in
// redo() and restores it in undo(). Commands that take items out of the
// scene own them until they are put back or the command is deleted.

class AddItemsCommand : public QUndoCommand
{
public:
    AddItemsCommand(EditorScene *scene, const QList<QGraphicsItem *> &items, const QString &text);
    ~AddItemsCommand() override;
    void redo() override;
    void undo() override;

private:
    EditorScene *m_scene;
    QList<QGraphicsItem *> m_items;
};

class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(EditorScene *scene, const QList<QGraphicsItem *> &items);
    ~RemoveItemsCommand() override;
    void redo() override;
    void undo() override;

private:
    EditorScene *m_scene;
    QList<QGraphicsItem *> m_items;
};

// Records a move that already happened (the user dragged the items).
class MoveItemsCommand : public QUndoCommand
{
public:
    struct Move { QGraphicsItem *item; QPointF from; QPointF to; };
    explicit MoveItemsCommand(const QList<Move> &moves);
    void redo() override;
    void undo() override;

private:
    QList<Move> m_moves;
};

// Resize of a frame or another rectangular annotation.
class SetRectCommand : public QUndoCommand
{
public:
    SetRectCommand(RectItem *item, const QRectF &from, const QRectF &to);
    void redo() override;
    void undo() override;

private:
    RectItem *m_item;
    QRectF m_from;
    QRectF m_to;
};

class SetFrameStyleCommand : public QUndoCommand
{
public:
    SetFrameStyleCommand(const QList<FrameItem *> &frames, const FrameStyle &style);
    void redo() override;
    void undo() override;

    // Dragging the line-width slider gives many changes; they merge into one step.
    int id() const override { return 1; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    QList<FrameItem *> m_frames;
    QList<FrameStyle> m_before;
    FrameStyle m_after;
};

class SetDimCommand : public QUndoCommand
{
public:
    SetDimCommand(EditorScene *scene, const DimSettings &settings);
    void redo() override;
    void undo() override;

    // Slider drags merge into one step, like SetFrameStyleCommand.
    int id() const override { return 2; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    EditorScene *m_scene;
    DimSettings m_before;
    DimSettings m_after;
};

// Changes the order of the annotations, and with it the numbers of the
// numbered frames (F-24, Q-18): the item at `from` in
// EditorScene::annotations() moves to `to`.
class ReorderItemsCommand : public QUndoCommand
{
public:
    ReorderItemsCommand(EditorScene *scene, int from, int to);
    void redo() override;
    void undo() override;

private:
    void apply(const QList<qint64> &orders);

    EditorScene *m_scene;
    QList<RectItem *> m_items; // in the original order
    QList<qint64> m_before;
    QList<qint64> m_after;
};

class SetRedactStyleCommand : public QUndoCommand
{
public:
    SetRedactStyleCommand(const QList<RedactItem *> &items, const RedactStyle &style);
    void redo() override;
    void undo() override;

private:
    QList<RedactItem *> m_items;
    QList<RedactStyle> m_before;
    RedactStyle m_after;
};
