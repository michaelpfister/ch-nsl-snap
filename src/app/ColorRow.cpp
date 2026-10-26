#include "app/ColorRow.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QConicalGradient>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QPainter>

#include <iterator>

namespace {

// Standard colours; red first, as in the instruction pages.
struct Swatch { QColor color; const char *name; };
const Swatch kSwatches[] = {
    {QColor(0xE5484D), QT_TRANSLATE_NOOP("ColorRow", "Rot")},
    {QColor(0xF76B15), QT_TRANSLATE_NOOP("ColorRow", "Orange")},
    {QColor(0xFFC53D), QT_TRANSLATE_NOOP("ColorRow", "Gelb")},
    {QColor(0x30A46C), QT_TRANSLATE_NOOP("ColorRow", "Grün")},
    {QColor(0x0090FF), QT_TRANSLATE_NOOP("ColorRow", "Blau")},
    {QColor(0x8E4EC6), QT_TRANSLATE_NOOP("ColorRow", "Violett")},
    {QColor(0x1F2328), QT_TRANSLATE_NOOP("ColorRow", "Schwarz")},
    {QColor(0xFFFFFF), QT_TRANSLATE_NOOP("ColorRow", "Weiss")},
};
// Felt-pen colours for the text marker: light, so text stays readable
// when multiplied with them (F-28).
const Swatch kMarkerSwatches[] = {
    {QColor(0xFFE629), QT_TRANSLATE_NOOP("ColorRow", "Gelb")},
    {QColor(0xFFB86B), QT_TRANSLATE_NOOP("ColorRow", "Orange")},
    {QColor(0x9BE9A8), QT_TRANSLATE_NOOP("ColorRow", "Grün")},
    {QColor(0x7DE3F4), QT_TRANSLATE_NOOP("ColorRow", "Cyan")},
    {QColor(0xFFA3D7), QT_TRANSLATE_NOOP("ColorRow", "Pink")},
    {QColor(0xC9B3FF), QT_TRANSLATE_NOOP("ColorRow", "Violett")},
};

constexpr int kButtonSize = 24;

// A round colour button; the checked one gets a ring in its own colour
// (grey for very light colours, which would not show on the panel).
// Without a colour it is the custom button: a rainbow ring.
class SwatchButton : public QAbstractButton
{
public:
    SwatchButton(const QColor &color, QWidget *parent)
        : QAbstractButton(parent), m_color(color)
    {
        setCheckable(true);
        setFixedSize(kButtonSize, kButtonSize);
        setCursor(Qt::PointingHandCursor);
    }

    // For the custom button: the colour shown inside the rainbow ring.
    void setInnerColor(const QColor &color) { m_inner = color; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        const QRectF dot = QRectF(rect()).adjusted(4, 4, -4, -4);
        if (!m_color.isValid()) {
            // Rainbow ring, with the own colour inside once there is one.
            QConicalGradient rainbow(r.center(), 90);
            for (int i = 0; i <= 6; ++i)
                rainbow.setColorAt(i / 6.0, QColor::fromHsvF(float(i % 6) / 6.0f, 0.85f, 0.95f));
            p.setPen(QPen(QBrush(rainbow), isChecked() ? 3.0 : 2.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(r.adjusted(1, 1, -1, -1));
            if (m_inner.isValid()) {
                p.setPen(QColor(0, 0, 0, 50));
                p.setBrush(m_inner);
                p.drawEllipse(dot.adjusted(1.5, 1.5, -1.5, -1.5));
            } else {
                // A small plus: "add a colour".
                p.setPen(QPen(palette().color(QPalette::WindowText), 1.5, Qt::SolidLine, Qt::RoundCap));
                const QPointF c = r.center();
                p.drawLine(c - QPointF(4, 0), c + QPointF(4, 0));
                p.drawLine(c - QPointF(0, 4), c + QPointF(0, 4));
            }
            return;
        }
        const bool light = m_color.lightnessF() > 0.9;
        if (isChecked()) {
            p.setPen(QPen(light ? QColor(0x8B, 0x8D, 0x98) : m_color, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(r);
        }
        p.setPen(light ? QPen(QColor(0, 0, 0, 60), 1) : Qt::NoPen);
        p.setBrush(m_color);
        p.drawEllipse(dot);
    }

private:
    QColor m_color;
    QColor m_inner;
};

} // namespace

ColorRow::ColorRow(Palette palette, QWidget *parent)
    : QWidget(parent)
    , m_buttons(new QButtonGroup(this))
{
    const QList<Swatch> swatches = palette == Palette::Marker
                                       ? QList<Swatch>(std::begin(kMarkerSwatches), std::end(kMarkerSwatches))
                                       : QList<Swatch>(std::begin(kSwatches), std::end(kSwatches));
    m_color = swatches.first().color;
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    for (const Swatch &swatch : swatches) {
        auto *button = new SwatchButton(swatch.color, this);
        const QString name = QCoreApplication::translate("ColorRow", swatch.name);
        button->setToolTip(name);
        button->setAccessibleName(name);
        button->setProperty("color", swatch.color);
        m_buttons->addButton(button);
        layout->addWidget(button);
        connect(button, &QAbstractButton::clicked, this, [this, color = swatch.color] {
            setColor(color);
            emit colorChosen(color);
        });
    }
    auto *custom = new SwatchButton(QColor(), this);
    custom->setToolTip(tr("Eigene Farbe: Farbrad, Hex-Code oder Pipette"));
    custom->setAccessibleName(tr("Eigene Farbe"));
    m_buttons->addButton(custom);
    layout->addWidget(custom);
    m_custom = custom;
    // The custom button opens the popup; its checked state shows whether
    // the current colour is an own one, so a click does not change it.
    connect(custom, &QAbstractButton::clicked, this, [this] {
        setColor(m_color);
        emit customRequested();
    });
    layout->addStretch(1);
    setColor(m_color);
}

bool ColorRow::isStandard(const QColor &color)
{
    for (const Swatch &swatch : kSwatches) {
        if (swatch.color == color)
            return true;
    }
    for (const Swatch &swatch : kMarkerSwatches) {
        if (swatch.color == color)
            return true;
    }
    return false;
}

QWidget *ColorRow::customButton() const
{
    return m_custom;
}

void ColorRow::setColor(const QColor &color)
{
    m_color = color;
    QAbstractButton *match = m_custom;
    for (QAbstractButton *button : m_buttons->buttons()) {
        if (button != m_custom && button->property("color").value<QColor>() == color)
            match = button;
    }
    match->setChecked(true);
    static_cast<SwatchButton *>(m_custom)->setInnerColor(match == m_custom ? color : QColor());
}
