#include "core/vault.h"
#include "test_helpers.h"

#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string MASTER = "correct horse";

VaultEntry gmail() { return {"Gmail", "me@example.com", "SuperSecret123!", "https://mail.google.com", "personal"}; }
VaultEntry bank() { return {"bank", "user1", "B4nk!pass", "", ""}; }

}  // namespace

// Each Argon2id derivation with the MODERATE settings takes about a second,
// so the tests below reuse one vault where they can.
class VaultTest : public TempDirTest {
protected:
    // Creates a vault with two entries, saves it and returns its path.
    std::string make_vault() {
        const std::string file = path("vault.pwm");
        Vault vault = Vault::create(file, MASTER);
        vault.add_entry(gmail());
        vault.add_entry(bank());
        vault.save();
        return file;
    }
};

TEST_F(VaultTest, CreateReturnsUnlockedEmptyVaultAndWritesFile) {
    Vault vault = Vault::create(path("vault.pwm"), MASTER);
    EXPECT_TRUE(vault.is_unlocked());
    EXPECT_TRUE(vault.get_entries().empty());
    EXPECT_TRUE(std::filesystem::exists(path("vault.pwm")));
}

TEST_F(VaultTest, CreateRefusesToOverwriteExistingFile) {
    const std::string file = make_vault();
    EXPECT_THROW(Vault::create(file, "other password"), std::runtime_error);
}

TEST_F(VaultTest, FileContainsNoPlaintext) {
    const std::string raw = read_file(make_vault());
    for (const std::string& secret : {"Gmail", "me@example.com", "SuperSecret123!", "mail.google", "B4nk!pass"}) {
        EXPECT_EQ(raw.find(secret), std::string::npos) << secret;
    }
}

TEST_F(VaultTest, LoadReturnsLockedVault) {
    Vault vault = Vault::load(make_vault());
    EXPECT_FALSE(vault.is_unlocked());
    EXPECT_THROW((void)vault.get_entries(), std::runtime_error);
    EXPECT_THROW((void)vault.find_entry("Gmail"), std::runtime_error);
    EXPECT_THROW(vault.add_entry(gmail()), std::runtime_error);
}

TEST_F(VaultTest, WrongPasswordIsRejectedAndRightOneWorks) {
    Vault vault = Vault::load(make_vault());
    EXPECT_FALSE(vault.unlock("wrong password"));
    EXPECT_FALSE(vault.is_unlocked());

    ASSERT_TRUE(vault.unlock(MASTER));
    ASSERT_EQ(vault.get_entries().size(), 2u);
    const VaultEntry* entry = vault.find_entry("Gmail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->username, "me@example.com");
    EXPECT_EQ(entry->password, "SuperSecret123!");
    EXPECT_EQ(entry->url, "https://mail.google.com");
    EXPECT_EQ(entry->notes, "personal");
}

TEST_F(VaultTest, LockWipesEntries) {
    Vault vault = Vault::load(make_vault());
    ASSERT_TRUE(vault.unlock(MASTER));
    vault.lock();
    EXPECT_FALSE(vault.is_unlocked());
    EXPECT_THROW((void)vault.get_entries(), std::runtime_error);
}

TEST_F(VaultTest, AddUpdateRemoveRules) {
    Vault vault = Vault::create(path("vault.pwm"), MASTER);
    vault.add_entry(gmail());
    vault.add_entry(bank());

    EXPECT_THROW(vault.add_entry(gmail()), std::runtime_error);                       // duplicate
    EXPECT_THROW(vault.add_entry({"", "u", "p", "", ""}), std::runtime_error);        // empty name
    EXPECT_THROW(vault.update_entry("missing", gmail()), std::runtime_error);         // unknown
    EXPECT_THROW(vault.update_entry("bank", gmail()), std::runtime_error);            // name taken

    VaultEntry renamed = bank();
    renamed.name = "My Bank";
    renamed.username = "user2";
    vault.update_entry("bank", renamed);
    EXPECT_EQ(vault.find_entry("bank"), nullptr);
    ASSERT_NE(vault.find_entry("My Bank"), nullptr);
    EXPECT_EQ(vault.find_entry("My Bank")->username, "user2");

    // Keeping the same name while editing other fields is allowed.
    VaultEntry same_name = gmail();
    same_name.password = "NewPassword1!";
    vault.update_entry("Gmail", same_name);
    EXPECT_EQ(vault.find_entry("Gmail")->password, "NewPassword1!");

    EXPECT_TRUE(vault.remove_entry("Gmail"));
    EXPECT_FALSE(vault.remove_entry("Gmail"));
    EXPECT_EQ(vault.get_entries().size(), 1u);
}

TEST_F(VaultTest, ChangesSurviveSaveAndReload) {
    const std::string file = make_vault();
    {
        Vault vault = Vault::load(file);
        ASSERT_TRUE(vault.unlock(MASTER));
        EXPECT_TRUE(vault.remove_entry("bank"));
        vault.add_entry({"GitHub", "ivlad", "gh-Pass-99", "https://github.com", ""});
        vault.save();
    }
    Vault reloaded = Vault::load(file);
    ASSERT_TRUE(reloaded.unlock(MASTER));
    EXPECT_EQ(reloaded.get_entries().size(), 2u);
    EXPECT_EQ(reloaded.find_entry("bank"), nullptr);
    EXPECT_NE(reloaded.find_entry("GitHub"), nullptr);
    EXPECT_FALSE(std::filesystem::exists(file + ".tmp"));
}

// Every save uses a fresh nonce, so saving the same data twice gives different bytes.
TEST_F(VaultTest, EverySaveUsesNewNonce) {
    const std::string file = make_vault();
    const std::string before = read_file(file);
    Vault vault = Vault::load(file);
    ASSERT_TRUE(vault.unlock(MASTER));
    vault.save();
    const std::string after = read_file(file);
    ASSERT_EQ(before.size(), after.size());
    EXPECT_EQ(before.substr(0, 40), after.substr(0, 40));  // same header up to the salt
    EXPECT_NE(before.substr(40), after.substr(40));         // different nonce and ciphertext
}

TEST_F(VaultTest, TamperedFileFailsToUnlock) {
    const std::string file = make_vault();
    std::string raw = read_file(file);
    raw[raw.size() - 3] ^= 1;
    write_file(file, raw);

    Vault vault = Vault::load(file);
    EXPECT_FALSE(vault.unlock(MASTER));
}

TEST_F(VaultTest, LoadRejectsInvalidFiles) {
    write_file(path("junk.pwm"), "hello world, this is not a vault at all........................................");
    EXPECT_THROW(Vault::load(path("junk.pwm")), std::runtime_error);
    EXPECT_THROW(Vault::load(path("missing.pwm")), std::runtime_error);
}

TEST_F(VaultTest, MoveTransfersUnlockedState) {
    Vault a = Vault::load(make_vault());
    ASSERT_TRUE(a.unlock(MASTER));
    Vault b = std::move(a);
    EXPECT_TRUE(b.is_unlocked());
    EXPECT_FALSE(a.is_unlocked());  // NOLINT(bugprone-use-after-move): checking the moved-from state
    EXPECT_NE(b.find_entry("Gmail"), nullptr);
}

TEST_F(VaultTest, AddAndUpdateManageTimestamps) {
    Vault vault = Vault::create(path("vault.pwm"), MASTER);
    const auto before = static_cast<uint64_t>(std::time(nullptr));
    VaultEntry entry = gmail();
    entry.created_at = 42;  // set by the vault, not by the caller
    vault.add_entry(entry);
    const auto after = static_cast<uint64_t>(std::time(nullptr));

    const VaultEntry* added = vault.find_entry("Gmail");
    ASSERT_NE(added, nullptr);
    EXPECT_GE(added->created_at, before);
    EXPECT_LE(added->created_at, after);
    EXPECT_EQ(added->modified_at, added->created_at);
    const uint64_t created = added->created_at;

    // Saving without changes keeps the modification time.
    VaultEntry unchanged = *added;
    unchanged.modified_at = 1;
    vault.update_entry("Gmail", unchanged);
    EXPECT_EQ(vault.find_entry("Gmail")->modified_at, created);

    VaultEntry edited = gmail();
    edited.created_at = 7;
    edited.favorite = true;
    edited.tags = {"email"};
    vault.update_entry("Gmail", edited);
    const VaultEntry* updated = vault.find_entry("Gmail");
    EXPECT_EQ(updated->created_at, created);
    EXPECT_GE(updated->modified_at, created);
    EXPECT_TRUE(updated->favorite);
    EXPECT_EQ(updated->tags, std::vector<std::string>{"email"});
    EXPECT_TRUE(updated->password_history.empty());  // the password did not change
}

TEST_F(VaultTest, PasswordChangesAreKeptInHistory) {
    Vault vault = Vault::create(path("vault.pwm"), MASTER);
    vault.add_entry(gmail());

    VaultEntry entry = gmail();
    for (int i = 1; i <= 7; ++i) {
        entry.password = "password-" + std::to_string(i);
        entry.password_history.clear();  // the caller cannot rewrite the history
        vault.update_entry("Gmail", entry);
    }

    const auto& history = vault.find_entry("Gmail")->password_history;
    ASSERT_EQ(history.size(), MAX_PASSWORD_HISTORY);
    // Newest first: the password before "password-7" was "password-6", and so on.
    for (size_t i = 0; i < history.size(); ++i) {
        EXPECT_EQ(history[i].password, "password-" + std::to_string(6 - i)) << "item " << i;
        EXPECT_GT(history[i].changed_at, 0u);
    }

    // The history survives a save and reload.
    vault.save();
    Vault reloaded = Vault::load(path("vault.pwm"));
    ASSERT_TRUE(reloaded.unlock(MASTER));
    const VaultEntry* loaded = reloaded.find_entry("Gmail");
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->password, "password-7");
    ASSERT_EQ(loaded->password_history.size(), MAX_PASSWORD_HISTORY);
    EXPECT_EQ(loaded->password_history[0].password, "password-6");
}

// tests/data/vault_v1.pwm was written by version 0.2.0 of the app (format version 1).
TEST_F(VaultTest, OpensVersion1VaultAndUpgradesItOnSave) {
    const std::string file = path("old.pwm");
    std::filesystem::copy_file(std::string(TEST_DATA_DIR) + "/vault_v1.pwm", file);
    EXPECT_EQ(read_file(file)[4], '\x01');

    const auto check_entries = [](const Vault& vault) {
        ASSERT_EQ(vault.get_entries().size(), 3u);
        const VaultEntry* gmail_entry = vault.find_entry("Gmail");
        ASSERT_NE(gmail_entry, nullptr);
        EXPECT_EQ(gmail_entry->username, "me@example.com");
        EXPECT_EQ(gmail_entry->password, "SuperSecret123!");
        EXPECT_EQ(gmail_entry->url, "https://mail.google.com");
        EXPECT_EQ(gmail_entry->notes, "personal\nsecond line");
        EXPECT_EQ(gmail_entry->created_at, 0u);  // unknown for migrated entries
        EXPECT_FALSE(gmail_entry->favorite);

        const VaultEntry* bank_entry = vault.find_entry("bank");
        ASSERT_NE(bank_entry, nullptr);
        EXPECT_EQ(bank_entry->password, "B4nk!pass");

        const VaultEntry* cafe = vault.find_entry("Caf\xC3\xA9 \xC8\x98tefan");
        ASSERT_NE(cafe, nullptr);
        EXPECT_EQ(cafe->password, std::string("a\0b", 3));
    };

    Vault vault = Vault::load(file);
    EXPECT_FALSE(vault.unlock("wrong password"));
    ASSERT_TRUE(vault.unlock("fixture-password-v1"));
    check_entries(vault);

    vault.save();
    EXPECT_EQ(read_file(file)[4], '\x02');

    Vault reloaded = Vault::load(file);
    ASSERT_TRUE(reloaded.unlock("fixture-password-v1"));
    check_entries(reloaded);
}
