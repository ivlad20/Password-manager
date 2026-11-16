#pragma once

#include "core/vault.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QString>
#include <memory>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

// Result of changing the master password on a worker thread.
struct ChangePasswordResult {
    bool changed = false;
    QString error;  // empty when the current password was simply wrong
};

// Asks for the current master password and a new one, then re-encrypts the vault.
// Argon2id runs twice (checking the old password, deriving the new key), so the work
// happens on a worker thread like in UnlockDialog.
class ChangePasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChangePasswordDialog(std::shared_ptr<Vault> vault, QWidget* parent = nullptr);

public slots:
    void reject() override;

private slots:
    void submit();
    void on_finished();

private:
    void set_busy(bool busy);
    void show_error(const QString& message);

    std::shared_ptr<Vault> vault;
    bool busy = false;

    QLineEdit* current_edit = nullptr;
    QLineEdit* new_edit = nullptr;
    QLineEdit* confirm_edit = nullptr;
    QLabel* status_label = nullptr;
    QProgressBar* progress = nullptr;
    QPushButton* cancel_button = nullptr;
    QPushButton* submit_button = nullptr;

    QFutureWatcher<ChangePasswordResult> watcher;
};
