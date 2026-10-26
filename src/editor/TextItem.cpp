#include "editor/TextItem.h"

#include "editor/EditorScene.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QGraphicsTextItem>
#include <QKeyEvent>
#include <QPainter>
#include <QPalette>
#include <QStyleOptionGraphicsItem>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <cmath>

namespace {

// The editing child: Escape ends editing like a click outside, by giving
// up the focus (the view listens for that).
class TextEditor : public QGraphicsTextItem
{
public:
    using QGraphicsTextItem::QGraphicsTextItem;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            clearFocus();
            event->accept();
            return;
        }
        QGraphicsTextItem::keyPressEvent(event);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        // Without Qt's dotted focus rectangle: TextItem draws its own outline.
        QStyleOptionGraphicsItem plain(*option);
        plain.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
        QGraphicsTextItem::paint(painter, &plain, widget);
    }
};

constexpr qreal kPadding = 4;      // logical px around the text, also without a box
constexpr qreal kBoxRadius = 4;    // logical px

} // namespace

TextItem::TextItem(const QPointF &topLeft, const TextStyle &style, double scale)
    : RectItem(QRectF(topLeft, QSizeF(1, 1)), scale)
    , m_style(style)
    , m_text(new TextEditor(this))
{
    // Above the frames, and like them not dimmed (F-27).
    setZValue(0.5);
    m_text->document()->setDocumentMargin(0);
    // Not clickable while not editing: clicks select and move this item.
    m_text->setAcceptedMouseButtons(Qt::NoButton);
    QObject::connect(m_text->document(), &QTextDocument::contentsChanged, m_text, [this] { updateLayout(); });
    updateLayout();
}

QString TextItem::label() const
{
    const QString text = m_style.text.simplified().isEmpty() ? QStringLiteral("…") : m_style.text.simplified();
    return QCoreApplication::translate("TextItem", "Text: %1")
        .arg(text.length() > 24 ? text.left(23) + QStringLiteral("…") : text);
}

void TextItem::setStyle(const TextStyle &style)
{
    if (style == m_style)
        return;
    m_style = style;
    updateLayout();
}

QFont TextItem::font() const
{
    // The system UI font, like the applications in the screenshot.
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPixelSize(std::max(1, int(std::lround(m_style.fontSize * imageScale()))));
    font.setBold(m_style.bold);
    return font;
}

qreal TextItem::padding() const
{
    return kPadding * imageScale();
}

void TextItem::updateLayout()
{
    // Only the look; the text is set when not editing, so typing is not disturbed.
    m_text->setFont(font());
    QColor color = m_style.color;
    if (m_style.box == TextBox::Filled)
        color = m_style.color.lightnessF() > 0.6 ? QColor(0x1F2328) : QColor(Qt::white);
    m_text->setDefaultTextColor(color);
    if (!m_editing && m_text->toPlainText() != m_style.text)
        m_text->setPlainText(m_style.text);

    // The rectangle keeps its top left corner and grows with the text. An
    // empty text keeps room for the cursor.
    const qreal p = padding();
    const QSizeF text = m_text->boundingRect().size();
    const qreal width = std::max(text.width(), m_style.fontSize * imageScale() * 0.5);
    m_text->setPos(rect().topLeft() + QPointF(p, p));
    setRect(QRectF(rect().topLeft(), QSizeF(width + 2 * p, text.height() + 2 * p)));
    update();
}

void TextItem::setEditing(bool editing)
{
    if (editing == m_editing)
        return;
    m_editing = editing;
    if (editing) {
        m_text->setTextInteractionFlags(Qt::TextEditorInteraction);
        m_text->setAcceptedMouseButtons(Qt::LeftButton);
        m_text->setFocus(Qt::MouseFocusReason);
        QTextCursor cursor(m_text->document());
        cursor.movePosition(QTextCursor::End);
        m_text->setTextCursor(cursor);
    } else {
        m_text->setTextInteractionFlags(Qt::NoTextInteraction);
        m_text->setAcceptedMouseButtons(Qt::NoButton);
        m_text->clearFocus();
        // Pasted rich text: back to the uniform look of the style.
        m_style.text = editedText();
        m_text->setPlainText(m_style.text);
    }
    // No moving while the mouse selects text.
    setFlag(ItemIsMovable, !editing);
    update();
}

QString TextItem::editedText() const
{
    return m_text->toPlainText();
}

QGraphicsItem *TextItem::editor() const
{
    return m_text;
}

QRectF TextItem::boundingRect() const
{
    const qreal m = handleMargin();
    return rect().adjusted(-m, -m, m, m);
}

QPainterPath TextItem::shape() const
{
    QPainterPath path;
    path.addRect(rect());
    return path;
}

void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());

    if (m_style.box != TextBox::None) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_style.box == TextBox::Filled ? m_style.color : QColor(Qt::white));
        const qreal radius = kBoxRadius * imageScale();
        painter->drawRoundedRect(rect(), radius, radius);
        painter->restore();
    }
    // The letters are drawn by the child.

    if ((isSelected() || m_editing) && editorScene && !editorScene->isExporting()) {
        painter->setPen(QPen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5), Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect());
    }
}
