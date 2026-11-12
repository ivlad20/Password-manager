#pragma once

#include "../core/vault_entry.h"

#include <cstdint>
#include <vector>

// Payload layout (before encryption):
//   entry count u32, then for each entry five length-prefixed strings:
//   name, username, password, url, notes
std::vector<uint8_t> serialize_entries(const std::vector<VaultEntry>& entries);

// Throws std::runtime_error if the payload is malformed.
std::vector<VaultEntry> deserialize_entries(const std::vector<uint8_t>& payload);
