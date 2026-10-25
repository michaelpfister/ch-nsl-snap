#include "app/PropertiesPanel.h"

#include "ui/Theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QDropEvent>
#include <QListView>
#include <QListWidget>

#include <algorithm>
#include <functional>
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

// The object list: entries can be dragged to change the order. The list
// does not move the entry itself; it reports the move and is rebuilt from
// the scene afterwards, so undo and the list never disagree.
class ObjectList : public QListWidget
{
public:
    std::function<void(int from, int to)> onMove;

    explicit ObjectList(QWidget *parent)
        : QListWidget(parent)
    {
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

protected:
    void dropEvent(QDropEvent *event) override
    {
        const int from = currentRow();
        const QModelIndex target = indexAt(event->position().toPoint());
        int to = count() - 1;
        if (target.isValid()) {
            to = target.row();
            if (dropIndicatorPosition() == QAbstractItemView::BelowItem)
                ++to;
            if (dropIndicatorPosition() != QAbstractItemView::OnItem && from < to)
                --to; // the entry leaves its old place first
        }
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        if (from >= 0 && from != to && onMove)
            onMove(from, std::clamp(to, 0, count() - 1));
    }
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
    const auto showValue = [valueLabel, unit](int v) { valueLabel->setText(QStringLiteral("%1 %2").arg(v).arg(unit)); };
    QObject::connect(slider, &QSlider::valueChanged, valueLabel, showValue);
    QObject::connect(slider, &QSlider::sliderMoved, valueLabel, showValue); // also without tracking
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
        button->setProperty("color", swatch.color);
        connect(button, &QAbstractButton::clicked, this, [this, color = swatch.color] {
            m_color = color;
            emit frameStyleChanged(frameStyle());
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
    connect(corners, &QButtonGroup::buttonClicked, this, [this] { emit frameStyleChanged(frameStyle()); });
    layout->addWidget(segment);
    layout->addSpacing(12);

    // Line width (F-23).
    m_width = labelledSlider(layout, tr("Linienstärke"), 1, 8, 3, QStringLiteral("px"), this);
    connect(m_width, &QSlider::valueChanged, this, [this] {
        if (!m_updating)
            emit frameStyleChanged(frameStyle());
    });
    layout->addSpacing(12);

    // Number badge position (F-24).
    layout->addWidget(sectionLabel(tr("Nummer"), this));
    m_badge = new QComboBox(this);
    m_badge->addItem(tr("Oben rechts"), int(BadgePosition::TopRight));
    m_badge->addItem(tr("Oben links"), int(BadgePosition::TopLeft));
    m_badge->addItem(tr("Unten rechts"), int(BadgePosition::BottomRight));
    m_badge->addItem(tr("Unten links"), int(BadgePosition::BottomLeft));
    m_badge->addItem(tr("Ohne Nummer"), int(BadgePosition::None));
    m_badge->setAccessibleName(tr("Nummer"));
    m_badge->setFocusPolicy(Qt::NoFocus);
    connect(m_badge, &QComboBox::activated, this, [this] { emit frameStyleChanged(frameStyle()); });
    layout->addWidget(m_badge);

    layout->addSpacing(16);
    layout->addWidget(separator(this));
    layout->addSpacing(16);

    // Dim background (F-27): fine-tuning only in the advanced mode; the
    // normal way is the switch and the blur buttons in the tool rail.
    m_dimSection = new QWidget(this);
    auto *dimLayout = new QVBoxLayout(m_dimSection);
    dimLayout->setContentsMargins(0, 0, 0, 0);
    dimLayout->setSpacing(8);
    // Label left, checkbox right, as in the design.
    auto *dimRow = new QHBoxLayout;
    auto *dimLabel = new QLabel(tr("Hintergrund abdunkeln"), m_dimSection);
    QFont bold = dimLabel->font();
    bold.setWeight(QFont::DemiBold);
    dimLabel->setFont(bold);
    m_dim = new QCheckBox(m_dimSection);
    m_dim->setAccessibleName(dimLabel->text());
    dimLabel->setBuddy(m_dim);
    dimRow->addWidget(dimLabel);
    dimRow->addStretch(1);
    dimRow->addWidget(m_dim);
    dimLayout->addLayout(dimRow);
    const DimSettings defaults;
    m_dimStrength = labelledSlider(dimLayout, tr("Stärke"), 0, 80, defaults.strength, QStringLiteral("%"), m_dimSection);
    m_dimBlur = labelledSlider(dimLayout, tr("Unschärfe"), 0, 10, defaults.blur, QStringLiteral("px"), m_dimSection);
    // Apply on release, not on every slider step: a new blur radius takes
    // a moment to compute.
    m_dimStrength->setTracking(false);
    m_dimBlur->setTracking(false);
    const auto emitDim = [this] {
        if (!m_updating)
            emit dimSettingsChanged(dimSettings());
    };
    connect(m_dim, &QCheckBox::toggled, this, emitDim);
    connect(m_dimStrength, &QSlider::valueChanged, this, emitDim);
    connect(m_dimBlur, &QSlider::valueChanged, this, emitDim);
    dimLayout->addSpacing(16);
    dimLayout->addWidget(separator(m_dimSection));
    dimLayout->addSpacing(8);
    m_dimSection->hide();
    layout->addWidget(m_dimSection);

    // Objects (D-012).
    layout->addWidget(sectionLabel(tr("Objekte"), this));
    auto *objects = new ObjectList(this);
    objects->onMove = [this](int from, int to) { emit objectMoved(from, to); };
    m_objects = objects;
    m_objects->setAccessibleName(tr("Objekte"));
    m_objects->setToolTip(tr("Ziehen, um die Reihenfolge der Nummern zu ändern"));
    m_objects->setFocusPolicy(Qt::NoFocus);
    connect(m_objects, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item->flags() != Qt::NoItemFlags)
            emit objectClicked(m_objects->row(item));
    });
    layout->addWidget(m_objects, 1);
    setObjects({}, {});

    showTool(ToolId::Select);
}

DimSettings PropertiesPanel::dimSettings() const
{
    DimSettings settings;
    settings.enabled = m_dim->isChecked();
    settings.strength = m_dimStrength->value();
    settings.blur = m_dimBlur->value();
    return settings;
}

void PropertiesPanel::setAdvancedMode(bool enabled)
{
    m_dimSection->setVisible(enabled);
}

void PropertiesPanel::setDimSettings(const DimSettings &settings)
{
    m_updating = true;
    m_dim->setChecked(settings.enabled);
    m_dimStrength->setValue(settings.strength);
    m_dimBlur->setValue(settings.blur);
    m_updating = false;
}

FrameStyle PropertiesPanel::frameStyle() const
{
    FrameStyle style;
    style.color = m_color;
    style.width = m_width->value();
    style.rounded = m_rounded->isChecked();
    style.badge = BadgePosition(m_badge->currentData().toInt());
    return style;
}

void PropertiesPanel::setFrameStyle(const FrameStyle &style)
{
    m_updating = true;
    m_color = style.color;
    for (QAbstractButton *button : m_swatches->buttons()) {
        if (button->property("color").value<QColor>() == style.color)
            button->setChecked(true);
    }
    m_width->setValue(style.width);
    (style.rounded ? m_rounded : m_square)->setChecked(true);
    m_badge->setCurrentIndex(m_badge->findData(int(style.badge)));
    m_updating = false;
}

void PropertiesPanel::setBadgeOptions(const QList<BadgePosition> &positions)
{
    auto *list = static_cast<QListView *>(m_badge->view());
    for (int row = 0; row < m_badge->count(); ++row) {
        const auto position = BadgePosition(m_badge->itemData(row).toInt());
        list->setRowHidden(row, position != BadgePosition::None && !positions.contains(position));
    }
}

void PropertiesPanel::setObjects(const QStringList &labels, const QList<bool> &selected)
{
    m_objects->clear();
    if (labels.isEmpty()) {
        auto *empty = new QListWidgetItem(tr("Noch keine Objekte"), m_objects);
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(m_mutedText);
        return;
    }
    for (qsizetype i = 0; i < labels.size(); ++i) {
        auto *item = new QListWidgetItem(labels[i], m_objects);
        // Selection is shown with the font, the list itself does not select.
        QFont font = item->font();
        font.setWeight(selected.value(i) ? QFont::DemiBold : QFont::Normal);
        item->setFont(font);
    }
}

void PropertiesPanel::applyTheme(const Theme &theme)
{
    m_mutedText = theme.muted;
    if (QListWidgetItem *item = m_objects->item(0); item && item->flags() == Qt::NoItemFlags)
        item->setForeground(theme.muted);
}

void PropertiesPanel::showTool(ToolId id)
{
    const ToolInfo &tool = toolInfo(id);
    m_title->setText(tool.name);
    m_hint->setText(id == ToolId::Select ? tr("nichts ausgewählt") : tr("für neue Objekte"));
}

void PropertiesPanel::showSelection(int count)
{
    m_title->setText(tr("Rahmen"));  // numbered or not; the "Nummer" field tells
    m_hint->setText(count == 1 ? tr("1 ausgewählt") : tr("%1 ausgewählt").arg(count));
}
