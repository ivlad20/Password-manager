#pragma once

#include "core/vault.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QString>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;

// Result of opening or creating a vault on a worker thread.
struct OpenResult {
    std::shared_ptr<Vault> vault;  // set on success
    QString error;                 // set on failure
};

// First screen of the application. Creates a new vault when the file does not exist yet,
// otherwise asks for the master password. Key derivation (Argon2id) takes about a second,
// so it runs on a worker thread to keep the window responsive.
class UnlockDialog : public QDialog {
    Q_OBJECT

public:
    explicit UnlockDialog(QString vault_path, QWidget* parent = nullptr);

    // The unlocked vault after the dialog was accepted.
    [[nodiscard]] std::shared_ptr<Vault> vault() const { return unlocked_vault; }

public slots:
    void reject() override;

private slots:
    void submit();
    void on_finished();

private:
    void set_busy(bool busy, const QString& message = {});
    void show_error(const QString& message);

    QString vault_path;
    bool create_mode;
    bool busy = false;

    QLineEdit* password_edit = nullptr;
    QLineEdit* confirm_edit = nullptr;  // only in create mode
    QLabel* status_label = nullptr;
    QProgressBar* progress = nullptr;
    QPushButton* submit_button = nullptr;

    QFutureWatcher<OpenResult> watcher;
    std::shared_ptr<Vault> unlocked_vault;
};
