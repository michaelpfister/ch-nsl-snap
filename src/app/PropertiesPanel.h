#pragma once

#include "editor/DimSettings.h"
#include "editor/FrameStyle.h"
#include "editor/RedactStyle.h"
#include "editor/ReplaceStyle.h"
#include "editor/Tool.h"

#include <QColor>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QSpinBox;
class QToolButton;
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

    // Which property groups are visible: frame (colour, corners, line,
    // number) and/or pixelate (kind, strength).
    void showSections(bool frame, bool redact, bool replace = false, bool crop = false);
    // Size of the crop draft (F-12), in image pixels.
    void setCropSize(const QSize &size);
    // "Text ersetzen" for a selected replaced text.
    void showReplaceSelection();

    ReplaceStyle replaceStyle() const;
    // Shows `style` without emitting replaceStyleChanged.
    void setReplaceStyle(const ReplaceStyle &style);
    // Puts the cursor into the text field, for typing right after drawing.
    void focusReplaceText();
    // "Verpixeln" with the number of selected areas.
    void showRedactSelection(int count);

    RedactStyle redactStyle() const;
    // Shows `style` without emitting redactStyleChanged.
    void setRedactStyle(const RedactStyle &style);

    FrameStyle frameStyle() const;
    // Shows `style` without emitting frameStyleChanged.
    void setFrameStyle(const FrameStyle &style);

    // Number positions offered in the "Nummer" field; "Ohne Nummer" is
    // always there. Positions where the number would be cut off are hidden.
    void setBadgeOptions(const QList<BadgePosition> &positions);

    // One entry per annotation; `selected` marks the selected ones.
    void setObjects(const QStringList &labels, const QList<bool> &selected);

    // The advanced mode (settings) shows the dim sliders (F-27).
    void setAdvancedMode(bool enabled);

    DimSettings dimSettings() const;
    // Shows `settings` without emitting dimSettingsChanged.
    void setDimSettings(const DimSettings &settings);

signals:
    // The user changed colour, corners or line width.
    void frameStyleChanged(const FrameStyle &style);
    // The user changed kind or strength of pixelating (F-26).
    void redactStyleChanged(const RedactStyle &style);
    // The user changed the replaced text or its look (F-29).
    void replaceStyleChanged(const ReplaceStyle &style);
    // "Original neu erkennen".
    void rematchRequested();
    // The user clicked an entry in the object list.
    void objectClicked(int index);
    // The user dragged an entry in the object list to a new place (F-24).
    void objectMoved(int from, int to);
    // "Zuschneiden" and "Ganzes Bild" in the crop section (F-12).
    void cropApplyRequested();
    void cropResetRequested();
    // The user changed the dim switch or one of its sliders (F-27).
    void dimSettingsChanged(const DimSettings &settings);

private:
    void updateColorButtons();

    QWidget *m_frameSection = nullptr;
    QWidget *m_redactSection = nullptr;
    QWidget *m_replaceSection = nullptr;
    QWidget *m_cropSection = nullptr;
    QLabel *m_cropSize = nullptr;
    QLineEdit *m_replaceText = nullptr;
    QFontComboBox *m_replaceFont = nullptr;
    QSpinBox *m_replaceSize = nullptr;
    QCheckBox *m_replaceBold = nullptr;
    QToolButton *m_replaceColor = nullptr;
    QToolButton *m_replaceBackground = nullptr;
    QColor m_replaceTextColor;
    QColor m_replaceBackgroundColor;
    ReplaceStyle m_replaceBase;
    QButtonGroup *m_redactMode = nullptr;
    QButtonGroup *m_redactLevel = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;
    QButtonGroup *m_swatches = nullptr;
    QPushButton *m_square = nullptr;
    QPushButton *m_rounded = nullptr;
    QSlider *m_width = nullptr;
    QComboBox *m_badge = nullptr;
    QWidget *m_dimSection = nullptr;
    QCheckBox *m_dim = nullptr;
    QSlider *m_dimStrength = nullptr;
    QSlider *m_dimBlur = nullptr;
    QListWidget *m_objects = nullptr;
    QColor m_color;
    QColor m_mutedText;
    bool m_updating = false; // true while setFrameStyle/setObjects run
};
