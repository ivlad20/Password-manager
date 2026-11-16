#include "gui/main_window.h"
#include "gui/lock_settings.h"
#include "crypto/password_strength.h"
#include "gui/strength_bar.h"
#include "gui/vault_import.h"
#include "core/vault.h"
#include "gui_test_helpers.h"
#include "../test_helpers.h"

#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QToolButton>
#include <memory>

namespace {

const std::string MASTER = "correcthorse";

QStringList visible_names(QListWidget* list) {
    QStringList names;
    for (int i = 0; i < list->count(); ++i) {
        if (!list->item(i)->isHidden()) names << list->item(i)->text();
    }
    return names;
}

void select(QListWidget* list, const QString& name) {
    const auto items = list->findItems(name, Qt::MatchExactly);
    ASSERT_EQ(items.size(), 1) << name.toStdString();
    list->setCurrentItem(items.first());
}

}  // namespace

// Every test starts with an unlocked main window over a vault that has three entries.
// The vault is created once per test through the core library, which is faster than
// going through the create dialog each time (the dialog has its own tests).
class MainWindowTest : public TempDirTest {
protected:
    void SetUp() override {
        TempDirTest::SetUp();
        file = path("vault.pwm");
        {
            Vault vault = Vault::create(file, MASTER);
            vault.add_entry({"Gmail", "me@example.com", "SuperSecret123!", "https://mail.google.com", "personal"});
            vault.add_entry({"bank", "user1", "B4nk!pass", "", ""});
            vault.add_entry({"GitHub", "ivlad", "gh-Pass-99", "https://github.com", "work"});
            vault.save();
        }

        // Every test starts with the default settings.
        QFile::remove(LockSettings::file_path());
        window = std::make_unique<MainWindow>(QString::fromStdString(file));
        on_next_dialog([](QWidget* dialog) {
            line_edits(dialog)[0]->setText(QString::fromStdString(MASTER));
            QMetaObject::invokeMethod(dialog, "submit");
        });
        ASSERT_TRUE(window->unlock());
        window->show();
        list = window->findChild<QListWidget*>();
        ASSERT_NE(list, nullptr);
    }

    void TearDown() override {
        window.reset();
        set_vault_file_chooser(nullptr);
        QFile::remove(LockSettings::file_path());
        TempDirTest::TearDown();
    }

    // Reads the file again from disk, to check that changes were saved.
    Vault reload() const {
        Vault vault = Vault::load(file);
        EXPECT_TRUE(vault.unlock(MASTER));
        return vault;
    }

    // Expects the unlock dialog (shown after an automatic lock) and unlocks again.
    // Turns the inactivity timer off first, so the window does not lock again right away.
    void unlock_when_asked(bool& asked, bool& was_locked, bool& was_minimized) {
        when_dialog_shown("UnlockDialog", [&, this](QWidget* dialog) {
            asked = true;
            was_locked = list->count() == 0 && !window->isVisible();
            was_minimized = dialog->isMinimized();
            window->set_idle_timeout_ms(0);
            line_edits(dialog)[0]->setText(QString::fromStdString(MASTER));
            QMetaObject::invokeMethod(dialog, "submit");
        });
    }

    [[nodiscard]] bool unlocked_again(const bool& asked) const {
        return wait_until([&] { return asked && window->isVisible() && list->count() == 3; });
    }

    QLineEdit* field_with_text(const QString& text) const {
        for (QLineEdit* edit : line_edits(window.get())) {
            if (edit->text() == text) return edit;
        }
        return nullptr;
    }

    std::string file;
    std::unique_ptr<MainWindow> window;
    QListWidget* list = nullptr;
};

TEST_F(MainWindowTest, ListIsSortedCaseInsensitive) {
    EXPECT_EQ(visible_names(list), (QStringList{"bank", "GitHub", "Gmail"}));
}

TEST_F(MainWindowTest, EditAndDeleteNeedASelection) {
    list->setCurrentRow(-1);
    EXPECT_FALSE(action(window.get(), "Edit")->isEnabled());
    EXPECT_FALSE(action(window.get(), "Delete")->isEnabled());
    select(list, "Gmail");
    EXPECT_TRUE(action(window.get(), "Edit")->isEnabled());
    EXPECT_TRUE(action(window.get(), "Delete")->isEnabled());
}

TEST_F(MainWindowTest, PasswordIsHiddenUntilShowIsPressed) {
    select(list, "Gmail");
    QLineEdit* password = field_with_text("SuperSecret123!");
    ASSERT_NE(password, nullptr);
    EXPECT_EQ(password->echoMode(), QLineEdit::Password);

    for (QToolButton* b : window->findChildren<QToolButton*>()) {
        if (b->text() == "Show") b->click();
    }
    EXPECT_EQ(password->echoMode(), QLineEdit::Normal);
}

TEST_F(MainWindowTest, SearchMatchesNameUsernameAndUrl) {
    QLineEdit* search = line_edit_with_placeholder(window.get(), "Search");
    ASSERT_NE(search, nullptr);

    search->setText("GMAIL");
    EXPECT_EQ(visible_names(list), QStringList{"Gmail"});
    search->setText("ivlad");
    EXPECT_EQ(visible_names(list), QStringList{"GitHub"});
    search->setText("github.com");
    EXPECT_EQ(visible_names(list), QStringList{"GitHub"});
    search->setText("nothing matches this");
    EXPECT_TRUE(visible_names(list).isEmpty());
    search->clear();
    EXPECT_EQ(visible_names(list).size(), 3);
}

TEST_F(MainWindowTest, CopyPasswordPutsItOnClipboard) {
    select(list, "Gmail");
    action(window.get(), "Copy password")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "SuperSecret123!");
}

TEST_F(MainWindowTest, ClosingWindowClearsCopiedPassword) {
    select(list, "Gmail");
    action(window.get(), "Copy password")->trigger();
    ASSERT_EQ(QGuiApplication::clipboard()->text(), "SuperSecret123!");
    window.reset();
    EXPECT_TRUE(QGuiApplication::clipboard()->text().isEmpty());
}

TEST_F(MainWindowTest, AddEntryWithGeneratedPassword) {
    on_next_dialog([](QWidget* dialog) {
        auto edits = line_edits(dialog);  // name, username, password come first
        edits[0]->setText("Steam");
        edits[1]->setText("gamer");
        button(dialog, "Generate")->click();
        line_edit_with_placeholder(dialog, "https://")->setText("https://store.steampowered.com");
        dialog->findChild<QPlainTextEdit*>()->setPlainText("games");
        QMetaObject::invokeMethod(dialog, "validate_and_accept");
    });
    action(window.get(), "Add")->trigger();

    EXPECT_EQ(visible_names(list), (QStringList{"bank", "GitHub", "Gmail", "Steam"}));
    ASSERT_NE(list->currentItem(), nullptr);
    EXPECT_EQ(list->currentItem()->text(), "Steam");  // the new entry gets selected

    Vault saved = reload();
    const VaultEntry* steam = saved.find_entry("Steam");
    ASSERT_NE(steam, nullptr);
    EXPECT_EQ(steam->username, "gamer");
    EXPECT_EQ(steam->password.size(), 20u);
    EXPECT_EQ(steam->url, "https://store.steampowered.com");
    EXPECT_EQ(steam->notes, "games");
}

TEST_F(MainWindowTest, AddRefusesDuplicateName) {
    QString error;
    on_next_dialog([&](QWidget* dialog) {
        auto edits = line_edits(dialog);
        edits[0]->setText("Gmail");
        edits[2]->setText("x");
        QMetaObject::invokeMethod(dialog, "validate_and_accept");
        error = visible_text(dialog);
        when_dialog_shown("MessageDialog", [](QWidget* box) { button(box, "Discard")->click(); });
        QMetaObject::invokeMethod(dialog, "reject");
    });
    action(window.get(), "Add")->trigger();

    EXPECT_TRUE(error.contains("already called")) << error.toStdString();
    EXPECT_EQ(list->count(), 3);
}

TEST_F(MainWindowTest, EditCanRenameEntry) {
    select(list, "bank");
    on_next_dialog([](QWidget* dialog) {
        auto edits = line_edits(dialog);
        edits[0]->setText("My Bank");
        edits[1]->setText("user2");
        QMetaObject::invokeMethod(dialog, "validate_and_accept");
    });
    action(window.get(), "Edit")->trigger();

    EXPECT_EQ(visible_names(list), (QStringList{"GitHub", "Gmail", "My Bank"}));
    Vault saved = reload();
    EXPECT_EQ(saved.find_entry("bank"), nullptr);
    ASSERT_NE(saved.find_entry("My Bank"), nullptr);
    EXPECT_EQ(saved.find_entry("My Bank")->username, "user2");
    EXPECT_EQ(saved.find_entry("My Bank")->password, "B4nk!pass");
}

TEST_F(MainWindowTest, DeleteAsksForConfirmation) {
    select(list, "GitHub");

    on_next_dialog([](QWidget* box) { button(box, "Cancel")->click(); });
    action(window.get(), "Delete")->trigger();
    EXPECT_EQ(list->count(), 3);

    on_next_dialog([](QWidget* box) { button(box, "Delete")->click(); });
    action(window.get(), "Delete")->trigger();
    EXPECT_EQ(visible_names(list), (QStringList{"bank", "Gmail"}));
    EXPECT_EQ(reload().find_entry("GitHub"), nullptr);
}

TEST_F(MainWindowTest, LocksAfterInactivity) {
    bool asked = false, was_locked = false, was_minimized = true;
    unlock_when_asked(asked, was_locked, was_minimized);
    window->set_idle_timeout_ms(100);

    ASSERT_TRUE(unlocked_again(asked));
    EXPECT_TRUE(was_locked);       // the entries were gone while the dialog was shown
    EXPECT_FALSE(was_minimized);   // after inactivity the dialog is shown normally
}

TEST_F(MainWindowTest, InputRestartsInactivityTimer) {
    window->set_idle_timeout_ms(400);
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 1000) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier);
        QApplication::sendEvent(list, &press);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    window->set_idle_timeout_ms(0);
    EXPECT_EQ(list->count(), 3);
    EXPECT_TRUE(window->isVisible());
}

TEST_F(MainWindowTest, AutoLockClosesOpenDialogs) {
    bool asked = false, was_locked = false, was_minimized = true;
    unlock_when_asked(asked, was_locked, was_minimized);
    // The add dialog is left open; the inactivity timer closes it before locking.
    when_dialog_shown("EntryDialog", [this](QWidget*) { window->set_idle_timeout_ms(100); });
    action(window.get(), "Add")->trigger();

    ASSERT_TRUE(unlocked_again(asked));
    EXPECT_TRUE(was_locked);
    EXPECT_EQ(QApplication::activeModalWidget(), nullptr);
    EXPECT_EQ(reload().get_entries().size(), 3u);
}

TEST_F(MainWindowTest, MinimizingLocksWhenEnabled) {
    LockSettings settings;
    settings.auto_lock_minutes = 0;
    settings.lock_on_minimize = false;
    window->apply_lock_settings(settings);
    window->showMinimized();
    wait_until([] { return false; }, 300);
    EXPECT_EQ(list->count(), 3);  // still unlocked
    window->showNormal();

    settings.lock_on_minimize = true;
    window->apply_lock_settings(settings);
    bool asked = false, was_locked = false, was_minimized = false;
    unlock_when_asked(asked, was_locked, was_minimized);
    window->showMinimized();

    ASSERT_TRUE(unlocked_again(asked));
    EXPECT_TRUE(was_locked);
    EXPECT_TRUE(was_minimized);  // the unlock dialog does not pop up over other windows
    EXPECT_FALSE(window->isMinimized());
}

TEST_F(MainWindowTest, SettingsAreSaved) {
    when_dialog_shown("SettingsDialog", [](QWidget* dialog) {
        auto* combo = dialog->findChild<QComboBox*>();
        combo->setCurrentIndex(combo->findData(15));
        for (QCheckBox* check : dialog->findChildren<QCheckBox*>()) {
            if (check->text().contains("minimized")) check->setChecked(false);
        }
        button(dialog, "Save")->click();
    });
    action(window.get(), "Settings")->trigger();

    const LockSettings saved = LockSettings::load();
    EXPECT_EQ(saved.auto_lock_minutes, 15);
    EXPECT_FALSE(saved.lock_on_minimize);
    EXPECT_TRUE(saved.lock_on_session_lock);
}

TEST_F(MainWindowTest, ChangeMasterPasswordFromSettings) {
    QString wrong_password_message;
    QString settings_message;
    when_dialog_shown("SettingsDialog", [&](QWidget* settings) {
        when_dialog_shown("ChangePasswordDialog", [&](QWidget* dialog) {
            line_edit_with_placeholder(dialog, "Current")->setText("not the password");
            line_edit_with_placeholder(dialog, "New master")->setText("brand new master");
            line_edit_with_placeholder(dialog, "Repeat")->setText("brand new master");
            QMetaObject::invokeMethod(dialog, "submit");
            wait_until([&] { return visible_text(dialog).contains("wrong"); });
            wrong_password_message = visible_text(dialog);

            line_edit_with_placeholder(dialog, "Current")->setText(QString::fromStdString(MASTER));
            line_edit_with_placeholder(dialog, "New master")->setText("brand new master");
            line_edit_with_placeholder(dialog, "Repeat")->setText("brand new master");
            QMetaObject::invokeMethod(dialog, "submit");
        });
        button(settings, "Change Master Password...")->click();
        settings_message = visible_text(settings);
        button(settings, "Cancel")->click();
    });
    action(window.get(), "Settings")->trigger();

    EXPECT_TRUE(wrong_password_message.contains("wrong")) << wrong_password_message.toStdString();
    EXPECT_TRUE(settings_message.contains("was changed")) << settings_message.toStdString();

    Vault saved = Vault::load(file);
    EXPECT_FALSE(saved.unlock(MASTER));
    ASSERT_TRUE(saved.unlock("brand new master"));
    EXPECT_EQ(saved.get_entries().size(), 3u);
}

TEST_F(MainWindowTest, ChangeMasterPasswordValidatesInput) {
    QStringList errors;
    when_dialog_shown("SettingsDialog", [&](QWidget* settings) {
        when_dialog_shown("ChangePasswordDialog", [&](QWidget* dialog) {
            auto attempt = [&](const QString& current, const QString& replacement, const QString& repeated) {
                line_edit_with_placeholder(dialog, "Current")->setText(current);
                line_edit_with_placeholder(dialog, "New master")->setText(replacement);
                line_edit_with_placeholder(dialog, "Repeat")->setText(repeated);
                QMetaObject::invokeMethod(dialog, "submit");
                errors << visible_text(dialog);
            };
            const QString master = QString::fromStdString(MASTER);
            attempt(master, "short", "short");
            attempt(master, "long enough 1", "long enough 2");
            attempt(master, master, master);
            button(dialog, "Cancel")->click();
        });
        button(settings, "Change Master Password...")->click();
        button(settings, "Cancel")->click();
    });
    action(window.get(), "Settings")->trigger();

    ASSERT_EQ(errors.size(), 3);
    EXPECT_TRUE(errors[0].contains("at least 8")) << errors[0].toStdString();
    EXPECT_TRUE(errors[1].contains("do not match")) << errors[1].toStdString();
    EXPECT_TRUE(errors[2].contains("same as the current")) << errors[2].toStdString();
    EXPECT_TRUE(reload().is_unlocked());  // the old password still opens the vault
}

TEST_F(MainWindowTest, EntryDialogShowsPasswordStrength) {
    int typed_score = -1, generated_score = -1;
    bool hidden_when_empty = false;
    on_next_dialog([&](QWidget* dialog) {
        auto* strength = dialog->findChild<StrengthBar*>();
        hidden_when_empty = !strength->isVisible();
        line_edits(dialog)[2]->setText("password");
        typed_score = strength->isVisible() ? strength->score() : -1;
        button(dialog, "Generate")->click();
        generated_score = strength->score();
        when_dialog_shown("MessageDialog", [](QWidget* box) { button(box, "Discard")->click(); });
        QMetaObject::invokeMethod(dialog, "reject");
    });
    action(window.get(), "Add")->trigger();

    EXPECT_TRUE(hidden_when_empty);
    EXPECT_EQ(typed_score, 0);
    EXPECT_EQ(generated_score, MAX_PASSWORD_STRENGTH);
}

TEST_F(MainWindowTest, CancelAsksBeforeDiscardingChanges) {
    // Without changes, Cancel closes right away (a question here would block the test).
    on_next_dialog([](QWidget* dialog) { QMetaObject::invokeMethod(dialog, "reject"); });
    action(window.get(), "Add")->trigger();

    select(list, "Gmail");
    bool open_after_keep_editing = false;
    bool closed_after_discard = false;
    on_next_dialog([&](QWidget* dialog) {
        line_edits(dialog)[1]->setText("someone-else@example.com");

        when_dialog_shown("MessageDialog", [](QWidget* box) { button(box, "Cancel")->click(); });
        QMetaObject::invokeMethod(dialog, "reject");
        open_after_keep_editing = dialog->isVisible();

        when_dialog_shown("MessageDialog", [](QWidget* box) { button(box, "Discard")->click(); });
        QMetaObject::invokeMethod(dialog, "reject");
        closed_after_discard = !dialog->isVisible();
    });
    action(window.get(), "Edit")->trigger();

    EXPECT_TRUE(open_after_keep_editing);
    EXPECT_TRUE(closed_after_discard);
    EXPECT_EQ(reload().find_entry("Gmail")->username, "me@example.com");
}

TEST_F(MainWindowTest, ExportCopiesVaultToFolder) {
    const QString folder = QString::fromStdString(path("exports"));
    window->set_export_folder(folder);
    QString message;
    when_dialog_shown("SettingsDialog", [&](QWidget* settings) {
        when_dialog_shown("MessageDialog", [&](QWidget* box) {
            message = visible_text(box);
            button(box, "Close")->click();
        });
        button(settings, "Export a Copy...")->click();
        button(settings, "Cancel")->click();
    });
    action(window.get(), "Settings")->trigger();

    const QStringList files = QDir(folder).entryList({"*.pwm"}, QDir::Files);
    ASSERT_EQ(files.size(), 1);
    EXPECT_TRUE(files[0].startsWith("PasswordManager-vault-")) << files[0].toStdString();
    EXPECT_TRUE(message.contains(files[0])) << message.toStdString();
    EXPECT_EQ(read_file(QDir(folder).filePath(files[0]).toStdString()), read_file(file));
}

TEST_F(MainWindowTest, ImportReplacesVaultAndKeepsBackup) {
    const std::string other = path("other computer.pwm");
    {
        Vault vault = Vault::create(other, "other computer pw");
        vault.add_entry({"Steam", "gamer", "st3am-Pass", "", ""});
        vault.save();
    }
    set_vault_file_chooser([&](QWidget*) { return QString::fromStdString(other); });

    QString import_title;
    QString info;
    when_dialog_shown("SettingsDialog", [&](QWidget* settings) {
        when_dialog_shown("UnlockDialog", [&](QWidget* dialog) {
            import_title = visible_text(dialog);
            when_dialog_shown("MessageDialog", [&](QWidget* confirm) {
                when_dialog_shown("MessageDialog", [&](QWidget* box) {
                    info = visible_text(box);
                    button(box, "OK")->click();
                });
                button(confirm, "Replace")->click();
            });
            line_edits(dialog)[0]->setText("other computer pw");
            QMetaObject::invokeMethod(dialog, "submit");
        });
        button(settings, "Import a Vault...")->click();
        button(settings, "Cancel")->click();
    });
    action(window.get(), "Settings")->trigger();

    EXPECT_TRUE(import_title.contains("Import a vault")) << import_title.toStdString();
    EXPECT_TRUE(info.contains("vault-backup-")) << info.toStdString();
    EXPECT_EQ(visible_names(list), QStringList{"Steam"});

    // The app's vault now opens with the imported password; the backup with the old one.
    Vault saved = Vault::load(file);
    ASSERT_TRUE(saved.unlock("other computer pw"));
    EXPECT_NE(saved.find_entry("Steam"), nullptr);

    const QStringList backups = QDir(QString::fromStdString(dir.string())).entryList({"vault-backup-*.pwm"});
    ASSERT_EQ(backups.size(), 1);
    Vault backup = Vault::load((dir / backups[0].toStdString()).string());
    ASSERT_TRUE(backup.unlock(MASTER));
    EXPECT_EQ(backup.get_entries().size(), 3u);
}

TEST_F(MainWindowTest, ImportRefusesFileThatIsNotAVault) {
    const std::string junk = path("notes.pwm");
    write_file(junk, "not a vault");
    set_vault_file_chooser([&](QWidget*) { return QString::fromStdString(junk); });

    QString error;
    when_dialog_shown("SettingsDialog", [&](QWidget* settings) {
        when_dialog_shown("MessageDialog", [&](QWidget* box) {
            error = visible_text(box);
            button(box, "OK")->click();
        });
        button(settings, "Import a Vault...")->click();
        button(settings, "Cancel")->click();
    });
    action(window.get(), "Settings")->trigger();

    EXPECT_TRUE(error.contains("not a Password Manager vault")) << error.toStdString();
    EXPECT_EQ(list->count(), 3);
    EXPECT_EQ(reload().get_entries().size(), 3u);
}
