#include "crypto/password_strength.h"
#include "gui/strength_bar.h"
#include "gui/unlock_dialog.h"
#include "gui/vault_import.h"
#include "gui_test_helpers.h"
#include "../test_helpers.h"

#include <QFile>

class UnlockDialogTest : public TempDirTest {
protected:
    QString vault_path() const { return QString::fromStdString(path("vault.pwm")); }

    // Creates the vault through the dialog and returns once it is accepted.
    void create_vault(const QString& password) {
        UnlockDialog dialog(vault_path());
        dialog.show();
        auto edits = line_edits(&dialog);
        edits[0]->setText(password);
        edits[1]->setText(password);
        QMetaObject::invokeMethod(&dialog, "submit");
        ASSERT_TRUE(wait_until([&] { return dialog.result() == QDialog::Accepted; }));
    }
};

TEST_F(UnlockDialogTest, CreateModeAsksForPasswordTwice) {
    UnlockDialog dialog(vault_path());
    dialog.show();
    EXPECT_EQ(line_edits(&dialog).size(), 2);
}

TEST_F(UnlockDialogTest, CreateModeRejectsShortPassword) {
    UnlockDialog dialog(vault_path());
    dialog.show();
    auto edits = line_edits(&dialog);
    edits[0]->setText("short");
    edits[1]->setText("short");
    QMetaObject::invokeMethod(&dialog, "submit");
    EXPECT_TRUE(visible_text(&dialog).contains("at least 8"));
    EXPECT_FALSE(QFile::exists(vault_path()));
}

TEST_F(UnlockDialogTest, CreateModeRejectsMismatchedPasswords) {
    UnlockDialog dialog(vault_path());
    dialog.show();
    auto edits = line_edits(&dialog);
    edits[0]->setText("correcthorse");
    edits[1]->setText("different!");
    QMetaObject::invokeMethod(&dialog, "submit");
    EXPECT_TRUE(visible_text(&dialog).contains("do not match"));
    EXPECT_FALSE(QFile::exists(vault_path()));
}

TEST_F(UnlockDialogTest, CreateModeCreatesUnlockedVault) {
    UnlockDialog dialog(vault_path());
    dialog.show();
    auto edits = line_edits(&dialog);
    edits[0]->setText("correcthorse");
    edits[1]->setText("correcthorse");
    QMetaObject::invokeMethod(&dialog, "submit");
    // Inputs are disabled while Argon2 runs on the worker thread.
    EXPECT_FALSE(edits[0]->isEnabled());

    ASSERT_TRUE(wait_until([&] { return dialog.result() == QDialog::Accepted; }));
    ASSERT_NE(dialog.vault(), nullptr);
    EXPECT_TRUE(dialog.vault()->is_unlocked());
    EXPECT_TRUE(QFile::exists(vault_path()));
}

TEST_F(UnlockDialogTest, UnlockModeRejectsWrongPasswordThenAcceptsRightOne) {
    create_vault("correcthorse");

    UnlockDialog dialog(vault_path());
    dialog.show();
    auto edits = line_edits(&dialog);
    ASSERT_EQ(edits.size(), 1);

    edits[0]->setText("wrong password");
    QMetaObject::invokeMethod(&dialog, "submit");
    ASSERT_TRUE(wait_until([&] { return edits[0]->isEnabled(); }));
    EXPECT_TRUE(visible_text(&dialog).contains("Wrong master password"));
    EXPECT_TRUE(edits[0]->text().isEmpty());  // the wrong password is cleared
    EXPECT_NE(dialog.result(), QDialog::Accepted);

    edits[0]->setText("correcthorse");
    QMetaObject::invokeMethod(&dialog, "submit");
    ASSERT_TRUE(wait_until([&] { return dialog.result() == QDialog::Accepted; }));
    EXPECT_TRUE(dialog.vault()->is_unlocked());
}

TEST_F(UnlockDialogTest, CreateModeShowsPasswordStrength) {
    UnlockDialog dialog(vault_path());
    dialog.show();
    auto* strength = dialog.findChild<StrengthBar*>();
    ASSERT_NE(strength, nullptr);
    EXPECT_FALSE(strength->isVisible());

    line_edits(&dialog)[0]->setText("password");
    EXPECT_TRUE(strength->isVisible());
    EXPECT_EQ(strength->score(), 0);
    EXPECT_TRUE(visible_text(&dialog).contains("Very weak"));

    line_edits(&dialog)[0]->setText("mFk7zR!tbQ#w2Lp9");
    EXPECT_EQ(strength->score(), MAX_PASSWORD_STRENGTH);
}

TEST_F(UnlockDialogTest, CreateScreenCanImportExistingVault) {
    const std::string other = path("from old computer.pwm");
    {
        Vault vault = Vault::create(other, "old computer pw");
        vault.add_entry({"Steam", "gamer", "st3am-Pass", "", ""});
        vault.save();
    }
    set_vault_file_chooser([&](QWidget*) { return QString::fromStdString(other); });

    UnlockDialog dialog(vault_path());
    dialog.show();
    QString wrong_password_error;
    when_dialog_shown("UnlockDialog", [&](QWidget* import_dialog) {
        line_edits(import_dialog)[0]->setText("wrong password");
        QMetaObject::invokeMethod(import_dialog, "submit");
        wait_until([&] { return visible_text(import_dialog).contains("Wrong"); });
        wrong_password_error = visible_text(import_dialog);

        line_edits(import_dialog)[0]->setText("old computer pw");
        QMetaObject::invokeMethod(import_dialog, "submit");
    });
    button(&dialog, "Import an Existing Vault...")->click();
    set_vault_file_chooser(nullptr);

    EXPECT_TRUE(wrong_password_error.contains("Wrong")) << wrong_password_error.toStdString();
    ASSERT_EQ(dialog.result(), QDialog::Accepted);
    ASSERT_NE(dialog.vault(), nullptr);
    EXPECT_NE(dialog.vault()->find_entry("Steam"), nullptr);

    // Saved as this computer's vault, with the imported master password.
    Vault saved = Vault::load(vault_path().toStdString());
    ASSERT_TRUE(saved.unlock("old computer pw"));
    EXPECT_EQ(saved.get_entries().size(), 1u);
}
