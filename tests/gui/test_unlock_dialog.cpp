#include "gui/unlock_dialog.h"
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
