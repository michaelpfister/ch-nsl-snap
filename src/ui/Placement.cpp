#include "ui/Placement.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include <algorithm>

void fitIntoScreen(QWidget *window)
{
    // The screen the dialog will open on: its parent's, else the main one.
    QScreen *screen = window->parentWidget() ? window->parentWidget()->screen() : QGuiApplication::primaryScreen();
    if (!screen)
        return;
    // Room for the window frame and title bar, which geometry() excludes and
    // which the platform only reports once the window is shown.
    const int frameAllowance = 60;
    const QRect available = screen->availableGeometry();
    const QSize wanted = window->size().expandedTo(window->minimumSizeHint());
    const QSize fitting(std::min(wanted.width(), available.width() - 20),
                        std::min(wanted.height(), available.height() - frameAllowance));
    // A minimum larger than the screen would win over resize().
    window->setMinimumSize(window->minimumSize().boundedTo(fitting));
    window->resize(fitting);
}
