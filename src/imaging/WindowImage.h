#pragma once

#include <QImage>
#include <QRect>

#include <optional>

struct CaptureResult;

// Turns a raw window capture into the final window image: crop to the opaque
// core (D-010), then clear shadow residue in the corners (D-009).
namespace WindowImage {

// How far from the frame edge we look for the opaque core, in logical px.
inline constexpr int kEdgeSearch = 4;
// Corner pixels below this alpha that are connected to a corner are shadow.
inline constexpr int kCornerAlphaThreshold = 80;
// Largest corner radius we expect, in logical px (libadwaita uses 12-15).
inline constexpr int kCornerMaxRadius = 24;

// The frame rectangle in image pixels of a whole-buffer capture.
QRect frameInImage(const QRect &frameRect, const QRect &bufferRect, double scale);

// The window's opaque core near `frame`: for each side, the outermost line
// within kEdgeSearch logical px whose middle half is fully opaque. Empty if a
// side has none, e.g. for a translucent window.
std::optional<QRect> opaqueCore(const QImage &image, const QRect &frame, double scale);

// Makes low-alpha pixels connected to each corner fully transparent and
// returns how many pixels were changed. Square corners are left alone.
int cleanCorners(QImage &image, double scale);

// Crop and clean in one step. Falls back to the frame rectangle if no
// opaque core is found.
QImage prepare(const CaptureResult &capture);

} // namespace WindowImage
