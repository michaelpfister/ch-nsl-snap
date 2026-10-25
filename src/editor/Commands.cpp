#include "editor/Commands.h"

#include "editor/EditorScene.h"
#include "editor/FrameItem.h"

#include <QCoreApplication>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Commands", text);
}

} // namespace

// --- Add ---------------------------------------------------------------

AddItemsCommand::AddItemsCommand(EditorScene *scene, const QList<QGraphicsItem *> &items, const QString &text)
    : QUndoCommand(text)
    , m_scene(scene)
    , m_items(items)
{
}

AddItemsCommand::~AddItemsCommand()
{
    // Items that are not in the scene (undone) belong to this command.
    for (QGraphicsItem *item : std::as_const(m_items)) {
        if (!item->scene())
            delete item;
    }
}

void AddItemsCommand::redo()
{
    m_scene->clearSelection();
    for (QGraphicsItem *item : std::as_const(m_items)) {
        if (!item->scene())
            m_scene->addItem(item);
        item->setSelected(true);
    }
    emit m_scene->annotationsChanged();
}

void AddItemsCommand::undo()
{
    for (QGraphicsItem *item : std::as_const(m_items))
        m_scene->removeItem(item);
    emit m_scene->annotationsChanged();
}

// --- Remove ------------------------------------------------------------

RemoveItemsCommand::RemoveItemsCommand(EditorScene *scene, const QList<QGraphicsItem *> &items)
    : QUndoCommand(items.size() == 1 ? tr("Objekt löschen") : tr("Objekte löschen"))
    , m_scene(scene)
    , m_items(items)
{
}

RemoveItemsCommand::~RemoveItemsCommand()
{
    for (QGraphicsItem *item : std::as_const(m_items)) {
        if (!item->scene())
            delete item;
    }
}

void RemoveItemsCommand::redo()
{
    for (QGraphicsItem *item : std::as_const(m_items))
        m_scene->removeItem(item);
    emit m_scene->annotationsChanged();
}

void RemoveItemsCommand::undo()
{
    m_scene->clearSelection();
    for (QGraphicsItem *item : std::as_const(m_items)) {
        m_scene->addItem(item);
        item->setSelected(true);
    }
    emit m_scene->annotationsChanged();
}

// --- Move --------------------------------------------------------------

MoveItemsCommand::MoveItemsCommand(const QList<Move> &moves)
    : QUndoCommand(tr("Verschieben"))
    , m_moves(moves)
{
}

void MoveItemsCommand::redo()
{
    for (const Move &move : std::as_const(m_moves))
        move.item->setPos(move.to);
}

void MoveItemsCommand::undo()
{
    for (const Move &move : std::as_const(m_moves))
        move.item->setPos(move.from);
}

// --- Frame rectangle ---------------------------------------------------

SetFrameRectCommand::SetFrameRectCommand(FrameItem *frame, const QRectF &from, const QRectF &to)
    : QUndoCommand(tr("Grösse ändern"))
    , m_frame(frame)
    , m_from(from)
    , m_to(to)
{
}

void SetFrameRectCommand::redo()
{
    m_frame->setRect(m_to);
}

void SetFrameRectCommand::undo()
{
    m_frame->setRect(m_from);
}

// --- Frame style -------------------------------------------------------

SetFrameStyleCommand::SetFrameStyleCommand(const QList<FrameItem *> &frames, const FrameStyle &style)
    : QUndoCommand(tr("Darstellung ändern"))
    , m_frames(frames)
    , m_after(style)
{
    for (const FrameItem *frame : frames)
        m_before << frame->style();
}

void SetFrameStyleCommand::redo()
{
    for (FrameItem *frame : std::as_const(m_frames))
        frame->setStyle(m_after);
}

void SetFrameStyleCommand::undo()
{
    for (qsizetype i = 0; i < m_frames.size(); ++i)
        m_frames[i]->setStyle(m_before[i]);
}

bool SetFrameStyleCommand::mergeWith(const QUndoCommand *other)
{
    const auto *next = static_cast<const SetFrameStyleCommand *>(other);
    if (next->m_frames != m_frames)
        return false;
    m_after = next->m_after;
    return true;
}
