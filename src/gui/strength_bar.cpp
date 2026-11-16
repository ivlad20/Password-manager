#include "strength_bar.h"
#include "crypto/password_strength.h"

#include <sodium.h>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

// One color per score, from very weak to very strong.
static const char* const SCORE_COLORS[] = {"#E5534B", "#E8833A", "#E3B341", "#93D473", "#3F8A1E"};

StrengthBar::StrengthBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(3);

    bar = new QProgressBar(this);
    bar->setRange(0, MAX_PASSWORD_STRENGTH + 1);
    bar->setTextVisible(false);
    bar->setFixedHeight(6);
    layout->addWidget(bar);

    label = new QLabel(this);
    label->setObjectName(QStringLiteral("muted"));
    label->setWordWrap(true);
    layout->addWidget(label);

    hide();
}

void StrengthBar::set_password(const QString& password) {
    if (password.isEmpty()) {
        current_score = 0;
        hide();
        return;
    }

    std::string text = password.toStdString();
    const PasswordStrength strength = estimate_strength(text);
    sodium_memzero(text.data(), text.size());

    current_score = strength.score;
    bar->setValue(strength.score + 1);  // even a very weak password shows one segment
    bar->setStyleSheet(QStringLiteral("QProgressBar { background: #E4E5E1; border: none; border-radius: 3px; }"
                                      "QProgressBar::chunk { background: %1; border-radius: 3px; }")
                               .arg(QLatin1String(SCORE_COLORS[strength.score])));

    QString message = tr("Strength: %1").arg(QString::fromStdString(strength.label));
    if (!strength.advice.empty()) {
        message += QStringLiteral(". ") + QString::fromStdString(strength.advice);
    }
    label->setText(message);
    show();
}
