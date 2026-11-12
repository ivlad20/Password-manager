#include "crypto/password_generator.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace {

bool has(const std::string& s, int (*predicate)(int)) {
    return std::any_of(s.begin(), s.end(), [&](char c) { return predicate(static_cast<unsigned char>(c)) != 0; });
}

int is_symbol(int c) { return std::isprint(c) && !std::isalnum(c) && c != ' '; }

}  // namespace

TEST(PasswordGenerator, DefaultLength) {
    EXPECT_EQ(generate_password().size(), DEFAULT_PASSWORD_LENGTH);
}

TEST(PasswordGenerator, RespectsRequestedLength) {
    for (size_t length : {MIN_PASSWORD_LENGTH, size_t{16}, size_t{64}, MAX_PASSWORD_LENGTH}) {
        EXPECT_EQ(generate_password(length).size(), length);
    }
}

// Run many times: a missing character class would only show up occasionally.
TEST(PasswordGenerator, ContainsEveryCharacterClass) {
    for (int i = 0; i < 500; ++i) {
        const std::string password = generate_password(MIN_PASSWORD_LENGTH);
        EXPECT_TRUE(has(password, std::islower)) << password;
        EXPECT_TRUE(has(password, std::isupper)) << password;
        EXPECT_TRUE(has(password, std::isdigit)) << password;
        EXPECT_TRUE(has(password, is_symbol)) << password;
        EXPECT_FALSE(has(password, std::isspace)) << password;
    }
}

TEST(PasswordGenerator, PasswordsAreDifferent) {
    std::set<std::string> seen;
    for (int i = 0; i < 100; ++i) {
        seen.insert(generate_password());
    }
    EXPECT_EQ(seen.size(), 100u);
}

TEST(PasswordGenerator, RejectsInvalidLength) {
    EXPECT_THROW(generate_password(MIN_PASSWORD_LENGTH - 1), std::invalid_argument);
    EXPECT_THROW(generate_password(MAX_PASSWORD_LENGTH + 1), std::invalid_argument);
    EXPECT_THROW(generate_password(0), std::invalid_argument);
}
