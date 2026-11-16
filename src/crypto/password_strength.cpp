#include "password_strength.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace {

// Some of the most used passwords from public leak statistics, in lowercase.
// Shorter variants ("pass", "admin") also catch the same word with digits added.
constexpr std::string_view COMMON_PASSWORDS[] = {
        "password", "passw0rd", "p@ssword", "p@ssw0rd", "pass", "passwort", "parola", "motdepasse",
        "123456", "1234567", "12345678", "123456789", "1234567890", "12345", "1234", "111111",
        "000000", "123123", "654321", "666666", "121212", "112233", "123321", "159753",
        "qwerty", "qwertyuiop", "qwerty123", "azerty", "asdfgh", "asdfghjkl", "zxcvbnm", "1q2w3e4r",
        "1qaz2wsx", "qazwsx", "q1w2e3r4", "abc123", "abcdef", "abcd1234", "a1b2c3", "aaaaaa",
        "iloveyou", "love", "lovely", "princess", "sunshine", "shadow", "dragon", "monkey",
        "football", "baseball", "soccer", "hockey", "basketball", "superman", "batman", "pokemon",
        "letmein", "welcome", "welcome1", "hello", "hello123", "freedom", "whatever", "trustno1",
        "master", "admin", "administrator", "root", "login", "guest", "test", "user",
        "secret", "default", "changeme", "access", "starwars", "matrix", "computer", "internet",
        "michael", "jennifer", "jordan", "daniel", "andrew", "charlie", "thomas", "jessica",
        "ashley", "nicole", "michelle", "hunter", "ranger", "buster", "tigger", "ginger",
        "summer", "winter", "spring", "autumn", "flower", "cheese", "cookie", "chocolate",
        "maverick", "mustang", "ferrari", "corvette", "harley", "yankees", "liverpool", "chelsea",
        "google", "facebook", "samsung", "apple", "microsoft", "windows", "linux", "ubuntu",
};

constexpr std::array<std::string_view, 4> KEYBOARD_ROWS = {
        "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm",
};

// Weight of a character that continues a pattern, compared with 1 for a normal one.
constexpr double PATTERN_WEIGHT = 0.2;

char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool is_ascii(char c) {
    return static_cast<unsigned char>(c) < 0x80;
}

// UTF-8 continuation bytes belong to the previous character.
bool is_continuation_byte(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

bool keyboard_neighbours(char a, char b) {
    for (std::string_view row : KEYBOARD_ROWS) {
        const size_t i = row.find(a);
        const size_t j = row.find(b);
        if (i != std::string_view::npos && j != std::string_view::npos &&
            (i + 1 == j || j + 1 == i)) {
            return true;
        }
    }
    return false;
}

// True if `current` continues a pattern started by `previous`: the same character,
// the next/previous letter or digit, or a neighbouring key on the same keyboard row.
bool continues_pattern(char previous, char current) {
    if (!is_ascii(previous) || !is_ascii(current)) {
        return false;
    }
    const char a = lower(previous);
    const char b = lower(current);
    if (a == b) {
        return true;
    }
    const bool same_kind = (std::isalpha(static_cast<unsigned char>(a)) && std::isalpha(static_cast<unsigned char>(b))) ||
                           (std::isdigit(static_cast<unsigned char>(a)) && std::isdigit(static_cast<unsigned char>(b)));
    if (same_kind && std::abs(a - b) == 1) {
        return true;
    }
    return keyboard_neighbours(a, b);
}

// A common password, possibly with digits or symbols around it ("Password1!", "2024qwerty").
bool is_common(const std::string& password) {
    std::string core;
    for (char c : password) {
        if (is_ascii(c) && std::isalpha(static_cast<unsigned char>(c))) {
            core += lower(c);
        } else if (!core.empty() || !is_ascii(c)) {
            core += lower(c);
        }
    }
    // Strip trailing digits and symbols.
    while (!core.empty() && is_ascii(core.back()) && !std::isalpha(static_cast<unsigned char>(core.back()))) {
        core.pop_back();
    }

    std::string whole;
    for (char c : password) {
        whole += lower(c);
    }
    for (std::string_view common : COMMON_PASSWORDS) {
        if (whole == common || core == common) {
            return true;
        }
    }
    return false;
}

const char* label_for(int score) {
    static const char* const labels[] = {"Very weak", "Weak", "Fair", "Strong", "Very strong"};
    return labels[score];
}

}  // namespace

PasswordStrength estimate_strength(const std::string& password) {
    PasswordStrength result;

    bool has_lower = false, has_upper = false, has_digit = false, has_symbol = false, has_other = false;
    double effective_length = 0.0;
    size_t length = 0;
    bool patterns = false;
    char previous = 0;

    for (char c : password) {
        if (is_continuation_byte(c)) {
            continue;
        }
        ++length;
        const auto byte = static_cast<unsigned char>(c);
        if (!is_ascii(c)) {
            has_other = true;
        } else if (std::islower(byte)) {
            has_lower = true;
        } else if (std::isupper(byte)) {
            has_upper = true;
        } else if (std::isdigit(byte)) {
            has_digit = true;
        } else {
            has_symbol = true;
        }

        if (length > 1 && continues_pattern(previous, c)) {
            effective_length += PATTERN_WEIGHT;
            patterns = true;
        } else {
            effective_length += 1.0;
        }
        previous = c;
    }

    int pool = 0;
    pool += has_lower ? 26 : 0;
    pool += has_upper ? 26 : 0;
    pool += has_digit ? 10 : 0;
    pool += has_symbol ? 33 : 0;
    pool += has_other ? 100 : 0;
    result.bits = pool > 0 ? effective_length * std::log2(static_cast<double>(pool)) : 0.0;

    if (password.empty()) {
        result.score = 0;
    } else if (is_common(password)) {
        result.score = 0;
        result.advice = "This is one of the most used passwords.";
    } else {
        if (result.bits < 28) {
            result.score = 0;
        } else if (result.bits < 36) {
            result.score = 1;
        } else if (result.bits < 60) {
            result.score = 2;
        } else if (result.bits < 80) {
            result.score = 3;
        } else {
            result.score = 4;
        }

        if (length < 8) {
            result.score = std::min(result.score, 1);
            result.advice = "Use at least 8 characters, ideally 12 or more.";
        } else if (patterns && result.score < 3) {
            result.advice = "Avoid repeated characters and sequences like 1234, abcd or qwerty.";
        } else if (result.score < 3) {
            result.advice = "Make it longer, or mix in uppercase letters, digits and symbols.";
        }
    }

    result.label = label_for(result.score);
    return result;
}
