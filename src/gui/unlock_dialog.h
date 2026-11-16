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
class StrengthBar;

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
    enum class Purpose {
        OpenOrCreate,  // the application's own vault: create it if missing, otherwise unlock it
        Import,        // a vault file being imported: only checks its master password
    };

    explicit UnlockDialog(QString vault_path, Purpose purpose = Purpose::OpenOrCreate,
                          QWidget* parent = nullptr);

    // The unlocked vault after the dialog was accepted.
    [[nodiscard]] std::shared_ptr<Vault> vault() const { return unlocked_vault; }

signals:
    // Emitted right before the dialog closes successfully, while it is still on screen.
    // The main window shows itself then, so the focus goes straight to it and not to
    // another application's window in between.
    void unlocked();

public slots:
    void reject() override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void submit();
    void on_finished();
    // Create screen: use a vault copied from another computer instead of a new one.
    void import_existing();

private:
    void set_busy(bool busy, const QString& message = {});
    void show_error(const QString& message);

    QString vault_path;
    Purpose purpose;
    bool create_mode;
    bool busy = false;

    QLineEdit* password_edit = nullptr;
    QLineEdit* confirm_edit = nullptr;  // only in create mode
    QLabel* status_label = nullptr;
    QProgressBar* progress = nullptr;
    QPushButton* submit_button = nullptr;
    QPushButton* import_button = nullptr;  // only in create mode
    StrengthBar* strength = nullptr;       // only in create mode

    QFutureWatcher<OpenResult> watcher;
    std::shared_ptr<Vault> unlocked_vault;
};
