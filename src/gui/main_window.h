#pragma once

#include "core/vault.h"

#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <memory>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QToolButton;

// Main window: searchable list of entries on the left, details of the selected one on the right.
// Every change is saved to disk immediately.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QString vault_path, QWidget* parent = nullptr);
    ~MainWindow() override;

    // Shows the unlock dialog. Returns false if the user cancelled it.
    bool unlock();

private slots:
    void add_entry();
    void edit_entry();
    void delete_entry();
    void lock();

    void copy_username();
    void copy_password();
    void copy_url();
    void clear_clipboard();

    void on_selection_changed();
    void apply_filter(const QString& text);

private:
    void build_ui();
    void reload_list(const QString& name_to_select = {});
    void show_entry(const VaultEntry* entry);
    [[nodiscard]] const VaultEntry* selected_entry() const;
    bool save();

    // Copies to the clipboard and schedules clearing it, so secrets do not stay there.
    void copy_secret(const QString& text, const QString& what);

    QString vault_path;
    std::shared_ptr<Vault> vault;

    QLineEdit* search_edit = nullptr;
    QListWidget* list = nullptr;

    QWidget* details = nullptr;
    QLabel* placeholder = nullptr;
    QLineEdit* name_value = nullptr;
    QLineEdit* username_value = nullptr;
    QLineEdit* password_value = nullptr;
    QToolButton* show_password_button = nullptr;
    QLabel* url_value = nullptr;
    QPlainTextEdit* notes_value = nullptr;

    QAction* edit_action = nullptr;
    QAction* delete_action = nullptr;
    QAction* copy_username_action = nullptr;
    QAction* copy_password_action = nullptr;

    QTimer clipboard_timer;
    QString copied_secret;
};
