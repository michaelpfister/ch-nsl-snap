#pragma once

#include "editor/ArrowStyle.h"
#include "editor/DimSettings.h"
#include "editor/FrameStyle.h"
#include "editor/MarkerStyle.h"
#include "editor/RedactStyle.h"
#include "editor/ReplaceStyle.h"
#include "editor/TextStyle.h"
#include "editor/Tool.h"

#include <QColor>
#include <QPointer>
#include <QWidget>

#include <functional>

class ColorPopup;
class ColorRow;
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

// The property groups of the panel; one or more are visible at a time.
enum class PanelSection { Frame, Redact, Replace, Crop, Text, Arrow, Marker };

// The panel on the right: style of the selected frames, or of new frames
// when nothing is selected, plus the list of objects (design mockup).
class PropertiesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void applyTheme(const Theme &theme);
    void showTool(ToolId id);
    // Title for a selection, e.g. "Rahmen" and "2 ausgewählt".
    void showSelection(const QString &title, int count);

    // Shows these property groups and hides the others.
    void showSections(const QList<PanelSection> &sections);
    // Size of the crop draft (F-12), in image pixels.
    void setCropSize(const QSize &size);
    // The look of free texts; the text itself is typed on the canvas.
    TextStyle textStyle() const;
    // Shows `style` without emitting textStyleChanged.
    void setTextStyle(const TextStyle &style);


    ReplaceStyle replaceStyle() const;
    // Shows `style` without emitting replaceStyleChanged.
    void setReplaceStyle(const ReplaceStyle &style);
    // Puts the cursor into the text field, for typing right after drawing.
    void focusReplaceText();
    ArrowStyle arrowStyle() const;
    // Shows `style` without emitting arrowStyleChanged.
    void setArrowStyle(const ArrowStyle &style);
    MarkerStyle markerStyle() const;
    // Shows `style` without emitting markerStyleChanged.
    void setMarkerStyle(const MarkerStyle &style);

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

    // The end of the eyedropper: `color` is the picked colour, or invalid
    // if the user cancelled. Applies it and opens the colour popup again.
    void finishEyedropper(const QColor &color);

    // The advanced mode (settings) shows the dim sliders (F-27).
    void setAdvancedMode(bool enabled);

    DimSettings dimSettings() const;
    // Shows `settings` without emitting dimSettingsChanged.
    void setDimSettings(const DimSettings &settings);

signals:
    // The eyedropper in the colour popup: the editor should let the user
    // click into the image and answer with finishEyedropper().
    void eyedropperRequested();
    // The user changed colour, corners or line width.
    void frameStyleChanged(const FrameStyle &style);
    // The user changed kind or strength of pixelating (F-26).
    void redactStyleChanged(const RedactStyle &style);
    // The user changed the replaced text or its look (F-29).
    void replaceStyleChanged(const ReplaceStyle &style);
    // The user changed colour or width of arrows (F-44), or the marker colour (F-28).
    void arrowStyleChanged(const ArrowStyle &style);
    void markerStyleChanged(const MarkerStyle &style);
    // The user changed colour, size, bold or box of free texts (F-25).
    void textStyleChanged(const TextStyle &style);
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
    // Opens the colour popup at `anchor`; `apply` receives every change.
    void openColorPopup(QWidget *anchor, const QColor &color, std::function<void(const QColor &)> apply);

    ColorPopup *m_colorPopup = nullptr;
    QPointer<QWidget> m_popupAnchor;
    std::function<void(const QColor &)> m_popupApply;
    ColorRow *m_frameColors = nullptr;
    ColorRow *m_textColors = nullptr;
    QWidget *m_arrowSection = nullptr;
    ColorRow *m_arrowColors = nullptr;
    QSlider *m_arrowWidth = nullptr;
    QWidget *m_markerSection = nullptr;
    ColorRow *m_markerColors = nullptr;

    QWidget *m_frameSection = nullptr;
    QWidget *m_redactSection = nullptr;
    QWidget *m_replaceSection = nullptr;
    QWidget *m_cropSection = nullptr;
    QWidget *m_textSection = nullptr;
    QSpinBox *m_textSize = nullptr;
    QCheckBox *m_textBold = nullptr;
    QButtonGroup *m_textBox = nullptr;
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
    QPushButton *m_square = nullptr;
    QPushButton *m_rounded = nullptr;
    QSlider *m_width = nullptr;
    QComboBox *m_badge = nullptr;
    QWidget *m_dimSection = nullptr;
    QCheckBox *m_dim = nullptr;
    QSlider *m_dimStrength = nullptr;
    QSlider *m_dimBlur = nullptr;
    QListWidget *m_objects = nullptr;
    QColor m_mutedText;
    bool m_updating = false; // true while setFrameStyle/setObjects run
};
