#include "storage/vault_file.h"
#include "test_helpers.h"

#include <filesystem>
#include <stdexcept>

namespace {

// magic (4) + version (4) + opslimit (8) + memlimit (8) + salt (16) + nonce (24)
constexpr size_t HEADER_SIZE = 64;

VaultFile sample_file() {
    VaultFile file;
    file.opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
    file.memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
    randombytes_buf(file.salt.data(), file.salt.size());
    file.nonce.resize(crypto_secretbox_NONCEBYTES);
    randombytes_buf(file.nonce.data(), file.nonce.size());
    file.ciphertext.resize(crypto_secretbox_MACBYTES + 10);
    randombytes_buf(file.ciphertext.data(), file.ciphertext.size());
    return file;
}

}  // namespace

class VaultFileTest : public TempDirTest {};

TEST_F(VaultFileTest, WriteThenReadRoundTrip) {
    const VaultFile original = sample_file();
    write_vault_file(path("v.pwm"), original);

    const VaultFile loaded = read_vault_file(path("v.pwm"));
    EXPECT_EQ(loaded.opslimit, original.opslimit);
    EXPECT_EQ(loaded.memlimit, original.memlimit);
    EXPECT_EQ(loaded.salt, original.salt);
    EXPECT_EQ(loaded.nonce, original.nonce);
    EXPECT_EQ(loaded.ciphertext, original.ciphertext);
}

TEST_F(VaultFileTest, LayoutStartsWithMagicAndVersion) {
    const VaultFile original = sample_file();
    write_vault_file(path("v.pwm"), original);

    const std::string raw = read_file(path("v.pwm"));
    EXPECT_EQ(raw.size(), HEADER_SIZE + original.ciphertext.size());
    EXPECT_EQ(raw.substr(0, 4), "PWMV");
    EXPECT_EQ(raw.substr(4, 4), std::string("\x01\x00\x00\x00", 4));  // version 1, little-endian
}

// The atomic save writes a temporary file first; it must not be left behind.
TEST_F(VaultFileTest, OverwriteLeavesNoTemporaryFile) {
    write_vault_file(path("v.pwm"), sample_file());
    const VaultFile second = sample_file();
    write_vault_file(path("v.pwm"), second);

    EXPECT_FALSE(std::filesystem::exists(path("v.pwm.tmp")));
    EXPECT_EQ(read_vault_file(path("v.pwm")).ciphertext, second.ciphertext);
}

TEST_F(VaultFileTest, MissingFileThrows) {
    EXPECT_THROW(read_vault_file(path("missing.pwm")), std::runtime_error);
}

TEST_F(VaultFileTest, WrongMagicThrows) {
    write_vault_file(path("v.pwm"), sample_file());
    std::string raw = read_file(path("v.pwm"));
    raw[0] = 'X';
    write_file(path("v.pwm"), raw);
    EXPECT_THROW(read_vault_file(path("v.pwm")), std::runtime_error);
}

TEST_F(VaultFileTest, UnknownVersionThrows) {
    write_vault_file(path("v.pwm"), sample_file());
    std::string raw = read_file(path("v.pwm"));
    raw[4] = 2;
    write_file(path("v.pwm"), raw);
    EXPECT_THROW(read_vault_file(path("v.pwm")), std::runtime_error);
}

// A tampered header must not be able to request gigabytes of memory for Argon2.
TEST_F(VaultFileTest, AbsurdKeyDerivationLimitsThrow) {
    VaultFile file = sample_file();
    file.memlimit = crypto_pwhash_MEMLIMIT_SENSITIVE + 1;
    write_vault_file(path("v.pwm"), file);
    EXPECT_THROW(read_vault_file(path("v.pwm")), std::runtime_error);

    file = sample_file();
    file.opslimit = 0;
    write_vault_file(path("v.pwm"), file);
    EXPECT_THROW(read_vault_file(path("v.pwm")), std::runtime_error);
}

TEST_F(VaultFileTest, TruncatedFileThrows) {
    write_vault_file(path("v.pwm"), sample_file());
    const std::string raw = read_file(path("v.pwm"));
    // Anything shorter than the header plus the 16-byte authentication tag is invalid.
    for (size_t size : {size_t{0}, size_t{3}, size_t{30}, HEADER_SIZE, HEADER_SIZE + crypto_secretbox_MACBYTES - 1}) {
        write_file(path("short.pwm"), raw.substr(0, size));
        EXPECT_THROW(read_vault_file(path("short.pwm")), std::runtime_error) << "size " << size;
    }
}
