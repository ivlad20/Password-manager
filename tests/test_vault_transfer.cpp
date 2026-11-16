#include "core/vault.h"
#include "core/vault_transfer.h"
#include "test_helpers.h"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

const std::string MASTER = "correct horse";

std::string file_name(const std::string& path) {
    return fs::path(path).filename().string();
}

}  // namespace

class VaultTransferTest : public TempDirTest {
protected:
    // Creates a vault with one entry and returns its path.
    std::string make_vault(const std::string& name, const std::string& password, const std::string& entry) {
        const std::string file = path(name);
        Vault vault = Vault::create(file, password);
        vault.add_entry({entry, "user", "secret-" + entry, "", ""});
        vault.save();
        return file;
    }
};

TEST_F(VaultTransferTest, ExportMakesIdenticalCopyWithDatedName) {
    const std::string vault = make_vault("vault.pwm", MASTER, "Gmail");
    const std::string folder = path("Downloads");

    const std::string first = export_vault(vault, folder);
    EXPECT_EQ(fs::path(first).parent_path(), fs::path(folder));
    EXPECT_EQ(file_name(first).rfind("PasswordManager-vault-", 0), 0u) << first;
    EXPECT_EQ(fs::path(first).extension(), ".pwm");
    EXPECT_EQ(read_file(first), read_file(vault));

    // The copy opens with the same master password.
    Vault copy = Vault::load(first);
    ASSERT_TRUE(copy.unlock(MASTER));
    EXPECT_NE(copy.find_entry("Gmail"), nullptr);

    // A second export on the same day does not overwrite the first one.
    const std::string second = export_vault(vault, folder);
    EXPECT_NE(second, first);
    EXPECT_NE(file_name(second).find(" (2).pwm"), std::string::npos) << second;
    EXPECT_TRUE(fs::exists(first));
}

TEST_F(VaultTransferTest, ExportRefusesFileThatIsNotAVault) {
    write_file(path("notes.pwm"), "just some text, not a vault");
    EXPECT_THROW(export_vault(path("notes.pwm"), path("Downloads")), std::runtime_error);
    EXPECT_FALSE(fs::exists(path("Downloads")) && !fs::is_empty(path("Downloads")));
}

TEST_F(VaultTransferTest, ImportReplacesVaultAndKeepsBackup) {
    const std::string current = make_vault("vault.pwm", MASTER, "Old entry");
    const std::string current_bytes = read_file(current);
    const std::string other = make_vault("other.pwm", "other computer pw", "New entry");

    // A wrong password for the imported vault is detected before anything changes.
    Vault imported = Vault::load(other);
    EXPECT_FALSE(imported.unlock("wrong password"));
    EXPECT_EQ(read_file(current), current_bytes);

    ASSERT_TRUE(imported.unlock("other computer pw"));
    const std::string backup = backup_vault(current);
    imported.save_to(current);

    EXPECT_EQ(fs::path(backup).parent_path(), fs::path(current).parent_path());
    EXPECT_EQ(file_name(backup).rfind("vault-backup-", 0), 0u) << backup;
    EXPECT_EQ(read_file(backup), current_bytes);

    // The default vault now holds the imported entries and uses the imported password.
    Vault reloaded = Vault::load(current);
    EXPECT_FALSE(reloaded.unlock(MASTER));
    ASSERT_TRUE(reloaded.unlock("other computer pw"));
    EXPECT_NE(reloaded.find_entry("New entry"), nullptr);
    EXPECT_EQ(reloaded.find_entry("Old entry"), nullptr);

    // Later saves go to the default vault, not to the file that was imported.
    const std::string other_bytes = read_file(other);
    imported.add_entry({"Another", "", "x", "", ""});
    imported.save();
    EXPECT_EQ(read_file(other), other_bytes);

    // The backup still opens with the old password.
    Vault old = Vault::load(backup);
    ASSERT_TRUE(old.unlock(MASTER));
    EXPECT_NE(old.find_entry("Old entry"), nullptr);
}

TEST_F(VaultTransferTest, ImportedVersion1VaultIsUpgraded) {
    const std::string target = path("vault.pwm");
    Vault imported = Vault::load(std::string(TEST_DATA_DIR) + "/vault_v1.pwm");
    ASSERT_TRUE(imported.unlock("fixture-password-v1"));
    imported.save_to(target);

    EXPECT_EQ(read_file(target)[4], '\x02');
    Vault reloaded = Vault::load(target);
    ASSERT_TRUE(reloaded.unlock("fixture-password-v1"));
    EXPECT_EQ(reloaded.get_entries().size(), 3u);
}

TEST_F(VaultTransferTest, SameFile) {
    const std::string vault = make_vault("vault.pwm", MASTER, "Gmail");
    EXPECT_TRUE(same_file(vault, (dir / "." / "vault.pwm").string()));
    EXPECT_FALSE(same_file(vault, path("missing.pwm")));
}
