#pragma once

#include "editor/FrameStyle.h"
#include "editor/Tool.h"

#include <QColor>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
struct Theme;

// The panel on the right: style of the selected frames, or of new frames
// when nothing is selected, plus the list of objects (design mockup).
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void applyTheme(const Theme &theme);
    void showTool(ToolId id);
    // "Rahmen" with the number of selected frames.
    void showSelection(int count);

    FrameStyle frameStyle() const;
    // Shows `style` without emitting frameStyleChanged.
    void setFrameStyle(const FrameStyle &style);

    // One entry per annotation; `selected` marks the selected ones.
    void setObjects(const QStringList &labels, const QList<bool> &selected);

    QCheckBox *dimCheckBox() const { return m_dim; }

signals:
    // The user changed colour, corners or line width.
    void frameStyleChanged(const FrameStyle &style);
    // The user clicked an entry in the object list.
    void objectClicked(int index);

private:
    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;
    QButtonGroup *m_swatches = nullptr;
    QPushButton *m_square = nullptr;
    QPushButton *m_rounded = nullptr;
    QSlider *m_width = nullptr;
    QComboBox *m_badge = nullptr;
    QCheckBox *m_dim = nullptr;
    QListWidget *m_objects = nullptr;
    QColor m_color;
    QColor m_mutedText;
    bool m_updating = false; // true while setFrameStyle/setObjects run
};
