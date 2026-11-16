//
// Created by vladi on 11/12/2026.
//

#include "vault.h"
#include "../crypto/crypto.h"
#include "../storage/serialization.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <utility>

static void wipe(std::string& value) {
    if (!value.empty()) {
        sodium_memzero(value.data(), value.size());
    }
    value.clear();
}

static void wipe(std::vector<uint8_t>& value) {
    if (!value.empty()) {
        sodium_memzero(value.data(), value.size());
    }
    value.clear();
}

static void wipe(VaultEntry& entry) {
    wipe(entry.name);
    wipe(entry.username);
    wipe(entry.password);
    wipe(entry.url);
    wipe(entry.notes);
    for (std::string& tag : entry.tags) {
        wipe(tag);
    }
    entry.tags.clear();
    for (PasswordHistoryItem& item : entry.password_history) {
        wipe(item.password);
    }
    entry.password_history.clear();
}

static uint64_t now() {
    return static_cast<uint64_t>(std::time(nullptr));
}

// True if the user-visible content differs (timestamps and history are ignored).
static bool content_differs(const VaultEntry& a, const VaultEntry& b) {
    return a.name != b.name || a.username != b.username || a.password != b.password ||
           a.url != b.url || a.notes != b.notes || a.favorite != b.favorite || a.tags != b.tags;
}

static void wipe(std::vector<VaultEntry>& entries) {
    for (VaultEntry& entry : entries) {
        wipe(entry);
    }
    entries.clear();
}

Vault::Vault(std::string path, VaultFile file)
    : path(std::move(path)), file(std::move(file)) {}

Vault::Vault(Vault&& other) noexcept
    : path(std::move(other.path)), file(std::move(other.file)),
      key(std::move(other.key)), entries(std::move(other.entries)), unlocked(other.unlocked) {
    other.unlocked = false;
}

Vault& Vault::operator=(Vault&& other) noexcept {
    if (this != &other) {
        // Wipe our own secrets before they are replaced and freed.
        lock();
        path = std::move(other.path);
        file = std::move(other.file);
        key = std::move(other.key);
        entries = std::move(other.entries);
        unlocked = other.unlocked;
        other.unlocked = false;
    }
    return *this;
}

Vault::~Vault() {
    lock();
}

Vault Vault::create(const std::string& path, const std::string& master_password) {
    if (std::filesystem::exists(path)) {
        throw std::runtime_error(path + " already exists");
    }

    VaultFile file;
    file.opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
    file.memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
    randombytes_buf(file.salt.data(), file.salt.size());

    Vault vault(path, std::move(file));
    vault.key = master_password_derive(master_password, vault.file.salt.data(),
                                       vault.file.opslimit, static_cast<size_t>(vault.file.memlimit));
    vault.unlocked = true;
    vault.save();
    return vault;
}

Vault Vault::load(const std::string& path) {
    return Vault(path, read_vault_file(path));
}

bool Vault::unlock(const std::string& master_password) {
    if (unlocked) {
        return true;
    }

    std::vector<uint8_t> derived = master_password_derive(master_password, file.salt.data(),
                                                          file.opslimit, static_cast<size_t>(file.memlimit));

    std::vector<uint8_t> payload;
    try {
        payload = decrypt_bytes(file.ciphertext, derived, file.nonce);
    } catch (const std::runtime_error&) {
        // Authentication failed: wrong password (or the file was tampered with).
        wipe(derived);
        return false;
    }

    // The password was right, so a parse error here means the vault is corrupted
    // and is reported as an exception, not as a wrong password.
    try {
        entries = file.version == VAULT_FORMAT_VERSION_1 ? deserialize_entries_v1(payload)
                                                         : deserialize_entries(payload);
    } catch (...) {
        wipe(payload);
        wipe(derived);
        throw;
    }
    wipe(payload);

    key = std::move(derived);
    unlocked = true;
    return true;
}

void Vault::lock() {
    wipe(key);
    wipe(entries);
    unlocked = false;
}

void Vault::save() {
    require_unlocked();

    std::vector<uint8_t> payload = serialize_entries(entries);
    // A new nonce on every save: reusing a nonce with the same key breaks the encryption.
    std::vector<uint8_t> nonce = generate_nonce();
    std::vector<uint8_t> ciphertext = encrypt_bytes(payload, key, nonce);
    wipe(payload);

    VaultFile updated = file;
    updated.version = VAULT_FORMAT_VERSION;  // an old vault is upgraded on its first save
    updated.nonce = std::move(nonce);
    updated.ciphertext = std::move(ciphertext);
    write_vault_file(path, updated);

    // Only update the in-memory copy once the write succeeded.
    file = std::move(updated);
}

void Vault::add_entry(const VaultEntry& entry) {
    require_unlocked();
    if (entry.name.empty()) {
        throw std::runtime_error("entry name cannot be empty");
    }
    if (find_entry(entry.name) != nullptr) {
        throw std::runtime_error("an entry named '" + entry.name + "' already exists");
    }
    VaultEntry added = entry;
    added.created_at = now();
    added.modified_at = added.created_at;
    added.password_history.clear();  // a new entry has no previous passwords
    entries.push_back(std::move(added));
}

void Vault::update_entry(const std::string& name, const VaultEntry& updated) {
    require_unlocked();
    if (updated.name.empty()) {
        throw std::runtime_error("entry name cannot be empty");
    }
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&](const VaultEntry& entry) { return entry.name == name; });
    if (it == entries.end()) {
        throw std::runtime_error("no entry named '" + name + "'");
    }
    if (updated.name != name && find_entry(updated.name) != nullptr) {
        throw std::runtime_error("an entry named '" + updated.name + "' already exists");
    }
    VaultEntry copy = updated;  // copy first in case `updated` refers to *it

    // These are managed here, not by the caller.
    copy.created_at = it->created_at;
    copy.modified_at = content_differs(*it, copy) ? now() : it->modified_at;
    copy.password_history = it->password_history;

    // Remember the replaced password, newest first.
    if (copy.password != it->password && !it->password.empty()) {
        copy.password_history.insert(copy.password_history.begin(),
                                     PasswordHistoryItem{it->password, copy.modified_at});
        while (copy.password_history.size() > MAX_PASSWORD_HISTORY) {
            wipe(copy.password_history.back().password);
            copy.password_history.pop_back();
        }
    }

    wipe(*it);
    *it = std::move(copy);
}

bool Vault::remove_entry(const std::string& name) {
    require_unlocked();
    auto it = std::find_if(entries.begin(), entries.end(),
                           [&](const VaultEntry& entry) { return entry.name == name; });
    if (it == entries.end()) {
        return false;
    }
    wipe(*it);
    entries.erase(it);
    return true;
}

const VaultEntry* Vault::find_entry(const std::string& name) const {
    require_unlocked();
    for (const VaultEntry& entry : entries) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<VaultEntry>& Vault::get_entries() const {
    require_unlocked();
    return entries;
}

void Vault::require_unlocked() const {
    if (!unlocked) {
        throw std::runtime_error("vault is locked");
    }
}
