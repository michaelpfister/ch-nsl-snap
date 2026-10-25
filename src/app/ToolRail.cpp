#include "app/ToolRail.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAction>
#include <QButtonGroup>
#include <QFrame>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton *railButton(QWidget *parent, const QString &tooltip, QSize size = QSize(44, 44))
{
    auto *button = new QToolButton(parent);
    button->setCheckable(true);
    button->setFixedSize(size);
    button->setIconSize(size.height() >= 40 ? QSize(20, 20) : QSize(18, 18));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

struct BlurLevel { int blur; const char *icon; const char *name; };
const BlurLevel kBlurLevels[] = {
    {DimSettings::kBlurNone, "blur-none", QT_TRANSLATE_NOOP("ToolRail", "Keine Unschärfe")},
    {DimSettings::kBlurLight, "blur-light", QT_TRANSLATE_NOOP("ToolRail", "Leichte Unschärfe (1 px)")},
    {DimSettings::kBlurStrong, "blur-strong", QT_TRANSLATE_NOOP("ToolRail", "Starke Unschärfe (3 px)")},
};

} // namespace

ToolRail::ToolRail(QWidget *parent)
    : QWidget(parent)
    , m_group(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("ToolRail"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(60);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(4);

    for (const ToolInfo &tool : allTools()) {
        if (tool.separatorBefore) {
            auto *line = new QFrame(this);
            line->setProperty("role", QStringLiteral("separator"));
            line->setFixedWidth(28);
            layout->addWidget(line, 0, Qt::AlignHCenter);
        }
        const QString tip = QStringLiteral("%1 (%2)").arg(tool.name, tool.shortcut.toString(QKeySequence::NativeText));
        QToolButton *button = railButton(this, tip);
        m_group->addButton(button);
        m_buttons.insert(tool.id, button);
        layout->addWidget(button, 0, Qt::AlignHCenter);

        // Single-key shortcut, active while the main window has focus.
        auto *action = new QAction(this);
        action->setShortcut(tool.shortcut);
        action->setShortcutContext(Qt::WindowShortcut);
        connect(action, &QAction::triggered, this, [this, id = tool.id] { setCurrentTool(id); });
        addAction(action);

        connect(button, &QToolButton::clicked, this, [this, id = tool.id] { setCurrentTool(id); });
    }

    layout->addStretch(1);

    // While dimmed: the three blur levels, above the dim switch, so the
    // switch itself never moves and can be toggled without moving the mouse.
    m_blurButtons = new QWidget(this);
    auto *blurLayout = new QVBoxLayout(m_blurButtons);
    blurLayout->setContentsMargins(0, 0, 0, 2);
    blurLayout->setSpacing(2);
    auto *blurGroup = new QButtonGroup(this);
    // Strongest at the top, "none" right above the switch.
    for (const BlurLevel &level : {kBlurLevels[2], kBlurLevels[1], kBlurLevels[0]}) {
        QToolButton *button = railButton(m_blurButtons, tr(level.name), QSize(44, 28));
        button->setProperty("blurIcon", QString::fromLatin1(level.icon));
        blurGroup->addButton(button);
        m_blurButton.insert(level.blur, button);
        blurLayout->addWidget(button, 0, Qt::AlignHCenter);
        connect(button, &QToolButton::clicked, this, [this, blur = level.blur] { emit blurChosen(blur); });
    }
    m_blurButtons->hide();
    layout->addWidget(m_blurButtons, 0, Qt::AlignHCenter);

    // Dim background (F-27): one switch, always 45 % without blur. Last in
    // the rail, below the stretch, so its position is fixed.
    m_dimButton = railButton(this, tr("Hintergrund abdunkeln"));
    layout->addWidget(m_dimButton, 0, Qt::AlignHCenter);
    connect(m_dimButton, &QToolButton::clicked, this, &ToolRail::dimToggled);

    setCurrentTool(ToolId::Select);
}

void ToolRail::applyTheme(const Theme &theme)
{
    for (const ToolInfo &tool : allTools())
        m_buttons.value(tool.id)->setIcon(Icons::toggleIcon(tool.icon, theme.muted, theme.softText));
    m_dimButton->setIcon(Icons::toggleIcon(QStringLiteral("dim"), theme.muted, theme.softText));
    for (QToolButton *button : std::as_const(m_blurButton))
        button->setIcon(Icons::toggleIcon(button->property("blurIcon").toString(), theme.muted, theme.softText));
}

void ToolRail::setCurrentTool(ToolId id)
{
    m_buttons.value(id)->setChecked(true);
    if (id == m_current)
        return;
    m_current = id;
    emit toolChanged(id);
}

void ToolRail::showDim(const DimSettings &settings)
{
    m_dimButton->setChecked(settings.enabled);
    m_blurButtons->setVisible(settings.enabled);
    // Values set with the advanced sliders match none of the three buttons,
    // so the group is briefly non-exclusive to allow "none checked".
    QToolButton *match = m_blurButton.value(settings.blur);
    QButtonGroup *group = m_blurButton.first()->group();
    group->setExclusive(false);
    for (QToolButton *button : std::as_const(m_blurButton))
        button->setChecked(button == match);
    group->setExclusive(true);
}
