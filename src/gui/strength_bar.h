#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;

// A colored bar with a short text, shown under a password field: red for a very weak
// password up to green for a very strong one. Hidden while the field is empty.
class StrengthBar : public QWidget {
    Q_OBJECT

public:
    explicit StrengthBar(QWidget* parent = nullptr);

    // 0 (very weak) to MAX_PASSWORD_STRENGTH (very strong); 0 also for an empty password.
    [[nodiscard]] int score() const { return current_score; }

public slots:
    void set_password(const QString& password);

private:
    QProgressBar* bar = nullptr;
    QLabel* label = nullptr;
    int current_score = 0;
};
