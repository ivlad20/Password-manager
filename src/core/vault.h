//
// Created by vladi on 11/12/2026.
//

#pragma once

#include "vault_entry.h"
#include "../storage/vault_file.h"

#include <cstdint>
#include <string>
#include <vector>

// Minimum length asked for a new master password (by the CLI and the GUI).
constexpr size_t MIN_MASTER_PASSWORD_LENGTH = 8;

// A vault bound to one file on disk.
// The key and the decrypted entries only live in memory between unlock() and lock();
// the key is never written to disk. A wrong master password is detected because
// decryption fails its authentication check.
class Vault {
public:
    // Creates a new vault file (fails if the file already exists) and returns it unlocked.
    static Vault create(const std::string& path, const std::string& master_password);

    // Reads an existing vault file and returns it locked.
    static Vault load(const std::string& path);

    Vault(const Vault&) = delete;
    Vault& operator=(const Vault&) = delete;
    Vault(Vault&& other) noexcept;
    Vault& operator=(Vault&& other) noexcept;
    ~Vault();

    // Returns false if the master password is wrong.
    bool unlock(const std::string& master_password);
    // Wipes the key and the decrypted entries from memory.
    void lock();
    [[nodiscard]] bool is_unlocked() const { return unlocked; }

    // Encrypts all entries with a fresh nonce and writes them to disk.
    void save();

    // Saves the vault to another file and uses that file from now on (for importing:
    // the vault unlocked from the chosen file is saved over the default vault).
    void save_to(const std::string& new_path);

    // Re-encrypts the vault with a key derived from `new_password` and a new random salt,
    // and saves it immediately. Returns false (and changes nothing) if `current_password`
    // is wrong. Throws if the file cannot be written; the old password then stays valid.
    bool change_master_password(const std::string& current_password, const std::string& new_password);

    // Throws if an entry with the same name already exists.
    void add_entry(const VaultEntry& entry);
    // Replaces the entry called `name` (the name itself may change).
    // Throws if it does not exist or if the new name is taken by another entry.
    void update_entry(const std::string& name, const VaultEntry& updated);
    // Returns false if no entry has this name.
    bool remove_entry(const std::string& name);
    // Returns nullptr if no entry has this name.
    [[nodiscard]] const VaultEntry* find_entry(const std::string& name) const;
    [[nodiscard]] const std::vector<VaultEntry>& get_entries() const;

private:
    Vault(std::string path, VaultFile file);
    void require_unlocked() const;
    // Encrypts the entries with `with_key` and a fresh nonce, writes `base` with the new
    // ciphertext to disk and returns what was written.
    VaultFile write_entries(VaultFile base, const std::vector<uint8_t>& with_key) const;

    std::string path;
    VaultFile file;
    std::vector<uint8_t> key;
    std::vector<VaultEntry> entries;
    bool unlocked = false;
};
