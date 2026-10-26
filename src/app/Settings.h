#pragma once

// User settings, stored with QSettings (~/.config/netslash.ch/NSL Snap.conf
// on Linux, the registry on Windows, a plist on macOS).
namespace Settings {

// Shows extra controls, e.g. the dim strength and blur sliders (F-27).
bool advancedMode();
void setAdvancedMode(bool enabled);

// Region capture (F-03, D-023): false = two clicks (default), true = drag.
bool regionByDragging();
void setRegionByDragging(bool enabled);

} // namespace Settings
