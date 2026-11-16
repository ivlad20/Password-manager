#include "window_activation.h"

#include <QGuiApplication>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void bring_to_front(QWidget* window) {
    window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
    window->show();
    window->raise();
    window->activateWindow();

#ifdef Q_OS_WIN
    // Only with the real Windows platform: the tests use "offscreen", which has no HWND.
    if (QGuiApplication::platformName() != QStringLiteral("windows")) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    const HWND foreground = GetForegroundWindow();
    if (foreground == hwnd) {
        return;
    }
    const DWORD foreground_thread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD this_thread = GetCurrentThreadId();
    if (foreground != nullptr && foreground_thread != this_thread &&
        AttachThreadInput(foreground_thread, this_thread, TRUE)) {
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        AttachThreadInput(foreground_thread, this_thread, FALSE);
    } else {
        SetForegroundWindow(hwnd);
    }
#endif
}
