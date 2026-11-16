#pragma once

class QWidget;

// Shows a window on top of the other applications' windows and gives it the keyboard focus.
//
// On Windows, activateWindow() alone is often not enough: Windows only lets the application
// that the user is working with take the foreground, otherwise the taskbar button just
// flashes and the window opens behind the others. When the window is not in front yet, this
// briefly attaches to the input of the foreground window's thread, which Windows allows to
// change the foreground (the same thing Qt does for its "always activate" option).
void bring_to_front(QWidget* window);
