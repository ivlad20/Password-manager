#include "main_window.h"
#include "entry_dialog.h"
#include "unlock_dialog.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStackedLayout>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

static const int CLIPBOARD_CLEAR_SECONDS = 30;

MainWindow::MainWindow(QString path, QWidget* parent)
    : QMainWindow(parent), vault_path(std::move(path)) {
    setWindowTitle(tr("Password Manager"));
    resize(900, 560);
    build_ui();

    clipboard_timer.setSingleShot(true);
    connect(&clipboard_timer, &QTimer::timeout, this, &MainWindow::clear_clipboard);
}

MainWindow::~MainWindow() {
    clear_clipboard();
}

void MainWindow::build_ui() {
    // Toolbar and keyboard shortcuts
    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto* add_action = toolbar->addAction(style()->standardIcon(QStyle::SP_FileIcon), tr("Add"));
    add_action->setShortcut(QKeySequence::New);
    edit_action = toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Edit"));
    edit_action->setShortcut(QKeySequence(Qt::Key_F2));
    delete_action = toolbar->addAction(style()->standardIcon(QStyle::SP_TrashIcon), tr("Delete"));
    delete_action->setShortcut(QKeySequence::Delete);
    toolbar->addSeparator();
    copy_username_action = toolbar->addAction(tr("Copy username"));
    copy_username_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    copy_password_action = toolbar->addAction(tr("Copy password"));
    copy_password_action->setShortcut(QKeySequence::Copy);
    toolbar->addSeparator();
    auto* lock_action = toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserStop), tr("Lock"));
    lock_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    for (QAction* action : toolbar->actions()) {
        if (!action->shortcut().isEmpty()) {
            action->setToolTip(QStringLiteral("%1 (%2)").arg(action->text(),
                                                             action->shortcut().toString(QKeySequence::NativeText)));
        }
    }

    connect(add_action, &QAction::triggered, this, &MainWindow::add_entry);
    connect(edit_action, &QAction::triggered, this, &MainWindow::edit_entry);
    connect(delete_action, &QAction::triggered, this, &MainWindow::delete_entry);
    connect(copy_username_action, &QAction::triggered, this, &MainWindow::copy_username);
    connect(copy_password_action, &QAction::triggered, this, &MainWindow::copy_password);
    connect(lock_action, &QAction::triggered, this, &MainWindow::lock);

    // Left side: search box and list of entries
    auto* left = new QWidget(this);
    auto* left_layout = new QVBoxLayout(left);
    left_layout->setContentsMargins(0, 0, 0, 0);

    search_edit = new QLineEdit(left);
    search_edit->setPlaceholderText(tr("Search (Ctrl+F)"));
    search_edit->setClearButtonEnabled(true);
    left_layout->addWidget(search_edit);

    auto* focus_search = new QAction(this);
    focus_search->setShortcut(QKeySequence::Find);
    addAction(focus_search);
    connect(focus_search, &QAction::triggered, search_edit, [this] {
        search_edit->setFocus();
        search_edit->selectAll();
    });

    list = new QListWidget(left);
    left_layout->addWidget(list);

    connect(search_edit, &QLineEdit::textChanged, this, &MainWindow::apply_filter);
    connect(list, &QListWidget::currentItemChanged, this, &MainWindow::on_selection_changed);
    connect(list, &QListWidget::itemDoubleClicked, this, &MainWindow::copy_password);

    // Right side: details of the selected entry (or a hint when nothing is selected)
    auto* right = new QWidget(this);
    auto* stack = new QStackedLayout(right);

    placeholder = new QLabel(tr("Select an entry, or press Add to create one."), right);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral("color: gray;"));
    stack->addWidget(placeholder);

    details = new QWidget(right);
    auto* form = new QFormLayout(details);

    auto make_readonly = [](QWidget* owner) {
        auto* edit = new QLineEdit(owner);
        edit->setReadOnly(true);
        edit->setFrame(false);
        return edit;
    };

    name_value = make_readonly(details);
    QFont title_font = name_value->font();
    title_font.setPointSizeF(title_font.pointSizeF() * 1.4);
    title_font.setBold(true);
    name_value->setFont(title_font);
    form->addRow(name_value);

    username_value = make_readonly(details);
    auto* copy_user_button = new QToolButton(details);
    copy_user_button->setText(tr("Copy"));
    auto* user_row = new QHBoxLayout;
    user_row->addWidget(username_value, 1);
    user_row->addWidget(copy_user_button);
    form->addRow(tr("Username:"), user_row);

    password_value = make_readonly(details);
    password_value->setEchoMode(QLineEdit::Password);
    show_password_button = new QToolButton(details);
    show_password_button->setText(tr("Show"));
    show_password_button->setCheckable(true);
    auto* copy_pass_button = new QToolButton(details);
    copy_pass_button->setText(tr("Copy"));
    auto* pass_row = new QHBoxLayout;
    pass_row->addWidget(password_value, 1);
    pass_row->addWidget(show_password_button);
    pass_row->addWidget(copy_pass_button);
    form->addRow(tr("Password:"), pass_row);

    url_value = new QLabel(details);
    url_value->setOpenExternalLinks(true);
    url_value->setTextInteractionFlags(Qt::TextBrowserInteraction);
    auto* copy_url_button = new QToolButton(details);
    copy_url_button->setText(tr("Copy"));
    auto* url_row = new QHBoxLayout;
    url_row->addWidget(url_value, 1);
    url_row->addWidget(copy_url_button);
    form->addRow(tr("URL:"), url_row);

    notes_value = new QPlainTextEdit(details);
    notes_value->setReadOnly(true);
    form->addRow(tr("Notes:"), notes_value);

    stack->addWidget(details);

    connect(copy_user_button, &QToolButton::clicked, this, &MainWindow::copy_username);
    connect(copy_pass_button, &QToolButton::clicked, this, &MainWindow::copy_password);
    connect(copy_url_button, &QToolButton::clicked, this, &MainWindow::copy_url);
    connect(show_password_button, &QToolButton::toggled, this, [this](bool visible) {
        password_value->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
        show_password_button->setText(visible ? tr("Hide") : tr("Show"));
    });

    auto* splitter = new QSplitter(this);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({280, 620});
    setCentralWidget(splitter);

    statusBar();
    show_entry(nullptr);
}

bool MainWindow::unlock() {
    UnlockDialog dialog(vault_path, this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    vault = dialog.vault();
    reload_list();
    statusBar()->showMessage(tr("Vault unlocked: %n entries", nullptr,
                                static_cast<int>(vault->get_entries().size())), 5000);
    return true;
}

void MainWindow::reload_list(const QString& name_to_select) {
    // Sort a list of pointers so the vault itself keeps its order.
    std::vector<const VaultEntry*> sorted;
    for (const VaultEntry& entry : vault->get_entries()) {
        sorted.push_back(&entry);
    }
    std::sort(sorted.begin(), sorted.end(), [](const VaultEntry* a, const VaultEntry* b) {
        return QString::fromStdString(a->name).compare(QString::fromStdString(b->name), Qt::CaseInsensitive) < 0;
    });

    list->blockSignals(true);
    list->clear();
    for (const VaultEntry* entry : sorted) {
        auto* item = new QListWidgetItem(QString::fromStdString(entry->name), list);
        // Hidden text used by the search box: name, username and URL.
        item->setData(Qt::UserRole, QString::fromStdString(entry->name + "\n" + entry->username + "\n" + entry->url));
        if (!entry->username.empty()) {
            item->setToolTip(QString::fromStdString(entry->username));
        }
    }
    list->blockSignals(false);

    apply_filter(search_edit->text());

    if (!name_to_select.isEmpty()) {
        const auto matches = list->findItems(name_to_select, Qt::MatchExactly);
        if (!matches.isEmpty()) {
            list->setCurrentItem(matches.first());
        }
    }
    on_selection_changed();
}

void MainWindow::apply_filter(const QString& text) {
    const QString needle = text.trimmed();
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        const QString haystack = item->data(Qt::UserRole).toString();
        item->setHidden(!needle.isEmpty() && !haystack.contains(needle, Qt::CaseInsensitive));
    }
    // Do not keep showing an entry that the filter just hid.
    if (list->currentItem() != nullptr && list->currentItem()->isHidden()) {
        list->setCurrentItem(nullptr);
    }
}

const VaultEntry* MainWindow::selected_entry() const {
    if (!vault || !vault->is_unlocked()) {
        return nullptr;
    }
    QListWidgetItem* item = list->currentItem();
    if (item == nullptr || item->isHidden()) {
        return nullptr;
    }
    return vault->find_entry(item->text().toStdString());
}

void MainWindow::on_selection_changed() {
    show_entry(selected_entry());
}

void MainWindow::show_entry(const VaultEntry* entry) {
    const bool has_entry = entry != nullptr;
    edit_action->setEnabled(has_entry);
    delete_action->setEnabled(has_entry);
    copy_username_action->setEnabled(has_entry);
    copy_password_action->setEnabled(has_entry);

    auto* stack = qobject_cast<QStackedLayout*>(details->parentWidget()->layout());
    show_password_button->setChecked(false);

    if (!has_entry) {
        stack->setCurrentWidget(placeholder);
        name_value->clear();
        username_value->clear();
        password_value->clear();
        url_value->clear();
        notes_value->clear();
        return;
    }

    name_value->setText(QString::fromStdString(entry->name));
    username_value->setText(QString::fromStdString(entry->username));
    password_value->setText(QString::fromStdString(entry->password));
    notes_value->setPlainText(QString::fromStdString(entry->notes));

    // Only http(s) addresses become clickable links; everything else is shown as plain text.
    const QString url = QString::fromStdString(entry->url);
    if (url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        url_value->setTextFormat(Qt::RichText);
        url_value->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(url.toHtmlEscaped(), url.toHtmlEscaped()));
    } else {
        url_value->setTextFormat(Qt::PlainText);
        url_value->setText(url);
    }

    stack->setCurrentWidget(details);
}

bool MainWindow::save() {
    try {
        vault->save();
        return true;
    } catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Save failed"),
                              tr("The change could not be written to disk:\n%1").arg(QString::fromStdString(error.what())));
        return false;
    }
}

void MainWindow::add_entry() {
    EntryDialog dialog(nullptr, [this](const std::string& name) { return vault->find_entry(name) != nullptr; }, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    VaultEntry entry = dialog.entry();
    vault->add_entry(entry);
    if (save()) {
        statusBar()->showMessage(tr("Entry added."), 3000);
    }
    search_edit->clear();
    reload_list(QString::fromStdString(entry.name));
}

void MainWindow::edit_entry() {
    const VaultEntry* existing = selected_entry();
    if (existing == nullptr) {
        return;
    }
    const std::string old_name = existing->name;

    EntryDialog dialog(existing, [this](const std::string& name) { return vault->find_entry(name) != nullptr; }, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    VaultEntry updated = dialog.entry();
    vault->update_entry(old_name, updated);
    if (save()) {
        statusBar()->showMessage(tr("Entry updated."), 3000);
    }
    reload_list(QString::fromStdString(updated.name));
}

void MainWindow::delete_entry() {
    const VaultEntry* entry = selected_entry();
    if (entry == nullptr) {
        return;
    }
    const QString name = QString::fromStdString(entry->name);
    const auto answer = QMessageBox::question(this, tr("Delete entry"),
                                              tr("Delete \"%1\"? This cannot be undone.").arg(name),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    vault->remove_entry(entry->name);
    if (save()) {
        statusBar()->showMessage(tr("Entry deleted."), 3000);
    }
    reload_list();
}

void MainWindow::lock() {
    clear_clipboard();
    list->clear();
    show_entry(nullptr);
    search_edit->clear();
    vault.reset();  // the Vault destructor wipes the key and the entries
    hide();

    if (unlock()) {
        show();
    } else {
        QApplication::quit();
    }
}

void MainWindow::copy_username() {
    if (const VaultEntry* entry = selected_entry()) {
        copy_secret(QString::fromStdString(entry->username), tr("Username"));
    }
}

void MainWindow::copy_password() {
    if (const VaultEntry* entry = selected_entry()) {
        copy_secret(QString::fromStdString(entry->password), tr("Password"));
    }
}

void MainWindow::copy_url() {
    if (const VaultEntry* entry = selected_entry()) {
        copy_secret(QString::fromStdString(entry->url), tr("URL"));
    }
}

void MainWindow::copy_secret(const QString& text, const QString& what) {
    if (text.isEmpty()) {
        return;
    }
    auto* mime = new QMimeData;
    mime->setText(text);
#ifdef Q_OS_WIN
    // Ask Windows not to keep this in the clipboard history (Win+V) or sync it to the cloud.
    const QByteArray zero(4, '\0');
    mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"ExcludeClipboardContentFromMonitorProcessing\""), zero);
    mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"CanIncludeInClipboardHistory\""), zero);
    mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"CanUploadToCloudClipboard\""), zero);
#endif
    QGuiApplication::clipboard()->setMimeData(mime);  // the clipboard takes ownership

    copied_secret = text;
    clipboard_timer.start(CLIPBOARD_CLEAR_SECONDS * 1000);
    statusBar()->showMessage(tr("%1 copied. The clipboard will be cleared in %2 seconds.")
                                 .arg(what).arg(CLIPBOARD_CLEAR_SECONDS),
                             CLIPBOARD_CLEAR_SECONDS * 1000);
}

void MainWindow::clear_clipboard() {
    clipboard_timer.stop();
    if (copied_secret.isEmpty()) {
        return;
    }
    // Only clear it if it still holds what we copied; the user may have copied something else since.
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard->text() == copied_secret) {
        clipboard->clear();
    }
    copied_secret.fill(QChar(0));
    copied_secret.clear();
}
