#include "gui/main_window.h"
#include "gui/lock_settings.h"
#include "core/vault.h"
#include "gui_test_helpers.h"
#include "../test_helpers.h"

#include <QCheckBox>
#include <QClipboard>
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

// Like on_next_dialog, but without blocking the caller: polls until a modal dialog of
// the given class is shown. Needed when the dialog opens by itself (auto-lock) or from
// inside another dialog.
void when_dialog_shown(const char* class_name, std::function<void(QWidget*)> handle) {
    auto* timer = new QTimer;
    timer->setInterval(20);
    QObject::connect(timer, &QTimer::timeout, [timer, class_name, handle] {
        QWidget* modal = QApplication::activeModalWidget();
        if (modal != nullptr && modal->inherits(class_name) && modal->isVisible()) {
            timer->stop();
            timer->deleteLater();
            handle(modal);
        }
    });
    timer->start();
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
