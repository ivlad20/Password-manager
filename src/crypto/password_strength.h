#pragma once

#include <string>

// Rough estimate of how hard a password is to guess. It is a guide for the user,
// not a guarantee: it cannot know whether a password was reused or leaked.
struct PasswordStrength {
    int score = 0;       // 0 very weak, 1 weak, 2 fair, 3 strong, 4 very strong
    double bits = 0.0;   // estimated entropy in bits
    std::string label;   // "Very weak" ... "Very strong"
    std::string advice;  // what makes it weak; empty when there is nothing to add
};

constexpr int MAX_PASSWORD_STRENGTH = 4;

// The estimate:
//   - a very common password (or one with only digits/symbols added at the end) is very weak;
//   - otherwise entropy = effective length * log2(size of the character pool), where the pool
//     depends on the kinds of characters used (lowercase, uppercase, digits, symbols, other);
//   - characters that continue a pattern (repeated "aaaa", sequences "1234" / "abcd",
//     neighbours on the keyboard "qwerty") add very little to the effective length;
//   - passwords shorter than 8 characters are at most weak.
PasswordStrength estimate_strength(const std::string& password);
