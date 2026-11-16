#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace theme {

void apply(QApplication& app) {
    // The native Windows style ignores parts of the style sheet; Fusion draws everything itself.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    // The design is light only, so do not follow Windows' dark mode.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);

    // Style sheets cannot color links inside labels; the palette does.
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::Link, QColor(0x3F, 0x8A, 0x1E));
    palette.setColor(QPalette::LinkVisited, QColor(0x3F, 0x8A, 0x1E));
    QApplication::setPalette(palette);

    QFile file(QStringLiteral(":/style.qss"));
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

}  // namespace theme
