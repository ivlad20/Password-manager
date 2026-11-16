#include "storage/byte_io.h"
#include "storage/serialization.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace {

bool same(const VaultEntry& a, const VaultEntry& b) {
    if (a.password_history.size() != b.password_history.size()) {
        return false;
    }
    for (size_t i = 0; i < a.password_history.size(); ++i) {
        if (a.password_history[i].password != b.password_history[i].password ||
            a.password_history[i].changed_at != b.password_history[i].changed_at) {
            return false;
        }
    }
    return a.name == b.name && a.username == b.username && a.password == b.password &&
           a.url == b.url && a.notes == b.notes && a.created_at == b.created_at &&
           a.modified_at == b.modified_at && a.favorite == b.favorite && a.tags == b.tags;
}

VaultEntry full_entry() {
    VaultEntry entry{"Gmail", "me@example.com", "SuperSecret123!", "https://mail.google.com", "personal"};
    entry.created_at = 1700000000;
    entry.modified_at = 1800000000;
    entry.favorite = true;
    entry.tags = {"email", "personal", "Școală"};
    entry.password_history = {{"older-pass", 1750000000}, {std::string("x\0y", 3), 1720000000}};
    return entry;
}

// Builds a v2 payload with a single entry made of the given raw fields.
std::vector<uint8_t> payload_with_fields(const std::vector<uint8_t>& fields) {
    std::vector<uint8_t> out;
    append_u32(out, 1);
    append_u32(out, static_cast<uint32_t>(fields.size()));
    append_bytes(out, fields.data(), fields.size());
    return out;
}

void append_field(std::vector<uint8_t>& out, uint16_t id, const std::string& value) {
    append_u16(out, id);
    append_u32(out, static_cast<uint32_t>(value.size()));
    append_bytes(out, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

}  // namespace

TEST(Serialization, EmptyListRoundTrip) {
    EXPECT_TRUE(deserialize_entries(serialize_entries({})).empty());
}

TEST(Serialization, EntriesRoundTrip) {
    const std::vector<VaultEntry> entries = {
            full_entry(),
            {"bank", "", "", "", ""},
            // Non-ASCII text, embedded zero bytes and new lines must survive unchanged.
            {"Ștefan's café", "user", std::string("a\0b", 3), "", "line 1\nline 2"},
    };
    const auto decoded = deserialize_entries(serialize_entries(entries));
    ASSERT_EQ(decoded.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        EXPECT_TRUE(same(decoded[i], entries[i])) << "entry " << i;
    }
}

TEST(Serialization, NewFieldsDefaultWhenAbsent) {
    std::vector<uint8_t> fields;
    append_field(fields, 1, "only a name");
    const auto decoded = deserialize_entries(payload_with_fields(fields));
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].name, "only a name");
    EXPECT_EQ(decoded[0].created_at, 0u);
    EXPECT_FALSE(decoded[0].favorite);
    EXPECT_TRUE(decoded[0].tags.empty());
    EXPECT_TRUE(decoded[0].password_history.empty());
}

// A field added by a newer version of the app must not break reading.
TEST(Serialization, UnknownFieldsAreSkipped) {
    std::vector<uint8_t> fields;
    append_field(fields, 1, "Gmail");
    append_field(fields, 999, "something from the future");
    append_field(fields, 3, "pass");
    const auto decoded = deserialize_entries(payload_with_fields(fields));
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].name, "Gmail");
    EXPECT_EQ(decoded[0].password, "pass");
}

TEST(Serialization, RejectsMalformedFixedSizeFields) {
    for (uint16_t id : {uint16_t{6}, uint16_t{7}, uint16_t{8}, uint16_t{10}}) {
        std::vector<uint8_t> fields;
        append_field(fields, id, "abc");  // wrong size for a timestamp, flag or history item
        EXPECT_THROW(deserialize_entries(payload_with_fields(fields)), std::runtime_error) << "field " << id;
    }
    std::vector<uint8_t> fields;
    append_field(fields, 8, std::string(1, '\x02'));  // favorite must be 0 or 1
    EXPECT_THROW(deserialize_entries(payload_with_fields(fields)), std::runtime_error);
}

TEST(Serialization, RejectsTruncatedPayload) {
    const auto payload = serialize_entries({full_entry()});
    for (size_t size = 0; size < payload.size(); ++size) {
        const std::vector<uint8_t> truncated(payload.begin(), payload.begin() + static_cast<long>(size));
        EXPECT_THROW(deserialize_entries(truncated), std::runtime_error) << "size " << size;
    }
}

TEST(Serialization, RejectsTrailingBytes) {
    auto payload = serialize_entries({{"Gmail", "me", "pass", "", ""}});
    payload.push_back(0);
    EXPECT_THROW(deserialize_entries(payload), std::runtime_error);
}

// A corrupted count must not make the reader allocate huge amounts of memory.
TEST(Serialization, RejectsImpossibleEntryCount) {
    const std::vector<uint8_t> payload = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_THROW(deserialize_entries(payload), std::runtime_error);
    EXPECT_THROW(deserialize_entries_v1(payload), std::runtime_error);
}

TEST(Serialization, RejectsFieldLongerThanEntry) {
    auto payload = serialize_entries({{"Gmail", "", "", "", ""}});
    // Layout: entry count u32 | entry size u32 | first field id u16 | first field size u32
    payload[10] = 0xFF;
    payload[11] = 0xFF;
    EXPECT_THROW(deserialize_entries(payload), std::runtime_error);
}

TEST(Serialization, RejectsEntryLongerThanPayload) {
    auto payload = serialize_entries({{"Gmail", "", "", "", ""}});
    payload[4] = 0xFF;
    payload[5] = 0xFF;
    EXPECT_THROW(deserialize_entries(payload), std::runtime_error);
}

TEST(Serialization, ReadsVersion1Payload) {
    std::vector<uint8_t> payload;
    append_u32(payload, 1);
    for (const std::string& value : {"Gmail", "me", "pass", "https://mail.google.com", "notes"}) {
        append_string(payload, value);
    }
    const auto decoded = deserialize_entries_v1(payload);
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_TRUE(same(decoded[0], {"Gmail", "me", "pass", "https://mail.google.com", "notes"}));

    payload.push_back(0);
    EXPECT_THROW(deserialize_entries_v1(payload), std::runtime_error);
}
