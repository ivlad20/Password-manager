#include "vault_import.h"
#include "core/vault_transfer.h"
#include "message_dialog.h"
#include "storage/vault_file.h"
#include "unlock_dialog.h"

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

static std::function<QString(QWidget*)>& chooser_override() {
    static std::function<QString(QWidget*)> chooser;
    return chooser;
}

QString choose_vault_file(QWidget* parent) {
    if (chooser_override()) {
        return chooser_override()(parent);
    }
    return QFileDialog::getOpenFileName(parent, QObject::tr("Import a vault"),
                                        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                        QObject::tr("Password Manager vaults (*.pwm);;All files (*)"));
}

void set_vault_file_chooser(std::function<QString(QWidget*)> chooser) {
    chooser_override() = std::move(chooser);
}

std::shared_ptr<Vault> pick_and_unlock_vault(QWidget* parent, const QString& current_vault_path) {
    const QString file = choose_vault_file(parent);
    if (file.isEmpty()) {
        return nullptr;
    }
    const QString title = QObject::tr("Import a vault");

    if (same_file(file.toStdString(), current_vault_path.toStdString())) {
        MessageDialog::error(parent, title, QObject::tr("This is the vault that is already open."));
        return nullptr;
    }
    // Check the header before asking for a password.
    try {
        read_vault_file(file.toStdString());
    } catch (const std::exception&) {
        MessageDialog::error(parent, title,
                             QObject::tr("%1 is not a Password Manager vault, or it is damaged.")
                                     .arg(QDir::toNativeSeparators(file)));
        return nullptr;
    }

    UnlockDialog dialog(file, UnlockDialog::Purpose::Import, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return nullptr;
    }
    return dialog.vault();
}
