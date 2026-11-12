#pragma once

// Whether the user asked the system for fewer animations (D-043): GNOME's
// "Animationen" switch (org.gnome.desktop.interface enable-animations),
// Windows' "Animationseffekte". Then the app shows end states without
// movement. Read once and remembered; the environment variable
// NSL_REDUCED_MOTION=1 or 0 overrides it (tests, and for trying it out).
namespace Motion {

bool reduced();

} // namespace Motion
