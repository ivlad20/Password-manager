#include "vault_transfer.h"
#include "../storage/vault_file.h"

#include <ctime>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string today() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[11];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &local);
    return text;
}

// "<folder>/<stem>.pwm", or "<folder>/<stem> (2).pwm" and so on if the name is taken.
fs::path free_path(const fs::path& folder, const std::string& stem) {
    fs::path candidate = folder / (stem + ".pwm");
    for (int i = 2; fs::exists(candidate); ++i) {
        candidate = folder / (stem + " (" + std::to_string(i) + ").pwm");
    }
    return candidate;
}

}  // namespace

std::string export_vault(const std::string& vault_path, const std::string& folder) {
    read_vault_file(vault_path);  // throws unless it is a complete vault file

    fs::create_directories(folder);
    const fs::path target = free_path(folder, "PasswordManager-vault-" + today());
    fs::copy_file(vault_path, target);
    return target.string();
}

std::string backup_vault(const std::string& path) {
    const fs::path source(path);
    const fs::path target = free_path(source.parent_path(), "vault-backup-" + today());
    fs::copy_file(source, target);
    return target.string();
}

bool same_file(const std::string& a, const std::string& b) {
    std::error_code error;
    const bool same = fs::equivalent(a, b, error);
    return !error && same;
}
