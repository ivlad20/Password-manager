#pragma once

#include "lock_settings.h"

#include <QDialog>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;

// Application settings: when to lock the vault automatically, and a button that
// changes the master password. Settings that belong to the vault itself do not go here.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    // `change_master_password` opens the change password dialog over the given parent
    // and returns true if the password was changed.
    SettingsDialog(const LockSettings& current,
                   std::function<bool(QWidget* parent)> change_master_password,
                   QWidget* parent = nullptr);

    // The chosen values; only meaningful after the dialog was accepted.
    [[nodiscard]] LockSettings settings() const;

private slots:
    void on_change_master_password();

private:
    std::function<bool(QWidget*)> change_master_password;

    QComboBox* auto_lock_combo = nullptr;
    QCheckBox* minimize_check = nullptr;
    QCheckBox* session_lock_check = nullptr;
    QLabel* password_status = nullptr;
};
