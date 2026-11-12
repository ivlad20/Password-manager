#pragma once

#include <string>

// Plaintext entry. Entries only exist decrypted in memory while the vault is unlocked
// on disk the whole list is encrypted together, so even the names stay hidden.
struct VaultEntry {
    std::string name;
    std::string username;
    std::string password;
    std::string url;
    std::string notes;
};
