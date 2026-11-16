#pragma once

#include <string>

// Moving a vault to another computer only needs a copy of its file: everything in it is
// encrypted, so the copy is safe to carry on a USB stick, send by email or keep in the
// cloud. It opens with the same master password.

// Copies the vault file into `folder` as PasswordManager-vault-YYYY-MM-DD.pwm, or
// "... (2).pwm", "... (3).pwm" when that name is taken. Returns the path of the copy.
// Throws if `vault_path` is not a vault file or the copy fails.
std::string export_vault(const std::string& vault_path, const std::string& folder);

// Copies the file at `path` next to it as vault-backup-YYYY-MM-DD.pwm (with " (2)" and so
// on when taken), before an import replaces it. Returns the path of the backup.
std::string backup_vault(const std::string& path);

// True if both paths name the same existing file.
bool same_file(const std::string& a, const std::string& b);
