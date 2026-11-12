#pragma once

#include "core/vault_entry.h"

#include <QDialog>
#include <functional>
#include <string>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

// Dialog for adding a new entry or editing an existing one.
class EntryDialog : public QDialog {
    Q_OBJECT

public:
    // `existing` is nullptr when adding. `name_taken` tells the dialog whether another
    // entry already uses a name, so the dialog can refuse duplicates before closing.
    EntryDialog(const VaultEntry* existing,
                std::function<bool(const std::string&)> name_taken,
                QWidget* parent = nullptr);
    ~EntryDialog() override;

    // The values typed by the user; only meaningful after the dialog was accepted.
    [[nodiscard]] VaultEntry entry() const;

private slots:
    void generate_password();
    void toggle_password_visibility(bool visible);
    void validate_and_accept();

private:
    std::function<bool(const std::string&)> name_taken;
    std::string original_name;

    QLineEdit* name_edit = nullptr;
    QLineEdit* username_edit = nullptr;
    QLineEdit* password_edit = nullptr;
    QSpinBox* length_spin = nullptr;
    QLineEdit* url_edit = nullptr;
    QPlainTextEdit* notes_edit = nullptr;
    QLabel* error_label = nullptr;
};
