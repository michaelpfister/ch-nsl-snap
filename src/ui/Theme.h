#pragma once

#include <QColor>
#include <QString>

class QApplication;

// The app's own look (docs/02-decisions.md D-014): colour tokens for light
// and dark, turned into one Qt style sheet. Values match the design mockup.
struct Theme
{
    bool dark = false;

    QColor bg;        // window background behind the panels
    QColor panel;     // top bar, tool rail, properties panel, status bar
    QColor text;
    QColor muted;     // secondary text and inactive icons
    QColor border;
    QColor canvas;    // area around the image
    QColor hover;     // hover background, segmented-control track
    QColor accent;    // primary button, focus, handles
    QColor soft;      // background of the active tool
    QColor softText;  // icon colour of the active tool

    static Theme light();
    static Theme darkTheme();

    // Follows the system's light/dark setting.
    static Theme current();

    QString styleSheet() const;

    // Sets palette, font and style sheet on the application.
    void apply(QApplication &app) const;
};
