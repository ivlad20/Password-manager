//
// Created by vladi on 11/12/2026.
//

#pragma once

#include <sodium.h>          // needed here for crypto_pwhash_SALTBYTES
#include <array>
#include <cstdint>
#include <string>
#include <vector>

using Salt = std::array<uint8_t, crypto_pwhash_SALTBYTES>;

class Vault_entry {
public:
    Vault_entry(std::string name, std::vector<uint8_t> ciphertext, std::vector<uint8_t> nonce);

    [[nodiscard]] const std::string& getName() const { return name; }
    [[nodiscard]] const std::vector<uint8_t>& getCiphertext() const { return ciphertext; }
    [[nodiscard]] const std::vector<uint8_t>& getNonce() const { return nonce; }

private:
    std::string name;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> nonce;
};

class Vault {
public:
    Vault(std::vector<uint8_t> key, const Salt& salt);

    bool unlock_vault(const std::string& master_password);
    void lock_vault();
    void new_entry(const Vault_entry& entry);

private:
    std::vector<uint8_t> key;
    std::vector<Vault_entry> entries;
    bool unlocked = false;
    Salt salt;
};