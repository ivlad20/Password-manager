#include "storage/serialization.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace {

bool same(const VaultEntry& a, const VaultEntry& b) {
    return a.name == b.name && a.username == b.username && a.password == b.password &&
           a.url == b.url && a.notes == b.notes;
}

}  // namespace

TEST(Serialization, EmptyListRoundTrip) {
    EXPECT_TRUE(deserialize_entries(serialize_entries({})).empty());
}

TEST(Serialization, EntriesRoundTrip) {
    const std::vector<VaultEntry> entries = {
            {"Gmail", "me@example.com", "SuperSecret123!", "https://mail.google.com", "personal"},
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

TEST(Serialization, RejectsTruncatedPayload) {
    const auto payload = serialize_entries({{"Gmail", "me", "pass", "url", "notes"}});
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
}

TEST(Serialization, RejectsStringLongerThanPayload) {
    auto payload = serialize_entries({{"Gmail", "", "", "", ""}});
    // The first string length comes right after the u32 entry count.
    payload[4] = 0xFF;
    payload[5] = 0xFF;
    EXPECT_THROW(deserialize_entries(payload), std::runtime_error);
}
