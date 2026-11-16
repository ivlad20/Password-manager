#pragma once

#include <cstdint>
#include <string>
#include <vector>

// How many old passwords an entry remembers.
constexpr size_t MAX_PASSWORD_HISTORY = 5;

// A password the entry used before, kept so it can be recovered after a change.
struct PasswordHistoryItem {
    std::string password;
    uint64_t changed_at = 0;  // when it was replaced, Unix time in seconds
};

// Plaintext entry. Entries only exist decrypted in memory while the vault is unlocked;
// on disk the whole list is encrypted together, so even the names stay hidden.
struct VaultEntry {
    std::string name;
    std::string username;
    std::string password;
    std::string url;
    std::string notes;

    // Unix time in seconds; 0 means unknown (entries migrated from format version 1).
    uint64_t created_at = 0;
    uint64_t modified_at = 0;

    bool favorite = false;
    std::vector<std::string> tags;

    // Newest first, at most MAX_PASSWORD_HISTORY items. Managed by Vault::update_entry.
    std::vector<PasswordHistoryItem> password_history;
};
