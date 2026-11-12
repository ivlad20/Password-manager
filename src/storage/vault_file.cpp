#include "vault_file.h"
#include "byte_io.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

static const uint8_t MAGIC[4] = {'P', 'W', 'M', 'V'};
static const uint32_t FORMAT_VERSION = 1;

void write_vault_file(const std::string& path, const VaultFile& file) {
    std::vector<uint8_t> out;
    append_bytes(out, MAGIC, sizeof(MAGIC));
    append_u32(out, FORMAT_VERSION);
    append_u64(out, file.opslimit);
    append_u64(out, file.memlimit);
    append_bytes(out, file.salt.data(), file.salt.size());
    append_bytes(out, file.nonce.data(), file.nonce.size());
    append_bytes(out, file.ciphertext.data(), file.ciphertext.size());

    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream stream(tmp_path, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("cannot open " + tmp_path + " for writing");
        }
        stream.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
        stream.flush();
        if (!stream) {
            throw std::runtime_error("failed to write " + tmp_path);
        }
    }

    // Replaces the old vault in one step.
    std::filesystem::rename(tmp_path, path);
}

VaultFile read_vault_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot open " + path);
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    ByteReader reader(data.data(), data.size());

    uint8_t magic[4];
    reader.read_bytes(magic, sizeof(magic));
    if (std::memcmp(magic, MAGIC, sizeof(MAGIC)) != 0) {
        throw std::runtime_error(path + " is not a vault file");
    }

    uint32_t version = reader.read_u32();
    if (version != FORMAT_VERSION) {
        throw std::runtime_error("unsupported vault format version " + std::to_string(version));
    }

    VaultFile file;
    file.opslimit = reader.read_u64();
    file.memlimit = reader.read_u64();

    // Reject absurd KDF parameters so a tampered file cannot make us
    // allocate gigabytes of memory or hang while deriving the key.
    if (file.opslimit < crypto_pwhash_OPSLIMIT_MIN || file.opslimit > crypto_pwhash_OPSLIMIT_SENSITIVE ||
        file.memlimit < crypto_pwhash_MEMLIMIT_MIN || file.memlimit > crypto_pwhash_MEMLIMIT_SENSITIVE) {
        throw std::runtime_error("invalid key derivation parameters in vault file");
    }

    reader.read_bytes(file.salt.data(), file.salt.size());

    file.nonce.resize(crypto_secretbox_NONCEBYTES);
    reader.read_bytes(file.nonce.data(), file.nonce.size());

    file.ciphertext = reader.read_rest();
    if (file.ciphertext.size() < crypto_secretbox_MACBYTES) {
        throw std::runtime_error("vault file is truncated");
    }

    return file;
}
