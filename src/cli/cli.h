#pragma once

#include "../core/vault.h"

#include <optional>
#include <string>

// Interactive command loop: opens (or creates) the vault at `path`, asks for the
// master password, then reads commands until "quit". Every change is saved immediately.
class Cli {
public:
    explicit Cli(std::string path);

    // Returns the process exit code.
    int run();

private:
    bool open_or_create();
    bool create_vault();
    bool unlock_vault();

    // Returns false when the loop should stop.
    bool handle_command(const std::string& command, const std::string& argument);

    void cmd_help() const;
    void cmd_list() const;
    void cmd_get(const std::string& name) const;
    void cmd_add();
    void cmd_edit(const std::string& name);
    void cmd_remove(const std::string& name);
    void cmd_generate(const std::string& argument) const;
    bool cmd_lock();

    // Asks for a new password: typed twice, or generated when left empty.
    // Returns false if input ended or the two typed passwords did not match.
    bool ask_new_password(std::string& out) const;
    void save();

    std::string path;
    std::optional<Vault> vault;
};
