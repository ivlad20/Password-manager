#include "serialization.h"
#include "byte_io.h"

#include <stdexcept>

std::vector<uint8_t> serialize_entries(const std::vector<VaultEntry>& entries) {
    if (entries.size() > UINT32_MAX) {
        throw std::runtime_error("too many entries");
    }

    std::vector<uint8_t> out;
    append_u32(out, static_cast<uint32_t>(entries.size()));
    for (const VaultEntry& entry : entries) {
        append_string(out, entry.name);
        append_string(out, entry.username);
        append_string(out, entry.password);
        append_string(out, entry.url);
        append_string(out, entry.notes);
    }
    return out;
}

std::vector<VaultEntry> deserialize_entries(const std::vector<uint8_t>& payload) {
    ByteReader reader(payload.data(), payload.size());

    uint32_t count = reader.read_u32();
    // Each entry needs at least five u32 length fields, so a larger count is corrupt.
    if (count > reader.remaining() / 20) {
        throw std::runtime_error("corrupted vault data: invalid entry count");
    }

    std::vector<VaultEntry> entries;
    entries.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        VaultEntry entry;
        entry.name = reader.read_string();
        entry.username = reader.read_string();
        entry.password = reader.read_string();
        entry.url = reader.read_string();
        entry.notes = reader.read_string();
        entries.push_back(std::move(entry));
    }

    if (reader.remaining() != 0) {
        throw std::runtime_error("corrupted vault data: trailing bytes");
    }
    return entries;
}
