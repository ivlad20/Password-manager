#include "crypto/crypto.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace {

// The fastest Argon2id settings, so these tests stay quick.
// The real vault uses MODERATE (see Vault::create).
std::vector<uint8_t> fast_derive(const std::string& password, const uint8_t* salt) {
    return master_password_derive(password, salt, crypto_pwhash_OPSLIMIT_MIN, crypto_pwhash_MEMLIMIT_MIN);
}

std::vector<uint8_t> random_key() {
    std::vector<uint8_t> key(crypto_secretbox_KEYBYTES);
    randombytes_buf(key.data(), key.size());
    return key;
}

}  // namespace

TEST(KeyDerivation, SamePasswordAndSaltGiveSameKey) {
    uint8_t salt[crypto_pwhash_SALTBYTES] = {1, 2, 3};
    const auto first = fast_derive("correct horse", salt);
    const auto second = fast_derive("correct horse", salt);
    EXPECT_EQ(first.size(), crypto_secretbox_KEYBYTES);
    EXPECT_EQ(first, second);
}

TEST(KeyDerivation, DifferentPasswordOrSaltGiveDifferentKey) {
    uint8_t salt[crypto_pwhash_SALTBYTES] = {1, 2, 3};
    uint8_t other_salt[crypto_pwhash_SALTBYTES] = {9, 9, 9};
    const auto key = fast_derive("correct horse", salt);
    EXPECT_NE(key, fast_derive("correct horsE", salt));
    EXPECT_NE(key, fast_derive("correct horse", other_salt));
}

TEST(Nonce, HasRightSizeAndIsRandom) {
    const auto a = generate_nonce();
    const auto b = generate_nonce();
    EXPECT_EQ(a.size(), crypto_secretbox_NONCEBYTES);
    EXPECT_NE(a, b);
}

TEST(Encryption, RoundTrip) {
    const auto key = random_key();
    const auto nonce = generate_nonce();
    const std::vector<uint8_t> plaintext = {'s', 'e', 'c', 'r', 'e', 't', 0, 255};

    const auto ciphertext = encrypt_bytes(plaintext, key, nonce);
    EXPECT_EQ(ciphertext.size(), plaintext.size() + crypto_secretbox_MACBYTES);
    EXPECT_EQ(decrypt_bytes(ciphertext, key, nonce), plaintext);
}

TEST(Encryption, PasswordRoundTrip) {
    const auto key = random_key();
    const auto nonce = generate_nonce();
    const auto ciphertext = encrypt_password("SuperSecret123!", key, nonce);
    EXPECT_EQ(decrypt_password(ciphertext, key, nonce), "SuperSecret123!");
}

TEST(Encryption, EmptyPlaintextRoundTrip) {
    const auto key = random_key();
    const auto nonce = generate_nonce();
    const auto ciphertext = encrypt_bytes({}, key, nonce);
    EXPECT_TRUE(decrypt_bytes(ciphertext, key, nonce).empty());
}

TEST(Encryption, WrongKeyFails) {
    const auto nonce = generate_nonce();
    const auto ciphertext = encrypt_password("secret", random_key(), nonce);
    EXPECT_THROW(decrypt_password(ciphertext, random_key(), nonce), std::runtime_error);
}

TEST(Encryption, WrongNonceFails) {
    const auto key = random_key();
    const auto ciphertext = encrypt_password("secret", key, generate_nonce());
    EXPECT_THROW(decrypt_password(ciphertext, key, generate_nonce()), std::runtime_error);
}

// Poly1305 authentication: changing even one bit is detected.
TEST(Encryption, TamperedCiphertextFails) {
    const auto key = random_key();
    const auto nonce = generate_nonce();
    auto ciphertext = encrypt_password("secret", key, nonce);
    for (size_t i = 0; i < ciphertext.size(); ++i) {
        auto tampered = ciphertext;
        tampered[i] ^= 1;
        EXPECT_THROW(decrypt_bytes(tampered, key, nonce), std::runtime_error) << "byte " << i;
    }
}

TEST(Encryption, TooShortCiphertextFails) {
    const std::vector<uint8_t> too_short(crypto_secretbox_MACBYTES - 1);
    EXPECT_THROW(decrypt_bytes(too_short, random_key(), generate_nonce()), std::runtime_error);
}
