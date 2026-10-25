#include "app/PropertiesPanel.h"

#include "ui/Theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {

// Frame colours (F-22); red first, as in the instruction pages.
struct Swatch { QColor color; const char *name; };
const Swatch kSwatches[] = {
    {QColor(0xE5484D), QT_TRANSLATE_NOOP("PropertiesPanel", "Rot")},
    {QColor(0xF76B15), QT_TRANSLATE_NOOP("PropertiesPanel", "Orange")},
    {QColor(0xFFC53D), QT_TRANSLATE_NOOP("PropertiesPanel", "Gelb")},
    {QColor(0x30A46C), QT_TRANSLATE_NOOP("PropertiesPanel", "Grün")},
    {QColor(0x0090FF), QT_TRANSLATE_NOOP("PropertiesPanel", "Blau")},
    {QColor(0x8E4EC6), QT_TRANSLATE_NOOP("PropertiesPanel", "Violett")},
};

// A round colour button; the checked one gets a ring in its own colour.
class SwatchButton : public QAbstractButton
{
public:
    SwatchButton(const QColor &color, QWidget *parent)
        : QAbstractButton(parent), m_color(color)
    {
        setCheckable(true);
        setFixedSize(30, 30);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (isChecked()) {
            p.setPen(QPen(m_color, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRectF(rect()).adjusted(1, 1, -1, -1));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawEllipse(QRectF(rect()).adjusted(4, 4, -4, -4));
    }

private:
    QColor m_color;
};

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("section"));
    return label;
}

QFrame *separator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setProperty("role", QStringLiteral("separator"));
    return line;
}

// A slider with its label and current value above it.
QSlider *labelledSlider(QVBoxLayout *layout, const QString &label, int min, int max, int value,
                        const QString &unit, QWidget *parent)
{
    auto *row = new QHBoxLayout;
    row->addWidget(sectionLabel(label, parent));
    row->addStretch(1);
    auto *valueLabel = sectionLabel(QStringLiteral("%1 %2").arg(value).arg(unit), parent);
    row->addWidget(valueLabel);
    layout->addLayout(row);

    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(min, max);
    slider->setValue(value);
    slider->setAccessibleName(label);
    QObject::connect(slider, &QSlider::valueChanged, valueLabel, [valueLabel, unit](int v) {
        valueLabel->setText(QStringLiteral("%1 %2").arg(v).arg(unit));
    });
    layout->addWidget(slider);
    return slider;
}

} // namespace

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
    , m_swatches(new QButtonGroup(this))
    , m_color(kSwatches[0].color)
{
    setObjectName(QStringLiteral("PropertiesPanel"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(288);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    // Title and hint.
    auto *header = new QHBoxLayout;
    m_title = new QLabel(this);
    m_title->setProperty("role", QStringLiteral("panelTitle"));
    m_hint = new QLabel(this);
    m_hint->setProperty("role", QStringLiteral("hint"));
    header->addWidget(m_title);
    header->addStretch(1);
    header->addWidget(m_hint);
    layout->addLayout(header);
    layout->addSpacing(12);

    // Colour (F-22).
    layout->addWidget(sectionLabel(tr("Farbe"), this));
    auto *swatchRow = new QHBoxLayout;
    swatchRow->setSpacing(6);
    for (const Swatch &swatch : kSwatches) {
        auto *button = new SwatchButton(swatch.color, this);
        button->setToolTip(tr(swatch.name));
        button->setAccessibleName(tr(swatch.name));
        m_swatches->addButton(button);
        swatchRow->addWidget(button);
        connect(button, &QAbstractButton::clicked, this, [this, color = swatch.color] {
            m_color = color;
            emit frameColorChanged(color);
        });
    }
    m_swatches->buttons().first()->setChecked(true);
    swatchRow->addStretch(1);
    layout->addLayout(swatchRow);
    layout->addSpacing(12);

    // Corners (F-23): segmented control.
    layout->addWidget(sectionLabel(tr("Ecken"), this));
    auto *segment = new QFrame(this);
    segment->setProperty("role", QStringLiteral("segment"));
    auto *segmentLayout = new QHBoxLayout(segment);
    segmentLayout->setContentsMargins(3, 3, 3, 3);
    segmentLayout->setSpacing(2);
    m_square = new QPushButton(tr("Eckig"), segment);
    m_rounded = new QPushButton(tr("Abgerundet"), segment);
    auto *corners = new QButtonGroup(this);
    for (QPushButton *button : {m_square, m_rounded}) {
        button->setCheckable(true);
        corners->addButton(button);
        segmentLayout->addWidget(button);
    }
    m_rounded->setChecked(true);
    layout->addWidget(segment);
    layout->addSpacing(12);

    // Line width (F-23).
    m_width = labelledSlider(layout, tr("Linienstärke"), 1, 8, 3, QStringLiteral("px"), this);
    layout->addSpacing(12);

    // Number badge position (F-24).
    layout->addWidget(sectionLabel(tr("Nummer"), this));
    auto *badge = new QComboBox(this);
    badge->addItems({tr("Oben rechts"), tr("Oben links"), tr("Unten rechts"), tr("Unten links"), tr("Ohne Nummer")});
    badge->setAccessibleName(tr("Nummer"));
    layout->addWidget(badge);

    layout->addSpacing(16);
    layout->addWidget(separator(this));
    layout->addSpacing(16);

    // Dim background (F-27).
    // Label left, checkbox right, as in the design.
    auto *dimRow = new QHBoxLayout;
    auto *dimLabel = new QLabel(tr("Hintergrund abdunkeln"), this);
    QFont bold = dimLabel->font();
    bold.setWeight(QFont::DemiBold);
    dimLabel->setFont(bold);
    m_dim = new QCheckBox(this);
    m_dim->setAccessibleName(dimLabel->text());
    dimLabel->setBuddy(m_dim);
    dimRow->addWidget(dimLabel);
    dimRow->addStretch(1);
    dimRow->addWidget(m_dim);
    layout->addLayout(dimRow);
    labelledSlider(layout, tr("Stärke"), 0, 80, 45, QStringLiteral("%"), this);
    labelledSlider(layout, tr("Unschärfe"), 0, 10, 2, QStringLiteral("px"), this);

    layout->addSpacing(16);
    layout->addWidget(separator(this));
    layout->addSpacing(16);

    // Objects (D-012).
    layout->addWidget(sectionLabel(tr("Objekte"), this));
    m_objects = new QListWidget(this);
    m_objects->setAccessibleName(tr("Objekte"));
    auto *empty = new QListWidgetItem(tr("Noch keine Objekte"), m_objects);
    empty->setFlags(Qt::NoItemFlags);
    layout->addWidget(m_objects, 1);

    showTool(ToolId::Select);
}

bool PropertiesPanel::roundedCorners() const
{
    return m_rounded->isChecked();
}

void PropertiesPanel::applyTheme(const Theme &theme)
{
    if (QListWidgetItem *item = m_objects->item(0); item && item->flags() == Qt::NoItemFlags)
        item->setForeground(theme.muted);
}

void PropertiesPanel::showTool(ToolId id)
{
    const ToolInfo &tool = toolInfo(id);
    m_title->setText(tool.name);
    m_hint->setText(id == ToolId::Select ? tr("nichts ausgewählt") : tr("neues Objekt"));
}
