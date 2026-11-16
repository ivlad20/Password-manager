#pragma once

class QApplication;

namespace theme {

// Switches to the Fusion style (fully styleable on every platform), forces the light
// color scheme and loads the application style sheet from the Qt resources (:/style.qss).
void apply(QApplication& app);

}  // namespace theme
