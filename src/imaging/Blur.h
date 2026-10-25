#pragma once

#include <QImage>

namespace Blur {

// Soft blur for the dimmed background (F-27): three box-blur passes in each
// direction, which come close to a Gaussian blur. `radius` is in image
// pixels; 0 returns the image unchanged. Keeps transparency.
QImage boxBlur(const QImage &image, int radius);

} // namespace Blur
