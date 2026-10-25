#pragma once

#include "editor/Tool.h"

#include <QMap>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QToolButton;
struct Theme;

// The vertical tool bar on the left (design mockup, F-20 ff.).
class ToolRail : public QWidget
{
    Q_OBJECT

public:
    explicit ToolRail(QWidget *parent = nullptr);

    void applyTheme(const Theme &theme);
    void setCurrentTool(ToolId id);
    ToolId currentTool() const { return m_current; }

    // The "dim background" toggle at the bottom (F-27).
    QToolButton *dimButton() const { return m_dimButton; }

signals:
    void toolChanged(ToolId id);

private:
    QButtonGroup *m_group = nullptr;
    QMap<ToolId, QToolButton *> m_buttons;
    QToolButton *m_dimButton = nullptr;
    ToolId m_current = ToolId::Select;
};
