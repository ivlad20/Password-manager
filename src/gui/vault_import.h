#pragma once

#include "core/vault.h"

#include <QString>
#include <functional>
#include <memory>

class QWidget;

// Asks for a vault file to import (*.pwm). Returns an empty string if cancelled.
QString choose_vault_file(QWidget* parent);

// Replaces the file dialog, which tests cannot drive. Pass nullptr to restore it.
void set_vault_file_chooser(std::function<QString(QWidget*)> chooser);

// Lets the user pick a vault file and type its master password. Returns the unlocked
// vault, still bound to the chosen file, or nullptr if the user cancelled. Problems
// (not a vault file, the vault that is already open) are shown to the user.
std::shared_ptr<Vault> pick_and_unlock_vault(QWidget* parent, const QString& current_vault_path);
