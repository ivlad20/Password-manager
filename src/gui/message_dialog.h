#pragma once

#include <QDialog>

class QPushButton;

// Styled replacement for QMessageBox: a title, a message and one or two buttons.
class MessageDialog : public QDialog {
    Q_OBJECT

public:
    enum class Kind {
        Confirm,          // Cancel + a primary button
        ConfirmDangerous, // Cancel + a red button; Cancel is the default
        Error,            // a single OK button
    };

    MessageDialog(Kind kind, const QString& title, const QString& text,
                  const QString& accept_text, QWidget* parent = nullptr);

    // Returns true if the user pressed the red button.
    static bool confirm_dangerous(QWidget* parent, const QString& title, const QString& text,
                                  const QString& accept_text);
    static void error(QWidget* parent, const QString& title, const QString& text);
};
