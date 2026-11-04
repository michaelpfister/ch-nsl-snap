#pragma once

class QWidget;

// Keeps a dialog inside the usable part of its screen (without the taskbar),
// so its title bar is never above the top edge, where it could not be
// dragged. Call it after setting the size the dialog wants, before showing:
// it shrinks the dialog if needed (its content should scroll) and lets Qt
// centre it on its parent as usual.
void fitIntoScreen(QWidget *window);
