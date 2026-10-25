#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

// Line icons from resources/icons/*.svg. The SVGs draw with "currentColor",
// which is replaced here, so one file serves every theme and state.
namespace Icons {

// A single-colour icon.
QIcon icon(const QString &name, const QColor &color);

// For checkable buttons: `normal` when off, `checked` when on.
QIcon toggleIcon(const QString &name, const QColor &normal, const QColor &checked);

} // namespace Icons
