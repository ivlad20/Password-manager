#include "crypto.h"
#include <sodium.h>
#include <iostream>
#include <stdexcept>

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
        throw std::runtime_error("crypto_pwhash failed (probably out of memory)");
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
    if (ciphertext.size() < crypto_secretbox_MACBYTES) {
        throw std::runtime_error("ciphertext is too short");
    }

    std::vector<uint8_t> output_buffer(ciphertext.size() - crypto_secretbox_MACBYTES);


    int result = crypto_secretbox_open_easy(
        output_buffer.data(),
        ciphertext.data(),
        ciphertext.size(),
        nonce.data(),
        key.data()
    );

    if (result != 0) {
        throw std::runtime_error("decryption failed");
    }

    return std::string(output_buffer.begin(), output_buffer.end());
}

void print_hex(const std::string& label, const std::vector<uint8_t>& data) {
    std::vector<char> hex(data.size() * 2 + 1);
    sodium_bin2hex(hex.data(), hex.size(), data.data(), data.size());
    std::cout << label << ": " << hex.data() << std::endl;
}