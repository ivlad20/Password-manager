#pragma once

#include "lock_settings.h"

#include <QDialog>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;

// What the buttons in the settings dialog do. Each one gets the settings dialog as the
// parent for the dialogs it opens.
struct SettingsActions {
    std::function<bool(QWidget*)> change_master_password;  // true if the password was changed
    std::function<void(QWidget*)> export_vault;
    std::function<bool(QWidget*)> import_vault;            // true if a vault was imported
};

// Application settings: when to lock the vault automatically, the master password, and
// moving the vault to another computer. Settings that belong to the vault itself do not go here.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const LockSettings& current, SettingsActions actions, QWidget* parent = nullptr);

    // The chosen values; only meaningful after the dialog was accepted.
    [[nodiscard]] LockSettings settings() const;

private:
    void show_status(const QString& message);

    SettingsActions actions;

    QComboBox* auto_lock_combo = nullptr;
    QCheckBox* minimize_check = nullptr;
    QCheckBox* session_lock_check = nullptr;
    QLabel* status_label = nullptr;
};
