#include "core/vault.h"
#include "test_helpers.h"

#include <filesystem>
#include <stdexcept>

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
