#pragma once

#include <cstddef>
#include <string>

constexpr size_t DEFAULT_PASSWORD_LENGTH = 20;
constexpr size_t MIN_PASSWORD_LENGTH = 8;
constexpr size_t MAX_PASSWORD_LENGTH = 128;

// Random password with at least one lowercase letter, uppercase letter, digit and symbol.
// Uses libsodium's CSPRNG, so it is safe for real passwords (unlike rand()).
// Throws std::invalid_argument if length is outside [MIN_PASSWORD_LENGTH, MAX_PASSWORD_LENGTH].
std::string generate_password(size_t length = DEFAULT_PASSWORD_LENGTH);
