#pragma once

// Helpers for driving the GUI from tests. The tests only use the public widgets
// (found by type, text or placeholder), the same way a user sees the window.

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <functional>

// Processes events until `condition` is true or the timeout expires.
inline bool wait_until(const std::function<bool()>& condition, int timeout_ms = 30000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_ms) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return condition();
}

inline QList<QLineEdit*> line_edits(QWidget* widget) {
    return widget->findChildren<QLineEdit*>();
}

inline QLineEdit* line_edit_with_placeholder(QWidget* widget, const QString& placeholder) {
    for (QLineEdit* edit : line_edits(widget)) {
        if (edit->placeholderText().startsWith(placeholder)) return edit;
    }
    return nullptr;
}

// All visible label texts joined together, to check error and status messages.
inline QString visible_text(QWidget* widget) {
    QString all;
    for (QLabel* label : widget->findChildren<QLabel*>()) {
        if (label->isVisible()) all += label->text() + QLatin1Char('|');
    }
    return all;
}

inline QPushButton* button(QWidget* widget, const QString& text) {
    for (QPushButton* b : widget->findChildren<QPushButton*>()) {
        if (b->text().remove(QLatin1Char('&')) == text) return b;
    }
    return nullptr;
}

inline QAction* action(QWidget* widget, const QString& text) {
    for (QAction* a : widget->findChildren<QAction*>()) {
        if (a->text().remove(QLatin1Char('&')) == text) return a;
    }
    return nullptr;
}

// Modal dialogs block in exec(), so the test schedules what to do in the dialog
// before triggering the action that opens it.
inline void on_next_dialog(std::function<void(QWidget*)> handle) {
    QTimer::singleShot(0, [handle] {
        wait_until([] { return QApplication::activeModalWidget() != nullptr; }, 5000);
        handle(QApplication::activeModalWidget());
    });
}
