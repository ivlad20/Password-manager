#include "main_window.h"
#include "change_password_dialog.h"
#include "entry_dialog.h"
#include "message_dialog.h"
#include "settings_dialog.h"
#include "unlock_dialog.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QStackedLayout>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wtsapi32.h>
#endif

static const int CLIPBOARD_CLEAR_SECONDS = 30;

// Local date and time, or "unknown" for entries migrated from format version 1.
static QString format_timestamp(uint64_t seconds) {
    if (seconds == 0) {
        return QObject::tr("unknown");
    }
    const QDateTime time = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds));
    return QLocale().toString(time, QLocale::ShortFormat);
}

MainWindow::MainWindow(QString path, QWidget* parent)
    : QMainWindow(parent), vault_path(std::move(path)) {
    setWindowTitle(tr("Password Manager"));
    resize(980, 620);
    setMinimumSize(820, 520);
    build_ui();

    clipboard_timer.setSingleShot(true);
    connect(&clipboard_timer, &QTimer::timeout, this, &MainWindow::clear_clipboard);

    // Any keyboard or mouse input in the application restarts the inactivity timer.
    idle_timer.setSingleShot(true);
    connect(&idle_timer, &QTimer::timeout, this, [this] { request_lock(LockReason::Idle); });
    qApp->installEventFilter(this);
    apply_lock_settings(LockSettings::load());

#ifdef Q_OS_WIN
    // Ask Windows to tell us when the session is locked (Win+L). Only with the real
    // Windows platform: the tests run on the "offscreen" one, which has no HWND.
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        session_notifications =
                WTSRegisterSessionNotification(reinterpret_cast<HWND>(winId()), NOTIFY_FOR_THIS_SESSION) != FALSE;
    }
#endif
}

MainWindow::~MainWindow() {
#ifdef Q_OS_WIN
    if (session_notifications) {
        WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
    }
#endif
    clear_clipboard();
}

void MainWindow::apply_lock_settings(const LockSettings& settings) {
    lock_settings = settings;
    set_idle_timeout_ms(settings.auto_lock_minutes * 60 * 1000);
}

void MainWindow::set_idle_timeout_ms(int milliseconds) {
    idle_timeout_ms = milliseconds;
    restart_idle_timer();
}

void MainWindow::restart_idle_timer() {
    if (vault && idle_timeout_ms > 0) {
        idle_timer.start(idle_timeout_ms);
    } else {
        idle_timer.stop();
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
            if (idle_timer.isActive()) {
                idle_timer.start();  // restarts with the same interval
            }
            break;
        default:
            break;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized() && vault && lock_settings.lock_on_minimize) {
        // Not in the middle of the state change itself.
        QTimer::singleShot(0, this, [this] { request_lock(LockReason::Minimized); });
    }
}

bool MainWindow::nativeEvent(const QByteArray& event_type, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (event_type == "windows_generic_MSG") {
        const auto* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_WTSSESSION_CHANGE && msg->wParam == WTS_SESSION_LOCK &&
            lock_settings.lock_on_session_lock) {
            QTimer::singleShot(0, this, [this] { request_lock(LockReason::SessionLocked); });
        }
    }
#endif
    return QMainWindow::nativeEvent(event_type, message, result);
}

void MainWindow::request_lock(LockReason reason) {
    if (!vault) {
        return;  // already locked
    }

    // Close the open dialogs, newest first. A dialog that is busy (for example while it
    // re-encrypts the vault) ignores the close and stays the active modal widget.
    QWidget* previous = nullptr;
    while (QWidget* modal = QApplication::activeModalWidget()) {
        if (modal == previous) {
            break;
        }
        previous = modal;
        modal->close();
    }
    if (QApplication::activeModalWidget() != nullptr) {
        QTimer::singleShot(1000, this, [this, reason] { request_lock(reason); });
        return;
    }
    // Let the closed dialogs' exec() calls return first.
    QTimer::singleShot(0, this, [this, reason] { lock_now(reason); });
}

// A light gray rounded container (styled through "QFrame#panel" in style.qss).
static QFrame* make_panel(QWidget* parent) {
    auto* panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("panel"));
    return panel;
}

static QLabel* make_field_label(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("field_label"));
    return label;
}

static QToolButton* make_small_button(const QString& text, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("small"));
    button->setText(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void MainWindow::build_ui() {
    // Actions with keyboard shortcuts. They are shown as buttons in the right panel.
    auto* add_action = new QAction(tr("Add"), this);
    add_action->setShortcut(QKeySequence::New);
    edit_action = new QAction(tr("Edit"), this);
    edit_action->setShortcut(QKeySequence(Qt::Key_F2));
    delete_action = new QAction(tr("Delete"), this);
    delete_action->setShortcut(QKeySequence::Delete);
    copy_username_action = new QAction(tr("Copy username"), this);
    copy_username_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    copy_password_action = new QAction(tr("Copy password"), this);
    copy_password_action->setShortcut(QKeySequence::Copy);
    auto* settings_action = new QAction(tr("Settings"), this);
    settings_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    auto* lock_action = new QAction(tr("Lock"), this);
    lock_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

    const QList<QAction*> actions = {add_action, edit_action, delete_action, copy_username_action,
                                     copy_password_action, settings_action, lock_action};
    for (QAction* action : actions) {
        action->setToolTip(QStringLiteral("%1 (%2)").arg(action->text(),
                                                         action->shortcut().toString(QKeySequence::NativeText)));
        addAction(action);  // shortcuts work anywhere in the window
    }

    connect(add_action, &QAction::triggered, this, &MainWindow::add_entry);
    connect(edit_action, &QAction::triggered, this, &MainWindow::edit_entry);
    connect(delete_action, &QAction::triggered, this, &MainWindow::delete_entry);
    connect(copy_username_action, &QAction::triggered, this, &MainWindow::copy_username);
    connect(copy_password_action, &QAction::triggered, this, &MainWindow::copy_password);
    connect(settings_action, &QAction::triggered, this, &MainWindow::open_settings);
    connect(lock_action, &QAction::triggered, this, &MainWindow::lock);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto* columns = new QHBoxLayout(central);
    columns->setContentsMargins(16, 16, 16, 8);
    columns->setSpacing(16);

    // ---- Left panel: search box and list of entries ----
    auto* left = make_panel(central);
    auto* left_layout = new QVBoxLayout(left);
    left_layout->setContentsMargins(12, 12, 12, 12);
    left_layout->setSpacing(10);

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
    list->setCursor(Qt::PointingHandCursor);
    left_layout->addWidget(list);

    connect(search_edit, &QLineEdit::textChanged, this, &MainWindow::apply_filter);
    connect(list, &QListWidget::currentItemChanged, this, &MainWindow::on_selection_changed);
    connect(list, &QListWidget::itemDoubleClicked, this, &MainWindow::copy_password);

    // ---- Middle panel: details of the selected entry (or a hint when nothing is selected) ----
    auto* middle = make_panel(central);
    auto* stack = new QStackedLayout(middle);

    placeholder = new QLabel(tr("Select an entry, or press Add to create one."), middle);
    placeholder->setObjectName(QStringLiteral("placeholder"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    stack->addWidget(placeholder);

    details = new QWidget(middle);
    auto* form = new QVBoxLayout(details);
    form->setContentsMargins(20, 18, 20, 20);
    form->setSpacing(6);

    auto make_value = [](QWidget* owner) {
        auto* edit = new QLineEdit(owner);
        edit->setObjectName(QStringLiteral("value"));
        edit->setReadOnly(true);
        return edit;
    };

    name_value = new QLineEdit(details);
    name_value->setObjectName(QStringLiteral("entry_title"));
    name_value->setReadOnly(true);
    form->addWidget(name_value);
    form->addSpacing(6);

    // Each field: a small label above, then the value with its buttons on one row.
    auto add_row = [&](const QString& label, QWidget* value, std::initializer_list<QWidget*> buttons) {
        form->addWidget(make_field_label(label, details));
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        row->addWidget(value, 1);
        for (QWidget* button : buttons) {
            row->addWidget(button);
        }
        form->addLayout(row);
    };

    username_value = make_value(details);
    auto* copy_user_button = make_small_button(tr("Copy"), details);
    add_row(tr("USERNAME"), username_value, {copy_user_button});

    password_value = make_value(details);
    password_value->setEchoMode(QLineEdit::Password);
    show_password_button = make_small_button(tr("Show"), details);
    show_password_button->setCheckable(true);
    auto* copy_pass_button = make_small_button(tr("Copy"), details);
    add_row(tr("PASSWORD"), password_value, {show_password_button, copy_pass_button});

    url_value = new QLabel(details);
    url_value->setObjectName(QStringLiteral("value"));
    url_value->setOpenExternalLinks(true);
    url_value->setTextInteractionFlags(Qt::TextBrowserInteraction);
    auto* copy_url_button = make_small_button(tr("Copy"), details);
    add_row(tr("WEBSITE"), url_value, {copy_url_button});

    form->addWidget(make_field_label(tr("NOTES"), details));
    notes_value = new QPlainTextEdit(details);
    notes_value->setObjectName(QStringLiteral("value"));
    notes_value->setReadOnly(true);
    form->addWidget(notes_value, 1);

    dates_value = new QLabel(details);
    dates_value->setObjectName(QStringLiteral("muted"));
    form->addWidget(dates_value);

    stack->addWidget(details);

    connect(copy_user_button, &QToolButton::clicked, this, &MainWindow::copy_username);
    connect(copy_pass_button, &QToolButton::clicked, this, &MainWindow::copy_password);
    connect(copy_url_button, &QToolButton::clicked, this, &MainWindow::copy_url);
    connect(show_password_button, &QToolButton::toggled, this, [this](bool visible) {
        password_value->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
        show_password_button->setText(visible ? tr("Hide") : tr("Show"));
    });

    // ---- Right panel: action buttons ----
    auto* right = make_panel(central);
    right->setFixedWidth(200);
    auto* right_layout = new QVBoxLayout(right);
    right_layout->setContentsMargins(20, 20, 20, 20);
    right_layout->setSpacing(12);

    auto add_action_button = [&](QAction* action, const QString& style) {
        auto* button = new QToolButton(right);
        button->setDefaultAction(action);  // text, tooltip and enabled state follow the action
        button->setObjectName(style);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMinimumHeight(40);
        button->setCursor(Qt::PointingHandCursor);
        right_layout->addWidget(button);
    };

    right_layout->addStretch();
    add_action_button(add_action, QStringLiteral("primary"));
    add_action_button(edit_action, QStringLiteral("primary"));
    add_action_button(copy_username_action, QStringLiteral("primary"));
    add_action_button(copy_password_action, QStringLiteral("primary"));
    add_action_button(delete_action, QStringLiteral("danger"));
    right_layout->addStretch();
    add_action_button(settings_action, QStringLiteral("secondary"));
    add_action_button(lock_action, QStringLiteral("secondary"));

    columns->addWidget(left, 3);
    columns->addWidget(middle, 4);
    columns->addWidget(right);
    setCentralWidget(central);

    statusBar()->setSizeGripEnabled(false);
    show_entry(nullptr);
}

bool MainWindow::unlock() {
    return show_unlock_dialog(false);
}

bool MainWindow::show_unlock_dialog(bool minimized) {
    // No parent: while the main window is hidden the dialog needs its own taskbar button.
    UnlockDialog dialog(vault_path);
    if (minimized) {
        // Locked because the window was minimized: do not pop up on the screen.
        dialog.setWindowState(Qt::WindowMinimized);
    }
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    vault = dialog.vault();
    reload_list();
    statusBar()->showMessage(tr("Vault unlocked: %n entries", nullptr,
                                static_cast<int>(vault->get_entries().size())), 5000);
    restart_idle_timer();
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
        dates_value->clear();
        return;
    }

    name_value->setText(QString::fromStdString(entry->name));
    username_value->setText(QString::fromStdString(entry->username));
    password_value->setText(QString::fromStdString(entry->password));
    notes_value->setPlainText(QString::fromStdString(entry->notes));
    dates_value->setText(tr("Created: %1    Modified: %2")
                                 .arg(format_timestamp(entry->created_at),
                                      format_timestamp(entry->modified_at)));

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
        MessageDialog::error(this, tr("Save failed"),
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
    if (!MessageDialog::confirm_dangerous(this, tr("Delete entry"),
                                          tr("Delete \"%1\"? This cannot be undone.").arg(name),
                                          tr("Delete"))) {
        return;
    }
    vault->remove_entry(entry->name);
    if (save()) {
        statusBar()->showMessage(tr("Entry deleted."), 3000);
    }
    reload_list();
}

void MainWindow::lock() {
    lock_now(LockReason::Manual);
}

void MainWindow::lock_now(LockReason reason) {
    if (!vault) {
        return;
    }
    idle_timer.stop();
    clear_clipboard();
    list->clear();
    show_entry(nullptr);
    search_edit->clear();
    vault.reset();  // the Vault destructor wipes the key and the entries
    hide();

    if (show_unlock_dialog(reason == LockReason::Minimized)) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
        raise();
        activateWindow();
    } else {
        QApplication::quit();
    }
}

void MainWindow::open_settings() {
    SettingsDialog dialog(lock_settings, [this](QWidget* parent) {
        ChangePasswordDialog change(vault, parent);
        return change.exec() == QDialog::Accepted;
    }, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const LockSettings updated = dialog.settings();
    updated.save();
    apply_lock_settings(updated);
    statusBar()->showMessage(tr("Settings saved."), 3000);
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
