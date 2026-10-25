#pragma once

#include "editor/Tool.h"

#include <QColor>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
struct Theme;

// The panel on the right: properties of the active tool or selected object
// (design mockup). The values are not wired to annotations yet; that comes
// with the tools themselves (D-012).
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void applyTheme(const Theme &theme);
    void showTool(ToolId id);

    QColor frameColor() const { return m_color; }
    bool roundedCorners() const;
    QCheckBox *dimCheckBox() const { return m_dim; }

signals:
    void frameColorChanged(const QColor &color);

private:
    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;
    QButtonGroup *m_swatches = nullptr;
    QPushButton *m_square = nullptr;
    QPushButton *m_rounded = nullptr;
    QSlider *m_width = nullptr;
    QCheckBox *m_dim = nullptr;
    QListWidget *m_objects = nullptr;
    QColor m_color;
};
