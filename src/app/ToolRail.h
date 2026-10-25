#pragma once

#include "editor/DimSettings.h"
#include "editor/Tool.h"

#include <QMap>
#include <QWidget>

class QButtonGroup;
class QToolButton;
struct Theme;

// The vertical tool bar on the left (design mockup, F-20 ff.). At the
// bottom: the "dim background" switch and, while it is on, three buttons
// above it for the blur level (F-27).
class ToolRail : public QWidget
{
    Q_OBJECT

public:
    explicit ToolRail(QWidget *parent = nullptr);

    void applyTheme(const Theme &theme);
    void setCurrentTool(ToolId id);
    ToolId currentTool() const { return m_current; }

    // Shows the scene's dim state without emitting signals.
    void showDim(const DimSettings &settings);

signals:
    void toolChanged(ToolId id);
    void dimToggled(bool enabled);
    // One of the blur buttons: DimSettings::kBlurNone, kBlurLight or kBlurStrong.
    void blurChosen(int blur);

private:
    QButtonGroup *m_group = nullptr;
    QMap<ToolId, QToolButton *> m_buttons;
    QToolButton *m_dimButton = nullptr;
    QWidget *m_blurButtons = nullptr;
    QMap<int, QToolButton *> m_blurButton; // by blur value
    ToolId m_current = ToolId::Select;
};
