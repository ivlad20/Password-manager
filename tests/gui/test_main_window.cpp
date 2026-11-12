#include "gui/main_window.h"
#include "core/vault.h"
#include "gui_test_helpers.h"
#include "../test_helpers.h"

#include <QClipboard>
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
        TempDirTest::TearDown();
    }

    // Reads the file again from disk, to check that changes were saved.
    Vault reload() const {
        Vault vault = Vault::load(file);
        EXPECT_TRUE(vault.unlock(MASTER));
        return vault;
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

    on_next_dialog([](QWidget* box) { button(box, "No")->click(); });
    action(window.get(), "Delete")->trigger();
    EXPECT_EQ(list->count(), 3);

    on_next_dialog([](QWidget* box) { button(box, "Yes")->click(); });
    action(window.get(), "Delete")->trigger();
    EXPECT_EQ(visible_names(list), (QStringList{"bank", "Gmail"}));
    EXPECT_EQ(reload().find_entry("GitHub"), nullptr);
}
