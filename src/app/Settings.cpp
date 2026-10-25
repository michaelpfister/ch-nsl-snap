#include "app/Settings.h"

#include <QSettings>

namespace Settings {

namespace {
const char *const kAdvancedMode = "editor/advancedMode";
}

bool advancedMode()
{
    return QSettings().value(kAdvancedMode, false).toBool();
}

void setAdvancedMode(bool enabled)
{
    QSettings().setValue(kAdvancedMode, enabled);
}

} // namespace Settings
