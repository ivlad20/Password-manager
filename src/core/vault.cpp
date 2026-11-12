//
// Created by vladi on 11/12/2026.
//

#include "vault.h"
#include "../crypto/crypto.h"   // for master_password_derive
#include <stdexcept>
#include <utility>

Vault_entry::Vault_entry(std::string name, std::vector<uint8_t> ciphertext, std::vector<uint8_t> nonce)
    : name(std::move(name)), ciphertext(std::move(ciphertext)), nonce(std::move(nonce)) {}

Vault::Vault(std::vector<uint8_t> key, const Salt& salt)
    : key(std::move(key)), salt(salt) {}

bool Vault::unlock_vault(const std::string& master_password) {
    std::vector<uint8_t> derived = master_password_derive(master_password, salt.data());
    bool match = derived.size() == key.size() &&
                 sodium_memcmp(derived.data(), key.data(), key.size()) == 0;
    sodium_memzero(derived.data(), derived.size());
    unlocked = match;
    return match;
}

void Vault::lock_vault() {
    unlocked = false;
}

void Vault::new_entry(const Vault_entry& entry) {
    if (!unlocked) {
        throw std::runtime_error("Vault locked");
    }
    entries.emplace_back(entry);
}