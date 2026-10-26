#include "app/Settings.h"

#include <QSettings>

namespace Settings {

namespace {
const char *const kAdvancedMode = "editor/advancedMode";
const char *const kRegionByDragging = "capture/regionByDragging";
}

bool advancedMode()
{
    return QSettings().value(kAdvancedMode, false).toBool();
}

void setAdvancedMode(bool enabled)
{
    QSettings().setValue(kAdvancedMode, enabled);
}

bool regionByDragging()
{
    return QSettings().value(kRegionByDragging, false).toBool();
}

void setRegionByDragging(bool enabled)
{
    QSettings().setValue(kRegionByDragging, enabled);
}

} // namespace Settings
