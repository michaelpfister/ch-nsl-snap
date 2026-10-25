#include "app/PropertiesPanel.h"

#include "editor/ReplaceItem.h"
#include "ui/Theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QToolButton>
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

QPushButton *textButtonFor(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setProperty("role", QStringLiteral("secondary"));
    return button;
}

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
    // Frame properties (F-22–F-24).
    m_frameSection = new QWidget(this);
    auto *frameLayout = new QVBoxLayout(m_frameSection);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(8);
    frameLayout->addWidget(sectionLabel(tr("Farbe"), this));
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
    frameLayout->addLayout(swatchRow);
    frameLayout->addSpacing(12);

    // Corners (F-23): segmented control.
    frameLayout->addWidget(sectionLabel(tr("Ecken"), this));
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
    frameLayout->addWidget(segment);
    frameLayout->addSpacing(12);

    // Line width (F-23).
    m_width = labelledSlider(frameLayout, tr("Linienstärke"), 1, 8, 3, QStringLiteral("px"), this);
    connect(m_width, &QSlider::valueChanged, this, [this] {
        if (!m_updating)
            emit frameStyleChanged(frameStyle());
    });
    frameLayout->addSpacing(12);

    // Number badge position (F-24).
    frameLayout->addWidget(sectionLabel(tr("Nummer"), this));
    m_badge = new QComboBox(this);
    m_badge->addItem(tr("Oben rechts"), int(BadgePosition::TopRight));
    m_badge->addItem(tr("Oben links"), int(BadgePosition::TopLeft));
    m_badge->addItem(tr("Unten rechts"), int(BadgePosition::BottomRight));
    m_badge->addItem(tr("Unten links"), int(BadgePosition::BottomLeft));
    m_badge->addItem(tr("Ohne Nummer"), int(BadgePosition::None));
    m_badge->setAccessibleName(tr("Nummer"));
    m_badge->setFocusPolicy(Qt::NoFocus);
    connect(m_badge, &QComboBox::activated, this, [this] { emit frameStyleChanged(frameStyle()); });
    frameLayout->addWidget(m_badge);
    layout->addWidget(m_frameSection);

    // Pixelate / blur properties (F-26).
    m_redactSection = new QWidget(this);
    auto *redactLayout = new QVBoxLayout(m_redactSection);
    redactLayout->setContentsMargins(0, 0, 0, 0);
    redactLayout->setSpacing(8);
    const auto segmentedRow = [this](QVBoxLayout *target, const QString &label, const QStringList &texts,
                                     QButtonGroup *group) {
        target->addWidget(sectionLabel(label, this));
        auto *frame = new QFrame(this);
        frame->setProperty("role", QStringLiteral("segment"));
        auto *row = new QHBoxLayout(frame);
        row->setContentsMargins(3, 3, 3, 3);
        row->setSpacing(2);
        for (int i = 0; i < texts.size(); ++i) {
            auto *button = new QPushButton(texts[i], frame);
            button->setCheckable(true);
            group->addButton(button, i);
            row->addWidget(button);
        }
        target->addWidget(frame);
    };
    m_redactMode = new QButtonGroup(this);
    m_redactLevel = new QButtonGroup(this);
    segmentedRow(redactLayout, tr("Art"), {tr("Verpixeln"), tr("Weichzeichnen")}, m_redactMode);
    redactLayout->addSpacing(12);
    segmentedRow(redactLayout, tr("Stärke"), {tr("Leicht"), tr("Mittel"), tr("Stark")}, m_redactLevel);
    auto *redactHint = new QLabel(tr("Für Namen und Codes «Mittel» oder «Stark» wählen: "
                                     "leicht verpixelter Text kann lesbar bleiben."), this);
    redactHint->setProperty("role", QStringLiteral("hint"));
    redactHint->setWordWrap(true);
    redactLayout->addWidget(redactHint);
    connect(m_redactMode, &QButtonGroup::idClicked, this, [this] { emit redactStyleChanged(redactStyle()); });
    connect(m_redactLevel, &QButtonGroup::idClicked, this, [this] { emit redactStyleChanged(redactStyle()); });
    setRedactStyle(RedactStyle());
    m_redactSection->hide();
    layout->addWidget(m_redactSection);

    // Replaced text (F-29).
    m_replaceSection = new QWidget(this);
    auto *replaceLayout = new QVBoxLayout(m_replaceSection);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(8);
    replaceLayout->addWidget(sectionLabel(tr("Neuer Text"), this));
    m_replaceText = new QLineEdit(this);
    m_replaceText->setPlaceholderText(tr("z. B. Firstname Lastname"));
    m_replaceText->setAccessibleName(tr("Neuer Text"));
    replaceLayout->addWidget(m_replaceText);
    replaceLayout->addSpacing(8);

    replaceLayout->addWidget(sectionLabel(tr("Schrift"), this));
    m_replaceFont = new QFontComboBox(this);
    m_replaceFont->setAccessibleName(tr("Schrift"));
    replaceLayout->addWidget(m_replaceFont);
    auto *sizeRow = new QHBoxLayout;
    m_replaceSize = new QSpinBox(this);
    m_replaceSize->setRange(6, 96);
    m_replaceSize->setSuffix(QStringLiteral(" px"));
    m_replaceSize->setAccessibleName(tr("Schriftgrösse"));
    m_replaceBold = new QCheckBox(tr("Fett"), this);
    sizeRow->addWidget(m_replaceSize, 1);
    sizeRow->addWidget(m_replaceBold);
    replaceLayout->addLayout(sizeRow);
    replaceLayout->addSpacing(8);

    // Colours: a swatch button each, opening the colour dialog.
    const auto colorButton = [this](const QString &name) {
        auto *button = new QToolButton(this);
        button->setText(name);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(18, 18));
        button->setAccessibleName(name);
        button->setProperty("role", QStringLiteral("icon"));
        return button;
    };
    auto *colorRow = new QHBoxLayout;
    m_replaceColor = colorButton(tr("Text"));
    m_replaceBackground = colorButton(tr("Hintergrund"));
    colorRow->addWidget(m_replaceColor);
    colorRow->addWidget(m_replaceBackground);
    colorRow->addStretch(1);
    replaceLayout->addWidget(sectionLabel(tr("Farben"), this));
    replaceLayout->addLayout(colorRow);

    auto *rematch = textButtonFor(tr("Original neu erkennen"), this);
    rematch->setToolTip(tr("Hintergrund, Textfarbe, Grösse und Position wieder aus dem Bild übernehmen"));
    replaceLayout->addWidget(rematch);
    auto *replaceHint = new QLabel(tr("Die Fläche wird mit der Hintergrundfarbe gefüllt; "
                                      "das Original ist danach nicht mehr im Bild."), this);
    replaceHint->setProperty("role", QStringLiteral("hint"));
    replaceHint->setWordWrap(true);
    replaceLayout->addWidget(replaceHint);
    m_replaceSection->hide();
    layout->addWidget(m_replaceSection);

    const auto emitReplace = [this] {
        if (!m_updating)
            emit replaceStyleChanged(replaceStyle());
    };
    connect(m_replaceText, &QLineEdit::textEdited, this, emitReplace);
    connect(m_replaceFont, &QFontComboBox::currentFontChanged, this, emitReplace);
    connect(m_replaceSize, &QSpinBox::valueChanged, this, emitReplace);
    connect(m_replaceBold, &QCheckBox::toggled, this, emitReplace);
    const auto pickColor = [this, emitReplace](QColor *target, const QString &title) {
        const QColor color = QColorDialog::getColor(*target, this, title);
        if (!color.isValid())
            return;
        *target = color;
        updateColorButtons();
        emitReplace();
    };
    connect(m_replaceColor, &QToolButton::clicked, this, [this, pickColor] { pickColor(&m_replaceTextColor, tr("Textfarbe")); });
    connect(m_replaceBackground, &QToolButton::clicked, this,
            [this, pickColor] { pickColor(&m_replaceBackgroundColor, tr("Hintergrundfarbe")); });
    connect(rematch, &QPushButton::clicked, this, &PropertiesPanel::rematchRequested);

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

RedactStyle PropertiesPanel::redactStyle() const
{
    RedactStyle style;
    style.mode = RedactMode(std::max(0, m_redactMode->checkedId()));
    style.level = RedactLevel(std::max(0, m_redactLevel->checkedId()));
    return style;
}

void PropertiesPanel::setRedactStyle(const RedactStyle &style)
{
    m_redactMode->button(int(style.mode))->setChecked(true);
    m_redactLevel->button(int(style.level))->setChecked(true);
}

void PropertiesPanel::showSections(bool frame, bool redact, bool replace)
{
    m_frameSection->setVisible(frame);
    m_redactSection->setVisible(redact);
    m_replaceSection->setVisible(replace);
}

void PropertiesPanel::showReplaceSelection()
{
    m_title->setText(tr("Text ersetzen"));
    m_hint->setText(tr("1 ausgewählt"));
}

ReplaceStyle PropertiesPanel::replaceStyle() const
{
    ReplaceStyle style = m_replaceBase;
    style.text = m_replaceText->text();
    style.fontFamily = m_replaceFont->currentFont().family();
    style.fontSize = m_replaceSize->value();
    style.bold = m_replaceBold->isChecked();
    style.color = m_replaceTextColor;
    style.background = m_replaceBackgroundColor;
    return style;
}

void PropertiesPanel::setReplaceStyle(const ReplaceStyle &style)
{
    m_updating = true;
    m_replaceBase = style; // keeps the position fields the panel does not show
    // Only when it differs, so typing keeps its cursor position.
    if (m_replaceText->text() != style.text)
        m_replaceText->setText(style.text);
    m_replaceFont->setCurrentFont(QFont(style.fontFamily.isEmpty() ? ReplaceItem::defaultFontFamily() : style.fontFamily));
    m_replaceSize->setValue(style.fontSize);
    m_replaceBold->setChecked(style.bold);
    m_replaceTextColor = style.color;
    m_replaceBackgroundColor = style.background;
    updateColorButtons();
    m_updating = false;
}

void PropertiesPanel::focusReplaceText()
{
    m_replaceText->setFocus();
    m_replaceText->selectAll();
}

void PropertiesPanel::updateColorButtons()
{
    const auto swatch = [](const QColor &color) {
        QPixmap pixmap(18, 18);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QColor(0, 0, 0, 60));
        p.setBrush(color);
        p.drawRoundedRect(QRectF(0.5, 0.5, 17, 17), 4, 4);
        return QIcon(pixmap);
    };
    m_replaceColor->setIcon(swatch(m_replaceTextColor));
    m_replaceBackground->setIcon(swatch(m_replaceBackgroundColor));
}

void PropertiesPanel::showRedactSelection(int count)
{
    m_title->setText(tr("Verpixeln"));
    m_hint->setText(count == 1 ? tr("1 ausgewählt") : tr("%1 ausgewählt").arg(count));
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
