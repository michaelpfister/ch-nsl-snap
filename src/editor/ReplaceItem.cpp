#include "editor/ReplaceItem.h"

#include "editor/EditorScene.h"
#include "imaging/TextSample.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <cmath>

ReplaceItem::ReplaceItem(const QRectF &rect, const ReplaceStyle &style, double scale)
    : RectItem(rect, scale, true)
    , m_style(style)
{
    // Like pixelated areas: above the dimmed background (D-018).
    setZValue(-0.25);
}

QString ReplaceItem::label() const
{
    const QString text = m_style.text.isEmpty() ? QStringLiteral("…") : m_style.text;
    return QCoreApplication::translate("ReplaceItem", "Text: %1")
        .arg(text.length() > 24 ? text.left(23) + QStringLiteral("…") : text);
}

void ReplaceItem::setStyle(const ReplaceStyle &style)
{
    if (style == m_style)
        return;
    prepareGeometryChange(); // the text may be wider than the area
    m_style = style;
}

QString ReplaceItem::defaultFontFamily()
{
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}

QFont ReplaceItem::font() const
{
    QFont font(m_style.fontFamily.isEmpty() ? defaultFontFamily() : m_style.fontFamily);
    font.setPixelSize(std::max(1, int(std::lround(m_style.fontSize * imageScale()))));
    font.setBold(m_style.bold);
    return font;
}

ReplaceItem::Match ReplaceItem::matchOriginal()
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    if (!editorScene)
        return Match::Ok;
    const QRect region = mapRectToScene(rect()).toAlignedRect();
    const TextSample sample = TextSample::analyze(editorScene->baseImage(), region);
    const double scale = imageScale();

    ReplaceStyle style = m_style;
    style.background = sample.background;
    if (!sample.reliable) {
        // No usable text: fill with the main colour, write in a contrasting
        // colour, centred, at the current size.
        style.color = sample.background.lightnessF() > 0.5 ? QColor(0x1F2328) : QColor(Qt::white);
        style.textLeft = 4;
        style.textBaseline = 0;
        setStyle(style);
        return sample.ink.isEmpty() ? Match::Ok : Match::NoPlainBackground;
    }
    style.color = sample.text;
    style.fontSize = std::max(6, int(std::lround(sample.fontPixelSize / scale)));
    style.textLeft = (sample.ink.left() - region.left()) / scale;
    style.textBaseline = (sample.baseline - region.top()) / scale;
    setStyle(style);
    return sample.touchesEdge ? Match::CutsText : Match::Ok;
}

QRectF ReplaceItem::textRect() const
{
    const QFontMetricsF metrics(font());
    const double scale = imageScale();
    // On the original text's baseline, or centred if none was found.
    const qreal centred = rect().center().y() + (metrics.ascent() - metrics.descent()) / 2;
    qreal baseline = m_style.textBaseline > 0 ? rect().top() + m_style.textBaseline * scale : centred;
    // Always inside the filled area: the letters must not stand on the
    // unchanged image (if the area is too low for them, centre them).
    const qreal minBaseline = rect().top() + metrics.ascent();
    const qreal maxBaseline = rect().bottom() - metrics.descent();
    baseline = minBaseline <= maxBaseline ? std::clamp(baseline, minBaseline, maxBaseline) : centred;
    const qreal left = std::clamp(m_style.textLeft * scale, 0.0, std::max(0.0, rect().width() - 2));
    return QRectF(rect().left() + left, baseline - metrics.ascent(),
                  metrics.horizontalAdvance(m_style.text) + 2, metrics.ascent() + metrics.descent());
}

QRectF ReplaceItem::boundingRect() const
{
    const qreal m = handleMargin();
    // The new text may be longer than the area; it is drawn anyway.
    return rect().adjusted(-m, -m, m, m).united(textRect());
}

QPainterPath ReplaceItem::shape() const
{
    QPainterPath path;
    path.addRect(rect());
    addHandlesToShape(path);
    return path;
}

void ReplaceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());

    // Remove the original: replace the pixels with the background colour.
    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(rect(), m_style.background);
    painter->restore();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->setFont(font());
    painter->setPen(m_style.color);
    const QRectF text = textRect();
    painter->drawText(QPointF(text.left(), text.top() + QFontMetricsF(font()).ascent()), m_style.text);
    painter->restore();

    // Outside the frames the dimmed background darkens this area too.
    if (editorScene && editorScene->dimSettings().enabled) {
        painter->save();
        painter->setClipPath(mapFromScene(editorScene->dimmedArea()));
        painter->fillRect(rect(), editorScene->dimVeil());
        painter->restore();
    }

    if (isSelected() && editorScene && !editorScene->isExporting()) {
        painter->setPen(QPen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5), Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect());
    }
    paintHandles(painter);
}
