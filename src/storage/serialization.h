#pragma once

#include "../core/vault_entry.h"

#include <cstdint>
#include <vector>

// Payload layout, format version 2 (before encryption):
//   entry count u32, then for each entry:
//     entry size u32 (bytes that follow for this entry), then a list of fields:
//       field id u16 | field size u32 | field bytes
//
// Fields (see FieldId in serialization.cpp):
//   1 name, 2 username, 3 password, 4 url, 5 notes   UTF-8 text
//   6 created_at, 7 modified_at                       u64, Unix time in seconds
//   8 favorite                                        u8, 0 or 1
//   9 tag                                             UTF-8 text, repeated once per tag
//   10 password history item                          u64 changed_at + the old password,
//                                                     repeated, newest first
//
// Unknown field ids are skipped when reading, so a newer version of the app can add
// fields without breaking older ones. Absent fields keep their default value.
std::vector<uint8_t> serialize_entries(const std::vector<VaultEntry>& entries);

// Reads a format version 2 payload. Throws std::runtime_error if it is malformed.
std::vector<VaultEntry> deserialize_entries(const std::vector<uint8_t>& payload);

// Reads a format version 1 payload (entry count u32, then for each entry five
// length-prefixed strings: name, username, password, url, notes).
// The fields added in version 2 get their default values.
std::vector<VaultEntry> deserialize_entries_v1(const std::vector<uint8_t>& payload);
