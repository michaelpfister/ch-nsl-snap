#include "app/ToolRail.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAction>
#include <QButtonGroup>
#include <QFrame>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton *railButton(QWidget *parent, const QString &tooltip)
{
    auto *button = new QToolButton(parent);
    button->setCheckable(true);
    button->setFixedSize(44, 44);
    button->setIconSize(QSize(20, 20));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

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
    m_dimButton = railButton(this, tr("Hintergrund abdunkeln"));
    layout->addWidget(m_dimButton, 0, Qt::AlignHCenter);

    setCurrentTool(ToolId::Select);
}

void ToolRail::applyTheme(const Theme &theme)
{
    for (const ToolInfo &tool : allTools())
        m_buttons.value(tool.id)->setIcon(Icons::toggleIcon(tool.icon, theme.muted, theme.softText));
    m_dimButton->setIcon(Icons::toggleIcon(QStringLiteral("dim"), theme.muted, theme.softText));
}

void ToolRail::setCurrentTool(ToolId id)
{
    m_buttons.value(id)->setChecked(true);
    if (id == m_current)
        return;
    m_current = id;
    emit toolChanged(id);
}
