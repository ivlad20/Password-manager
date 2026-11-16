#include "serialization.h"
#include "byte_io.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

// Field ids of the version 2 payload. Never reuse or renumber an id: files written by
// older versions of the app must keep their meaning.
enum FieldId : uint16_t {
    FIELD_NAME = 1,
    FIELD_USERNAME = 2,
    FIELD_PASSWORD = 3,
    FIELD_URL = 4,
    FIELD_NOTES = 5,
    FIELD_CREATED_AT = 6,
    FIELD_MODIFIED_AT = 7,
    FIELD_FAVORITE = 8,
    FIELD_TAG = 9,
    FIELD_HISTORY_ITEM = 10,
};

[[noreturn]] void corrupted(const std::string& reason) {
    throw std::runtime_error("corrupted vault data: " + reason);
}

void check_size(size_t size) {
    if (size > UINT32_MAX) {
        throw std::runtime_error("vault data too large to serialize");
    }
}

void append_field(std::vector<uint8_t>& out, uint16_t id, const uint8_t* data, size_t size) {
    check_size(size);
    append_u16(out, id);
    append_u32(out, static_cast<uint32_t>(size));
    append_bytes(out, data, size);
}

void append_text_field(std::vector<uint8_t>& out, uint16_t id, const std::string& value) {
    append_field(out, id, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

void append_u64_field(std::vector<uint8_t>& out, uint16_t id, uint64_t value) {
    std::vector<uint8_t> bytes;
    append_u64(bytes, value);
    append_field(out, id, bytes.data(), bytes.size());
}

std::vector<uint8_t> serialize_entry(const VaultEntry& entry) {
    std::vector<uint8_t> out;
    append_text_field(out, FIELD_NAME, entry.name);
    append_text_field(out, FIELD_USERNAME, entry.username);
    append_text_field(out, FIELD_PASSWORD, entry.password);
    append_text_field(out, FIELD_URL, entry.url);
    append_text_field(out, FIELD_NOTES, entry.notes);
    append_u64_field(out, FIELD_CREATED_AT, entry.created_at);
    append_u64_field(out, FIELD_MODIFIED_AT, entry.modified_at);

    const uint8_t favorite = entry.favorite ? 1 : 0;
    append_field(out, FIELD_FAVORITE, &favorite, 1);

    for (const std::string& tag : entry.tags) {
        append_text_field(out, FIELD_TAG, tag);
    }
    for (const PasswordHistoryItem& item : entry.password_history) {
        std::vector<uint8_t> bytes;
        append_u64(bytes, item.changed_at);
        append_bytes(bytes, reinterpret_cast<const uint8_t*>(item.password.data()), item.password.size());
        append_field(out, FIELD_HISTORY_ITEM, bytes.data(), bytes.size());
        std::fill(bytes.begin(), bytes.end(), uint8_t{0});  // it held an old password
    }
    return out;
}

uint64_t read_u64_field(ByteReader& field) {
    if (field.remaining() != 8) {
        corrupted("invalid timestamp field");
    }
    return field.read_u64();
}

VaultEntry deserialize_entry(ByteReader& reader) {
    VaultEntry entry;
    while (reader.remaining() > 0) {
        const uint16_t id = reader.read_u16();
        const uint32_t size = reader.read_u32();
        if (size > reader.remaining()) {
            corrupted("field longer than its entry");
        }
        ByteReader field = reader.read_block(size);

        switch (id) {
            case FIELD_NAME: entry.name = field.read_raw_string(size); break;
            case FIELD_USERNAME: entry.username = field.read_raw_string(size); break;
            case FIELD_PASSWORD: entry.password = field.read_raw_string(size); break;
            case FIELD_URL: entry.url = field.read_raw_string(size); break;
            case FIELD_NOTES: entry.notes = field.read_raw_string(size); break;
            case FIELD_CREATED_AT: entry.created_at = read_u64_field(field); break;
            case FIELD_MODIFIED_AT: entry.modified_at = read_u64_field(field); break;
            case FIELD_FAVORITE: {
                if (size != 1) {
                    corrupted("invalid favorite field");
                }
                const uint8_t value = field.read_u8();
                if (value > 1) {
                    corrupted("invalid favorite field");
                }
                entry.favorite = value == 1;
                break;
            }
            case FIELD_TAG: entry.tags.push_back(field.read_raw_string(size)); break;
            case FIELD_HISTORY_ITEM: {
                if (size < 8) {
                    corrupted("invalid password history field");
                }
                PasswordHistoryItem item;
                item.changed_at = field.read_u64();
                item.password = field.read_raw_string(field.remaining());
                entry.password_history.push_back(std::move(item));
                break;
            }
            default:
                // Written by a newer version of the app: keep reading, ignore the field.
                break;
        }
    }

    if (entry.password_history.size() > MAX_PASSWORD_HISTORY) {
        entry.password_history.resize(MAX_PASSWORD_HISTORY);
    }
    return entry;
}

}  // namespace

std::vector<uint8_t> serialize_entries(const std::vector<VaultEntry>& entries) {
    check_size(entries.size());

    std::vector<uint8_t> out;
    append_u32(out, static_cast<uint32_t>(entries.size()));
    for (const VaultEntry& entry : entries) {
        std::vector<uint8_t> bytes = serialize_entry(entry);
        check_size(bytes.size());
        append_u32(out, static_cast<uint32_t>(bytes.size()));
        append_bytes(out, bytes.data(), bytes.size());
        std::fill(bytes.begin(), bytes.end(), uint8_t{0});  // plaintext copy of the entry
    }
    return out;
}

std::vector<VaultEntry> deserialize_entries(const std::vector<uint8_t>& payload) {
    ByteReader reader(payload.data(), payload.size());

    const uint32_t count = reader.read_u32();
    // Each entry needs at least its u32 size, so a larger count is corrupt.
    if (count > reader.remaining() / 4) {
        corrupted("invalid entry count");
    }

    std::vector<VaultEntry> entries;
    entries.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t size = reader.read_u32();
        if (size > reader.remaining()) {
            corrupted("entry longer than the data");
        }
        ByteReader entry_reader = reader.read_block(size);
        entries.push_back(deserialize_entry(entry_reader));
    }

    if (reader.remaining() != 0) {
        corrupted("trailing bytes");
    }
    return entries;
}

std::vector<VaultEntry> deserialize_entries_v1(const std::vector<uint8_t>& payload) {
    ByteReader reader(payload.data(), payload.size());

    const uint32_t count = reader.read_u32();
    // Each entry needs at least five u32 length fields, so a larger count is corrupt.
    if (count > reader.remaining() / 20) {
        corrupted("invalid entry count");
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
        corrupted("trailing bytes");
    }
    return entries;
}
