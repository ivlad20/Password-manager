#include "change_password_dialog.h"

#include <sodium.h>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

static void wipe(std::string& value) {
    if (!value.empty()) {
        sodium_memzero(value.data(), value.size());
    }
    value.clear();
}

ChangePasswordDialog::ChangePasswordDialog(std::shared_ptr<Vault> vault_to_change, QWidget* parent)
    : QDialog(parent), vault(std::move(vault_to_change)) {
    setWindowTitle(tr("Change master password"));
    setFixedWidth(460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Change master password"), this);
    title->setObjectName(QStringLiteral("title"));
    layout->addWidget(title);

    auto* intro = new QLabel(tr("The vault is encrypted again with the new password. "
                                "Copies of the vault made before keep the old password."),
                             this);
    intro->setObjectName(QStringLiteral("muted"));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    layout->addSpacing(6);

    auto make_password_edit = [&](const QString& placeholder) {
        auto* edit = new QLineEdit(this);
        edit->setEchoMode(QLineEdit::Password);
        edit->setPlaceholderText(placeholder);
        layout->addWidget(edit);
        return edit;
    };
    current_edit = make_password_edit(tr("Current master password"));
    new_edit = make_password_edit(tr("New master password"));
    confirm_edit = make_password_edit(tr("Repeat the new master password"));

    status_label = new QLabel(this);
    status_label->setWordWrap(true);
    status_label->hide();
    layout->addWidget(status_label);

    progress = new QProgressBar(this);
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->hide();
    layout->addWidget(progress);
    layout->addSpacing(8);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    cancel_button = new QPushButton(tr("Cancel"), this);
    submit_button = new QPushButton(tr("Change Password"), this);
    submit_button->setObjectName(QStringLiteral("primary"));
    submit_button->setDefault(true);
    buttons->addWidget(cancel_button);
    buttons->addWidget(submit_button);
    layout->addLayout(buttons);

    connect(cancel_button, &QPushButton::clicked, this, &ChangePasswordDialog::reject);
    connect(submit_button, &QPushButton::clicked, this, &ChangePasswordDialog::submit);
    connect(&watcher, &QFutureWatcher<ChangePasswordResult>::finished, this, &ChangePasswordDialog::on_finished);

    current_edit->setFocus();
}

void ChangePasswordDialog::reject() {
    // The worker thread is re-encrypting the vault; let it finish.
    if (busy) {
        return;
    }
    QDialog::reject();
}

void ChangePasswordDialog::submit() {
    if (busy) {
        return;
    }
    if (current_edit->text().isEmpty()) {
        show_error(tr("Enter the current master password."));
        current_edit->setFocus();
        return;
    }
    if (new_edit->text().size() < static_cast<qsizetype>(MIN_MASTER_PASSWORD_LENGTH)) {
        show_error(tr("The new master password must have at least %1 characters.").arg(MIN_MASTER_PASSWORD_LENGTH));
        new_edit->setFocus();
        return;
    }
    if (new_edit->text() != confirm_edit->text()) {
        show_error(tr("The new passwords do not match."));
        confirm_edit->clear();
        confirm_edit->setFocus();
        return;
    }
    if (new_edit->text() == current_edit->text()) {
        show_error(tr("The new master password is the same as the current one."));
        new_edit->setFocus();
        return;
    }

    std::string current = current_edit->text().toStdString();
    std::string replacement = new_edit->text().toStdString();
    current_edit->clear();
    new_edit->clear();
    confirm_edit->clear();
    set_busy(true);

    watcher.setFuture(QtConcurrent::run([target = vault, current, replacement]() mutable {
        ChangePasswordResult result;
        try {
            result.changed = target->change_master_password(current, replacement);
        } catch (const std::exception& error) {
            result.error = QString::fromStdString(error.what());
        }
        wipe(current);
        wipe(replacement);
        return result;
    }));

    wipe(current);
    wipe(replacement);
}

void ChangePasswordDialog::on_finished() {
    const ChangePasswordResult result = watcher.result();
    set_busy(false);

    if (result.changed) {
        accept();
        return;
    }
    if (result.error.isEmpty()) {
        show_error(tr("The current master password is wrong. Nothing was changed."));
        current_edit->setFocus();
    } else {
        show_error(tr("The vault could not be saved, the old master password still works:\n%1").arg(result.error));
    }
}

void ChangePasswordDialog::set_busy(bool value) {
    busy = value;
    current_edit->setEnabled(!value);
    new_edit->setEnabled(!value);
    confirm_edit->setEnabled(!value);
    cancel_button->setEnabled(!value);
    submit_button->setEnabled(!value);
    progress->setVisible(value);
    if (value) {
        status_label->setStyleSheet(QStringLiteral("color: #6B7078;"));
        status_label->setText(tr("Encrypting the vault with the new password..."));
        status_label->show();
    }
}

void ChangePasswordDialog::show_error(const QString& message) {
    status_label->setStyleSheet(QStringLiteral("color: #C93C34;"));
    status_label->setText(message);
    status_label->show();
}
