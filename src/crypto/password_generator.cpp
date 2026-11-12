#include "password_generator.h"

#include <sodium.h>
#include <stdexcept>

static const std::string LOWER = "abcdefghijklmnopqrstuvwxyz";
static const std::string UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const std::string DIGITS = "0123456789";
static const std::string SYMBOLS = "!@#$%^&*()-_=+[]{};:,.?/";

static bool contains_any(const std::string& text, const std::string& charset) {
    return text.find_first_of(charset) != std::string::npos;
}

std::string generate_password(size_t length) {
    if (length < MIN_PASSWORD_LENGTH || length > MAX_PASSWORD_LENGTH) {
        throw std::invalid_argument("password length must be between " +
                                    std::to_string(MIN_PASSWORD_LENGTH) + " and " +
                                    std::to_string(MAX_PASSWORD_LENGTH));
    }

    const std::string alphabet = LOWER + UPPER + DIGITS + SYMBOLS;

    // Draw every character uniformly from the full alphabet and retry until all four
    // classes appear. Retrying keeps the distribution unbiased, unlike forcing one
    // character of each class into fixed positions.
    std::string password(length, '\0');
    while (true) {
        for (char& c : password) {
            c = alphabet[randombytes_uniform(static_cast<uint32_t>(alphabet.size()))];
        }
        if (contains_any(password, LOWER) && contains_any(password, UPPER) &&
            contains_any(password, DIGITS) && contains_any(password, SYMBOLS)) {
            return password;
        }
    }
}
