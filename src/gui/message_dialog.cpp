#include "message_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MessageDialog::MessageDialog(Kind kind, const QString& title, const QString& text,
                             const QString& accept_text, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(title);
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);

    auto* title_label = new QLabel(title, this);
    title_label->setObjectName(QStringLiteral("title"));
    layout->addWidget(title_label);

    auto* text_label = new QLabel(text, this);
    text_label->setWordWrap(true);
    text_label->setObjectName(QStringLiteral("muted"));
    layout->addWidget(text_label);
    layout->addSpacing(8);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();

    auto* accept_button = new QPushButton(accept_text, this);
    accept_button->setObjectName(kind == Kind::ConfirmDangerous ? QStringLiteral("danger")
                                                                : QStringLiteral("primary"));
    connect(accept_button, &QPushButton::clicked, this, &QDialog::accept);

    if (kind != Kind::Error) {
        auto* cancel_button = new QPushButton(tr("Cancel"), this);
        connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
        buttons->addWidget(cancel_button);
        // For destructive actions Enter must not delete by accident.
        (kind == Kind::ConfirmDangerous ? cancel_button : accept_button)->setDefault(true);
    } else {
        accept_button->setDefault(true);
    }
    buttons->addWidget(accept_button);
    layout->addLayout(buttons);
}

bool MessageDialog::confirm_dangerous(QWidget* parent, const QString& title, const QString& text,
                                      const QString& accept_text) {
    MessageDialog dialog(Kind::ConfirmDangerous, title, text, accept_text, parent);
    return dialog.exec() == QDialog::Accepted;
}

void MessageDialog::error(QWidget* parent, const QString& title, const QString& text) {
    MessageDialog dialog(Kind::Error, title, text, tr("OK"), parent);
    dialog.exec();
}
