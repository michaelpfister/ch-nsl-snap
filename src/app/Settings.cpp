#include "app/Settings.h"

#include "app/FileNames.h"

#include <QSettings>

#include <algorithm>

namespace Settings {

namespace {
const char *const kAdvancedMode = "editor/advancedMode";
const char *const kRegionByDragging = "capture/regionByDragging";
const char *const kCaptureDelay = "capture/delay";
}

bool advancedMode()
{
    return QSettings().value(kAdvancedMode, false).toBool();
}

void setAdvancedMode(bool enabled)
{
    QSettings().setValue(kAdvancedMode, enabled);
}

int captureDelay()
{
    return QSettings().value(kCaptureDelay, 0).toInt();
}

void setCaptureDelay(int seconds)
{
    QSettings().setValue(kCaptureDelay, seconds);
}

bool regionByDragging()
{
    return QSettings().value(kRegionByDragging, false).toBool();
}

void setRegionByDragging(bool enabled)
{
    QSettings().setValue(kRegionByDragging, enabled);
}

QString saveFolder()
{
    const QString folder = QSettings().value("save/folder").toString();
    return folder.isEmpty() ? FileNames::defaultFolder() : folder;
}

void setSaveFolder(const QString &folder)
{
    QSettings().setValue("save/folder", folder);
}

QString fileNamePattern()
{
    const QString pattern = QSettings().value("save/pattern").toString();
    return pattern.trimmed().isEmpty() ? FileNames::defaultPattern() : pattern;
}

void setFileNamePattern(const QString &pattern)
{
    QSettings().setValue("save/pattern", pattern);
}

bool closeToTray()
{
    return QSettings().value("tray/closeToTray", true).toBool();
}

void setCloseToTray(bool enabled)
{
    QSettings().setValue("tray/closeToTray", enabled);
}

bool trayHintShown()
{
    return QSettings().value("tray/hintShown", false).toBool();
}

void setTrayHintShown(bool shown)
{
    QSettings().setValue("tray/hintShown", shown);
}

namespace {

// Colours are stored as "#rrggbb"; an invalid stored value gives `fallback`.
QColor colorValue(const QSettings &settings, const char *key, const QColor &fallback)
{
    const QColor color = QColor::fromString(settings.value(key).toString());
    return color.isValid() ? color : fallback;
}

} // namespace

FrameStyle frameStyle()
{
    const QSettings settings;
    FrameStyle style;
    style.color = colorValue(settings, "styles/frame/color", style.color);
    style.width = std::clamp(settings.value("styles/frame/width", style.width).toInt(), 1, 8);
    style.rounded = settings.value("styles/frame/rounded", style.rounded).toBool();
    // The corner where the number tool puts its numbers.
    const int badge = settings.value("styles/frame/badge", int(BadgePosition::TopRight)).toInt();
    style.badge = badge >= int(BadgePosition::TopRight) && badge <= int(BadgePosition::BottomLeft)
                      ? BadgePosition(badge) : BadgePosition::TopRight;
    return style;
}

void setFrameStyle(const FrameStyle &style)
{
    QSettings settings;
    settings.setValue("styles/frame/color", style.color.name());
    settings.setValue("styles/frame/width", style.width);
    settings.setValue("styles/frame/rounded", style.rounded);
    if (style.badge != BadgePosition::None)
        settings.setValue("styles/frame/badge", int(style.badge));
}

TextStyle textStyle()
{
    const QSettings settings;
    TextStyle style;
    style.color = colorValue(settings, "styles/text/color", style.color);
    style.fontSize = std::clamp(settings.value("styles/text/size", style.fontSize).toInt(), 8, 96);
    style.bold = settings.value("styles/text/bold", style.bold).toBool();
    const int box = settings.value("styles/text/box", int(style.box)).toInt();
    style.box = box >= int(TextBox::None) && box <= int(TextBox::Filled) ? TextBox(box) : TextBox::None;
    return style;
}

void setTextStyle(const TextStyle &style)
{
    QSettings settings;
    settings.setValue("styles/text/color", style.color.name());
    settings.setValue("styles/text/size", style.fontSize);
    settings.setValue("styles/text/bold", style.bold);
    settings.setValue("styles/text/box", int(style.box));
}

ArrowStyle arrowStyle()
{
    const QSettings settings;
    ArrowStyle style;
    style.color = colorValue(settings, "styles/arrow/color", style.color);
    style.width = std::clamp(settings.value("styles/arrow/width", style.width).toInt(), 2, 12);
    return style;
}

void setArrowStyle(const ArrowStyle &style)
{
    QSettings settings;
    settings.setValue("styles/arrow/color", style.color.name());
    settings.setValue("styles/arrow/width", style.width);
}

MarkerStyle markerStyle()
{
    const QSettings settings;
    MarkerStyle style;
    style.color = colorValue(settings, "styles/marker/color", style.color);
    return style;
}

void setMarkerStyle(const MarkerStyle &style)
{
    QSettings().setValue("styles/marker/color", style.color.name());
}

RedactStyle redactStyle()
{
    const QSettings settings;
    RedactStyle style;
    const int mode = settings.value("styles/redact/mode", int(style.mode)).toInt();
    const int level = settings.value("styles/redact/level", int(style.level)).toInt();
    if (mode >= int(RedactMode::Pixelate) && mode <= int(RedactMode::Blur))
        style.mode = RedactMode(mode);
    if (level >= int(RedactLevel::Light) && level <= int(RedactLevel::Strong))
        style.level = RedactLevel(level);
    return style;
}

void setRedactStyle(const RedactStyle &style)
{
    QSettings settings;
    settings.setValue("styles/redact/mode", int(style.mode));
    settings.setValue("styles/redact/level", int(style.level));
}

QString replaceText()
{
    return QSettings().value("styles/replace/text", QStringLiteral("Firstname Lastname")).toString();
}

void setReplaceText(const QString &text)
{
    QSettings().setValue("styles/replace/text", text);
}

void resetStyles()
{
    QSettings settings;
    settings.remove("styles");
    settings.remove("colors");
}

QList<QColor> recentColors()
{
    QList<QColor> colors;
    for (const QString &name : QSettings().value("colors/recent").toStringList()) {
        const QColor color = QColor::fromString(name);
        if (color.isValid() && !colors.contains(color) && colors.size() < kMaxRecentColors)
            colors << color;
    }
    return colors;
}

void addRecentColor(const QColor &color)
{
    if (!color.isValid())
        return;
    QList<QColor> colors = recentColors();
    colors.removeAll(color);
    colors.prepend(color);
    QStringList names;
    for (const QColor &c : std::as_const(colors).first(std::min<qsizetype>(colors.size(), kMaxRecentColors)))
        names << c.name();
    QSettings().setValue("colors/recent", names);
}

} // namespace Settings
