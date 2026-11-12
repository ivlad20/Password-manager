#include "unlock_dialog.h"

#include <sodium.h>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

static const int MIN_MASTER_PASSWORD_LENGTH = 8;

UnlockDialog::UnlockDialog(QString path, QWidget* parent)
    : QDialog(parent), vault_path(std::move(path)), create_mode(!QFileInfo::exists(vault_path)) {
    setWindowTitle(create_mode ? tr("Create a new vault") : tr("Unlock vault"));
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(this);
    intro->setWordWrap(true);
    if (create_mode) {
        intro->setText(tr("No vault was found. Choose a master password for a new vault.\n"
                          "It cannot be recovered if you forget it."));
    } else {
        intro->setText(tr("Enter the master password for:\n%1").arg(QDir::toNativeSeparators(vault_path)));
    }
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    password_edit = new QLineEdit(this);
    password_edit->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Master password:"), password_edit);
    if (create_mode) {
        confirm_edit = new QLineEdit(this);
        confirm_edit->setEchoMode(QLineEdit::Password);
        form->addRow(tr("Repeat password:"), confirm_edit);
    }
    layout->addLayout(form);

    status_label = new QLabel(this);
    status_label->setWordWrap(true);
    status_label->hide();
    layout->addWidget(status_label);

    // An indeterminate progress bar while Argon2id runs.
    progress = new QProgressBar(this);
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->hide();
    layout->addWidget(progress);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    submit_button = buttons->button(QDialogButtonBox::Ok);
    submit_button->setText(create_mode ? tr("Create") : tr("Unlock"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &UnlockDialog::submit);
    connect(buttons, &QDialogButtonBox::rejected, this, &UnlockDialog::reject);
    connect(&watcher, &QFutureWatcher<OpenResult>::finished, this, &UnlockDialog::on_finished);

    password_edit->setFocus();
}

void UnlockDialog::reject() {
    // Closing while the worker thread runs could leave a half-created vault behind.
    if (busy) {
        return;
    }
    QDialog::reject();
}

void UnlockDialog::submit() {
    if (busy) {
        return;
    }

    std::string password = password_edit->text().toStdString();

    if (create_mode) {
        if (password_edit->text().size() < MIN_MASTER_PASSWORD_LENGTH) {
            show_error(tr("The master password must have at least %1 characters.").arg(MIN_MASTER_PASSWORD_LENGTH));
            sodium_memzero(password.data(), password.size());
            return;
        }
        if (password_edit->text() != confirm_edit->text()) {
            show_error(tr("The passwords do not match."));
            confirm_edit->clear();
            confirm_edit->setFocus();
            sodium_memzero(password.data(), password.size());
            return;
        }
    }

    password_edit->clear();
    if (confirm_edit != nullptr) {
        confirm_edit->clear();
    }
    set_busy(true, create_mode ? tr("Creating the vault...") : tr("Unlocking..."));

    const bool create = create_mode;
    const std::string path = vault_path.toStdString();

    watcher.setFuture(QtConcurrent::run([path, password, create]() mutable {
        OpenResult result;
        try {
            if (create) {
                result.vault = std::make_shared<Vault>(Vault::create(path, password));
            } else {
                auto vault = std::make_shared<Vault>(Vault::load(path));
                if (vault->unlock(password)) {
                    result.vault = vault;
                } else {
                    result.error = QStringLiteral("Wrong master password.");
                }
            }
        } catch (const std::exception& error) {
            result.error = QString::fromStdString(error.what());
        }
        sodium_memzero(password.data(), password.size());
        return result;
    }));

    sodium_memzero(password.data(), password.size());
}

void UnlockDialog::on_finished() {
    OpenResult result = watcher.result();
    set_busy(false);

    if (result.vault) {
        unlocked_vault = result.vault;
        accept();
        return;
    }
    show_error(result.error);
    password_edit->setFocus();
}

void UnlockDialog::set_busy(bool value, const QString& message) {
    busy = value;
    password_edit->setEnabled(!value);
    if (confirm_edit != nullptr) {
        confirm_edit->setEnabled(!value);
    }
    submit_button->setEnabled(!value);
    progress->setVisible(value);
    if (value) {
        status_label->setStyleSheet({});
        status_label->setText(message);
        status_label->show();
    }
}

void UnlockDialog::show_error(const QString& message) {
    status_label->setStyleSheet(QStringLiteral("color: #c0392b;"));
    status_label->setText(message);
    status_label->show();
}
