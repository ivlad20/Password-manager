#include "cli.h"
#include "console.h"
#include "../crypto/password_generator.h"

#include <sodium.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

static const int MAX_UNLOCK_ATTEMPTS = 3;
static const size_t MIN_MASTER_PASSWORD_LENGTH = 8;

static void wipe(std::string& value) {
    if (!value.empty()) {
        sodium_memzero(value.data(), value.size());
    }
    value.clear();
}

static std::string trim(const std::string& text) {
    const char* whitespace = " \t";
    size_t start = text.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

// Shows "Label [current]: " and keeps the current value when the user just presses Enter.
static bool read_with_default(const std::string& label, const std::string& current, std::string& out) {
    std::string input;
    if (!read_line(label + " [" + current + "]: ", input)) {
        return false;
    }
    out = input.empty() ? current : input;
    return true;
}

Cli::Cli(std::string path) : path(std::move(path)) {}

int Cli::run() {
    if (!open_or_create()) {
        return 1;
    }

    std::cout << "Type 'help' to see the available commands.\n";

    std::string line;
    while (read_line("pm> ", line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        size_t space = line.find(' ');
        std::string command = line.substr(0, space);
        std::string argument = space == std::string::npos ? "" : trim(line.substr(space + 1));

        try {
            if (!handle_command(command, argument)) {
                break;
            }
        } catch (const std::exception& error) {
            std::cout << "Error: " << error.what() << "\n";
        }
    }

    vault.reset();  // locks and wipes the vault from memory
    std::cout << "Bye.\n";
    return 0;
}

bool Cli::open_or_create() {
    if (!std::filesystem::exists(path)) {
        std::cout << "No vault found at " << path << ".\n";
        if (!confirm("Create a new vault?")) {
            return false;
        }
        return create_vault();
    }

    try {
        vault = Vault::load(path);
    } catch (const std::exception& error) {
        std::cout << "Cannot open vault: " << error.what() << "\n";
        return false;
    }
    return unlock_vault();
}

bool Cli::create_vault() {
    std::string password;
    std::string repeated;

    while (true) {
        if (!read_hidden("New master password: ", password)) {
            return false;
        }
        if (password.size() < MIN_MASTER_PASSWORD_LENGTH) {
            std::cout << "The master password must have at least " << MIN_MASTER_PASSWORD_LENGTH
                      << " characters.\n";
            continue;
        }
        if (!read_hidden("Repeat master password: ", repeated)) {
            wipe(password);
            return false;
        }
        if (password != repeated) {
            std::cout << "The passwords do not match, try again.\n";
            wipe(password);
            wipe(repeated);
            continue;
        }
        break;
    }

    std::cout << "Creating vault (this takes a moment)...\n";
    try {
        vault = Vault::create(path, password);
    } catch (const std::exception& error) {
        std::cout << "Cannot create vault: " << error.what() << "\n";
        wipe(password);
        wipe(repeated);
        return false;
    }
    wipe(password);
    wipe(repeated);

    std::cout << "Vault created at " << path << ".\n"
              << "Do not forget the master password: there is no way to recover it.\n";
    return true;
}

bool Cli::unlock_vault() {
    std::string password;
    for (int attempt = 1; attempt <= MAX_UNLOCK_ATTEMPTS; ++attempt) {
        if (!read_hidden("Master password: ", password)) {
            return false;
        }
        bool ok = vault->unlock(password);
        wipe(password);
        if (ok) {
            std::cout << "Vault unlocked (" << vault->get_entries().size() << " entries).\n";
            return true;
        }
        std::cout << "Wrong password.\n";
    }
    std::cout << "Too many failed attempts.\n";
    return false;
}

bool Cli::handle_command(const std::string& command, const std::string& argument) {
    if (command == "help") {
        cmd_help();
    } else if (command == "list" || command == "ls") {
        cmd_list();
    } else if (command == "get") {
        cmd_get(argument);
    } else if (command == "add") {
        cmd_add();
    } else if (command == "edit") {
        cmd_edit(argument);
    } else if (command == "remove" || command == "rm") {
        cmd_remove(argument);
    } else if (command == "gen") {
        cmd_generate(argument);
    } else if (command == "lock") {
        return cmd_lock();
    } else if (command == "quit" || command == "exit") {
        return false;
    } else {
        std::cout << "Unknown command '" << command << "'. Type 'help'.\n";
    }
    return true;
}

void Cli::cmd_help() const {
    std::cout << "Commands:\n"
              << "  list              show all entries\n"
              << "  get <name>        show one entry, including its password\n"
              << "  add               add a new entry\n"
              << "  edit <name>       change an entry (press Enter to keep a field)\n"
              << "  remove <name>     delete an entry\n"
              << "  gen [length]      generate a random password (default "
              << DEFAULT_PASSWORD_LENGTH << ")\n"
              << "  lock              lock the vault and ask for the master password again\n"
              << "  quit              lock the vault and exit\n";
}

void Cli::cmd_list() const {
    std::vector<const VaultEntry*> sorted;
    for (const VaultEntry& entry : vault->get_entries()) {
        sorted.push_back(&entry);
    }
    if (sorted.empty()) {
        std::cout << "The vault is empty. Use 'add' to create an entry.\n";
        return;
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const VaultEntry* a, const VaultEntry* b) { return a->name < b->name; });

    for (const VaultEntry* entry : sorted) {
        std::cout << "  " << entry->name;
        if (!entry->username.empty()) {
            std::cout << "  (" << entry->username << ")";
        }
        std::cout << "\n";
    }
}

void Cli::cmd_get(const std::string& name) const {
    if (name.empty()) {
        std::cout << "Usage: get <name>\n";
        return;
    }
    const VaultEntry* entry = vault->find_entry(name);
    if (entry == nullptr) {
        std::cout << "No entry named '" << name << "'.\n";
        return;
    }
    std::cout << "  Name:     " << entry->name << "\n"
              << "  Username: " << entry->username << "\n"
              << "  Password: " << entry->password << "\n"
              << "  URL:      " << entry->url << "\n"
              << "  Notes:    " << entry->notes << "\n";
}

void Cli::cmd_add() {
    VaultEntry entry;
    if (!read_line("Name: ", entry.name)) {
        return;
    }
    entry.name = trim(entry.name);
    if (entry.name.empty()) {
        std::cout << "The name cannot be empty.\n";
        return;
    }
    if (vault->find_entry(entry.name) != nullptr) {
        std::cout << "An entry named '" << entry.name << "' already exists.\n";
        return;
    }
    if (!read_line("Username: ", entry.username)) {
        return;
    }
    if (!ask_new_password(entry.password)) {
        return;
    }
    if (!read_line("URL: ", entry.url) || !read_line("Notes: ", entry.notes)) {
        wipe(entry.password);
        return;
    }

    vault->add_entry(entry);
    wipe(entry.password);
    save();
    std::cout << "Entry '" << entry.name << "' added.\n";
}

void Cli::cmd_edit(const std::string& name) {
    if (name.empty()) {
        std::cout << "Usage: edit <name>\n";
        return;
    }
    const VaultEntry* existing = vault->find_entry(name);
    if (existing == nullptr) {
        std::cout << "No entry named '" << name << "'.\n";
        return;
    }

    VaultEntry updated = *existing;
    if (!read_with_default("Name", existing->name, updated.name) ||
        !read_with_default("Username", existing->username, updated.username)) {
        wipe(updated.password);
        return;
    }
    updated.name = trim(updated.name);

    std::string choice;
    if (!read_line("Password: [Enter] keep, [t] type a new one, [g] generate: ", choice)) {
        wipe(updated.password);
        return;
    }
    if (choice == "t" || choice == "g") {
        wipe(updated.password);
        if (choice == "g") {
            updated.password = generate_password();
            std::cout << "Generated password: " << updated.password << "\n";
        } else {
            std::string repeated;
            if (!read_hidden("New password: ", updated.password) ||
                !read_hidden("Repeat password: ", repeated)) {
                wipe(updated.password);
                return;
            }
            bool match = updated.password == repeated;
            wipe(repeated);
            if (!match) {
                std::cout << "The passwords do not match, nothing was changed.\n";
                wipe(updated.password);
                return;
            }
        }
    }

    if (!read_with_default("URL", existing->url, updated.url) ||
        !read_with_default("Notes", existing->notes, updated.notes)) {
        wipe(updated.password);
        return;
    }

    vault->update_entry(name, updated);
    wipe(updated.password);
    save();
    std::cout << "Entry '" << updated.name << "' updated.\n";
}

void Cli::cmd_remove(const std::string& name) {
    if (name.empty()) {
        std::cout << "Usage: remove <name>\n";
        return;
    }
    if (vault->find_entry(name) == nullptr) {
        std::cout << "No entry named '" << name << "'.\n";
        return;
    }
    if (!confirm("Delete '" + name + "'? This cannot be undone.")) {
        std::cout << "Cancelled.\n";
        return;
    }
    vault->remove_entry(name);
    save();
    std::cout << "Entry '" << name << "' deleted.\n";
}

void Cli::cmd_generate(const std::string& argument) const {
    size_t length = DEFAULT_PASSWORD_LENGTH;
    if (!argument.empty()) {
        try {
            size_t parsed_chars = 0;
            length = std::stoul(argument, &parsed_chars);
            if (parsed_chars != argument.size()) {
                throw std::invalid_argument("not a number");
            }
        } catch (const std::logic_error&) {
            std::cout << "Usage: gen [length]\n";
            return;
        }
    }
    std::string password = generate_password(length);
    std::cout << password << "\n";
    wipe(password);
}

bool Cli::cmd_lock() {
    vault->lock();
    std::cout << "Vault locked.\n";
    return unlock_vault();
}

bool Cli::ask_new_password(std::string& out) const {
    std::string password;
    if (!read_hidden("Password (leave empty to generate one): ", password)) {
        return false;
    }

    if (password.empty()) {
        out = generate_password();
        std::cout << "Generated password: " << out << "\n";
        return true;
    }

    std::string repeated;
    if (!read_hidden("Repeat password: ", repeated)) {
        wipe(password);
        return false;
    }
    bool match = password == repeated;
    wipe(repeated);
    if (!match) {
        std::cout << "The passwords do not match, the entry was not added.\n";
        wipe(password);
        return false;
    }
    out = std::move(password);
    return true;
}

void Cli::save() {
    try {
        vault->save();
    } catch (const std::exception& error) {
        std::cout << "Warning: the change is only in memory, saving failed: " << error.what() << "\n";
    }
}
