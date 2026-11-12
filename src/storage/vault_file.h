#pragma once

#include <sodium.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

using Salt = std::array<uint8_t, crypto_pwhash_SALTBYTES>;

// On-disk layout (all integers little-endian):
//   magic "PWMV" (4) | version u32 | opslimit u64 | memlimit u64 |
//   salt (16) | nonce (24) | ciphertext (rest of the file)
// Only the ciphertext is secret; the header is what is needed to re-derive the key.
struct VaultFile {
    uint64_t opslimit = 0;
    uint64_t memlimit = 0;
    Salt salt{};
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> ciphertext;
};

// Writes to "<path>.tmp" first and then renames it over the real file,
// so a crash in the middle of a save never leaves a half-written vault.
void write_vault_file(const std::string& path, const VaultFile& file);

// Throws std::runtime_error if the file is missing, truncated or not a vault.
VaultFile read_vault_file(const std::string& path);
