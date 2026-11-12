//
// Created by vladi on 11/12/2026.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> master_password_derive(const std::string& master_key, const uint8_t* salt);
std::vector<uint8_t> generate_nonce();
std::vector<uint8_t> encrypt_password(const std::string& plaintext,
                                      const std::vector<uint8_t>& key,
                                      const std::vector<uint8_t>& nonce);
std::string decrypt_password(const std::vector<uint8_t>& ciphertext,
                             const std::vector<uint8_t>& key,
                             const std::vector<uint8_t>& nonce);
void print_hex(const std::string& label, const std::vector<uint8_t>& data);


