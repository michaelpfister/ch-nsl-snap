#include "editor/Commands.h"

#include "editor/EditorScene.h"
#include "editor/FrameItem.h"
#include "editor/RectItem.h"
#include "editor/RedactItem.h"
#include "editor/ReplaceItem.h"
#include "editor/TextItem.h"

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

SetRectCommand::SetRectCommand(RectItem *item, const QRectF &from, const QRectF &to)
    : QUndoCommand(tr("Grösse ändern"))
    , m_item(item)
    , m_from(from)
    , m_to(to)
{
}

void SetRectCommand::redo()
{
    m_item->setRect(m_to);
}

void SetRectCommand::undo()
{
    m_item->setRect(m_from);
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

// --- Dim background ----------------------------------------------------

SetDimCommand::SetDimCommand(EditorScene *scene, const DimSettings &settings)
    : QUndoCommand(tr("Hintergrund abdunkeln"))
    , m_scene(scene)
    , m_before(scene->dimSettings())
    , m_after(settings)
{
}

void SetDimCommand::redo()
{
    m_scene->setDimSettings(m_after);
}

void SetDimCommand::undo()
{
    m_scene->setDimSettings(m_before);
}

bool SetDimCommand::mergeWith(const QUndoCommand *other)
{
    const auto *next = static_cast<const SetDimCommand *>(other);
    // Switching on or off stays its own step; slider moves merge.
    if (next->m_after.enabled != m_after.enabled || m_before.enabled != m_after.enabled)
        return false;
    m_after = next->m_after;
    return true;
}

// --- Reorder ------------------------------------------------------------

ReorderItemsCommand::ReorderItemsCommand(EditorScene *scene, int from, int to)
    : QUndoCommand(tr("Reihenfolge ändern"))
    , m_scene(scene)
    , m_items(scene->annotations())
{
    for (const RectItem *item : std::as_const(m_items))
        m_before << item->order();
    // The items keep the same set of order values, handed out anew.
    QList<RectItem *> moved = m_items;
    moved.move(from, to);
    m_after.resize(m_items.size());
    for (qsizetype i = 0; i < moved.size(); ++i)
        m_after[m_items.indexOf(moved[i])] = m_before[i];
}

void ReorderItemsCommand::apply(const QList<qint64> &orders)
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
        m_items[i]->setOrder(orders[i]);
    emit m_scene->annotationsChanged(); // repaints the numbers, updates the list
}

void ReorderItemsCommand::redo()
{
    apply(m_after);
}

void ReorderItemsCommand::undo()
{
    apply(m_before);
}

// --- Pixelate style ----------------------------------------------------

SetRedactStyleCommand::SetRedactStyleCommand(const QList<RedactItem *> &items, const RedactStyle &style)
    : QUndoCommand(tr("Verpixeln ändern"))
    , m_items(items)
    , m_after(style)
{
    for (const RedactItem *item : items)
        m_before << item->style();
}

void SetRedactStyleCommand::redo()
{
    for (RedactItem *item : std::as_const(m_items))
        item->setStyle(m_after);
}

void SetRedactStyleCommand::undo()
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
        m_items[i]->setStyle(m_before[i]);
}

// --- Replaced text -----------------------------------------------------

SetReplaceStyleCommand::SetReplaceStyleCommand(ReplaceItem *item, const ReplaceStyle &style)
    : QUndoCommand(tr("Text ändern"))
    , m_item(item)
    , m_before(item->style())
    , m_after(style)
{
}

void SetReplaceStyleCommand::redo()
{
    m_item->setStyle(m_after);
}

void SetReplaceStyleCommand::undo()
{
    m_item->setStyle(m_before);
}

bool SetReplaceStyleCommand::mergeWith(const QUndoCommand *other)
{
    const auto *next = static_cast<const SetReplaceStyleCommand *>(other);
    if (next->m_item != m_item)
        return false;
    m_after = next->m_after;
    return true;
}

// --- Free text ---------------------------------------------------------

SetTextStyleCommand::SetTextStyleCommand(const QList<TextItem *> &items, const TextStyle &look)
    : QUndoCommand(tr("Text formatieren"))
    , m_items(items)
    , m_look(look)
{
    for (const TextItem *item : items)
        m_before << item->style();
}

void SetTextStyleCommand::redo()
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        TextStyle style = m_look;
        style.text = m_before[i].text;
        m_items[i]->setStyle(style);
    }
}

void SetTextStyleCommand::undo()
{
    for (qsizetype i = 0; i < m_items.size(); ++i)
        m_items[i]->setStyle(m_before[i]);
}

bool SetTextStyleCommand::mergeWith(const QUndoCommand *other)
{
    const auto *next = static_cast<const SetTextStyleCommand *>(other);
    if (next->m_items != m_items)
        return false;
    m_look = next->m_look;
    return true;
}

EditTextCommand::EditTextCommand(TextItem *item, const QString &fromText, const QPointF &fromPos)
    : QUndoCommand(tr("Text ändern"))
    , m_item(item)
    , m_fromText(fromText)
    , m_toText(item->style().text)
    , m_fromPos(fromPos)
    , m_toPos(item->pos())
{
}

void EditTextCommand::redo()
{
    TextStyle style = m_item->style();
    style.text = m_toText;
    m_item->setStyle(style);
    m_item->setPos(m_toPos);
}

void EditTextCommand::undo()
{
    TextStyle style = m_item->style();
    style.text = m_fromText;
    m_item->setStyle(style);
    m_item->setPos(m_fromPos);
}

// --- Crop --------------------------------------------------------------

SetCropCommand::SetCropCommand(EditorScene *scene, const QRect &crop)
    : QUndoCommand(tr("Zuschneiden"))
    , m_scene(scene)
    , m_before(scene->cropRect())
    , m_after(crop)
{
}

void SetCropCommand::redo()
{
    m_scene->setCropRect(m_after);
}

void SetCropCommand::undo()
{
    m_scene->setCropRect(m_before);
}
