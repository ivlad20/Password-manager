#include <sodium.h>
#include <iostream>
#include <string>
#include <utility>
#include <vector>


//derive the master key that unlocks all the other passwords, irreversible(works only one way)
std::vector<uint8_t> master_password_derive(const std::string& master_key, const uint8_t* salt) {
    std::vector<uint8_t> key(crypto_secretbox_KEYBYTES);
    int result = crypto_pwhash(
        key.data(), key.size(),
        master_key.c_str(), master_key.length(),
        salt,
        crypto_pwhash_OPSLIMIT_MODERATE,
        crypto_pwhash_MEMLIMIT_MODERATE,
        crypto_pwhash_ALG_ARGON2ID13
        );

    if (result != 0) {
        throw std::runtime_error("crypto_pwhash failed (probabil out of memory)");
    }

    return key;
}

std::vector<uint8_t> generate_nonce() {
    std::vector<uint8_t> nonce(crypto_secretbox_NONCEBYTES);
    randombytes_buf(nonce.data(), nonce.size());
    return nonce;
}

std::vector<uint8_t> encrypt_password(
    const std::string& plaintext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce)
{
    std::vector<uint8_t> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);

    int result = crypto_secretbox_easy(
        ciphertext.data(),
        reinterpret_cast<const uint8_t*>(plaintext.data()),
        plaintext.size(),
        nonce.data(),
        key.data()
    );

    if (result != 0) {
        throw std::runtime_error("crypto_secretbox_easy failed");
    }

    return ciphertext;
}

std::string decrypt_password(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce)
{
    std::vector<uint8_t> output_buffer(ciphertext.size() - crypto_secretbox_MACBYTES);


    int result = crypto_secretbox_open_easy(
        output_buffer.data(),
        ciphertext.data(),
        ciphertext.size(),
        nonce.data(),
        key.data()
    );

    if (result != 0) {
        throw std::runtime_error("decriptare esuata");
    }

    return std::string(output_buffer.begin(), output_buffer.end());
}

void print_hex(const std::string& label, const std::vector<uint8_t>& data) {
    std::vector<char> hex(data.size() * 2 + 1);
    sodium_bin2hex(hex.data(), hex.size(), data.data(), data.size());
    std::cout << label << ": " << hex.data() << std::endl;
}

class Vault_entry {
public:
    Vault_entry(std::string name, std::vector<uint8_t> ciphertext, std::vector<uint8_t> nonce)
        : name(std::move(name)), ciphertext(std::move(ciphertext)), nonce(std::move(nonce)) {}

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
    Vault(std::vector<uint8_t> key, uint8_t salt[crypto_pwhash_SALTBYTES])
        : key(std::move(key)), salt(std::move(salt)) {}


    void unlock_vault() {
        if (master_password_derive(master_password, salt) == key) {
            this->unlocked = true;
        }
    };

    void lock_vault() {
        this->unlocked = false;
    }

    void new_entry(Vault_entry& entry) {
        if (!this->unlocked) {
            entries.emplace_back(entry);
        }
        else {
            throw std::runtime_error("Vault locked");
        }
    }


private:
    std::vector<uint8_t> key;
    std::vector<Vault_entry> entries;
    bool unlocked = false;
    std::string master_password;
    uint8_t salt[crypto_pwhash_SALTBYTES];
    Vault_entry entry;

};

int main() {

    return 0;
}