#include "crypto/password_generator.h"
#include "crypto/password_strength.h"

#include <gtest/gtest.h>

TEST(PasswordStrength, EmptyIsVeryWeak) {
    const PasswordStrength strength = estimate_strength("");
    EXPECT_EQ(strength.score, 0);
    EXPECT_EQ(strength.label, "Very weak");
}

TEST(PasswordStrength, CommonPasswordsAreVeryWeak) {
    for (const char* password : {"password", "PASSWORD", "Password1!", "p@ssw0rd", "iloveyou2024", "dragon"}) {
        const PasswordStrength strength = estimate_strength(password);
        EXPECT_EQ(strength.score, 0) << password;
        EXPECT_FALSE(strength.advice.empty()) << password;
    }
}

TEST(PasswordStrength, PatternsAreWeak) {
    for (const char* password : {"12345678", "qwertyui", "abcdefgh", "aaaaaaaaaaaa", "98765432", "asdfghjkl;"}) {
        EXPECT_LE(estimate_strength(password).score, 1) << password;
    }
}

TEST(PasswordStrength, ShortPasswordsAreAtMostWeak) {
    EXPECT_LE(estimate_strength("Xk#9q!").score, 1);
    EXPECT_FALSE(estimate_strength("Xk#9q!").advice.empty());
}

TEST(PasswordStrength, LongerAndMixedIsStronger) {
    const int lowercase = estimate_strength("mfkzrtbq").score;
    const int mixed = estimate_strength("mFk7zR!tbQ").score;
    const int long_mixed = estimate_strength("mFk7zR!tbQ#w2Lp9").score;
    EXPECT_LT(lowercase, mixed);
    EXPECT_LE(mixed, long_mixed);
    EXPECT_EQ(long_mixed, MAX_PASSWORD_STRENGTH);
}

TEST(PasswordStrength, PassphraseIsStrong) {
    EXPECT_GE(estimate_strength("correct horse battery staple").score, 3);
}

TEST(PasswordStrength, GeneratedPasswordsAreVeryStrong) {
    for (int i = 0; i < 20; ++i) {
        const std::string password = generate_password(DEFAULT_PASSWORD_LENGTH);
        EXPECT_EQ(estimate_strength(password).score, MAX_PASSWORD_STRENGTH) << password;
    }
}

TEST(PasswordStrength, NonAsciiCharactersCountOnce) {
    // "ș" is two bytes in UTF-8 but one character: 7 characters is still too short.
    EXPECT_LE(estimate_strength("\xC8\x99\xC8\x99" "Abc1!").score, 1);
    EXPECT_GE(estimate_strength("\xC8\x99tefan-M\xC4\x83rie-9!").score, 3);
}
