#pragma once

#include <QDateTime>
#include <QString>

// File names for saved images (F-41). The pattern is a text with
// placeholders, set in the settings:
//   {Datum} 2026-10-26   {Zeit} 14-05-09   {Titel} the window title, "Bereich", …
namespace FileNames {

QString defaultPattern(); // "{Datum}_{Zeit}_{Titel}"

// The file name for `pattern`, with ".png". Characters that are not
// allowed in file names on Windows, macOS or Linux become "_"; an empty
// result becomes "screenshot".
QString fromPattern(const QString &pattern, const QString &title, const QDateTime &time);

// ~/Pictures/NSL Snap (or the home folder if there is no pictures folder).
QString defaultFolder();

} // namespace FileNames
