#include "unlock_dialog.h"
#include "strength_bar.h"
#include "window_activation.h"
#include "vault_import.h"

#include <sodium.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

static const QString REPOSITORY_URL = QStringLiteral("https://github.com/ivlad20/Password-manager");

// First name of the signed-in Windows / Linux user, for the greeting.
static QString user_name() {
    QString name = qEnvironmentVariable("USERNAME");
    if (name.isEmpty()) {
        name = qEnvironmentVariable("USER");
    }
    return name;
}

UnlockDialog::UnlockDialog(QString path, Purpose dialog_purpose, QWidget* parent)
    : QDialog(parent), vault_path(std::move(path)), purpose(dialog_purpose),
      create_mode(dialog_purpose == Purpose::OpenOrCreate && !QFileInfo::exists(vault_path)) {
    const bool importing = purpose == Purpose::Import;
    if (importing) {
        setWindowTitle(tr("Import a vault"));
    } else {
        setWindowTitle(create_mode ? tr("Create a new vault") : tr("Unlock vault"));
    }
    setFixedWidth(600);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 32, 40, 16);
    layout->setSpacing(10);

    // Greeting and explanation
    const QString name = user_name();
    auto* greeting = new QLabel(this);
    greeting->setObjectName(QStringLiteral("title"));
    greeting->setAlignment(Qt::AlignCenter);
    if (importing) {
        greeting->setText(tr("Import a vault"));
    } else if (create_mode) {
        greeting->setText(tr("Welcome!"));
    } else {
        greeting->setText(name.isEmpty() ? tr("Hi!") : tr("Hi %1!").arg(name));
    }
    layout->addWidget(greeting);

    auto* intro = new QLabel(this);
    intro->setWordWrap(true);
    intro->setAlignment(Qt::AlignCenter);
    if (importing) {
        intro->setText(tr("Enter the master password of the vault you want to import:"));
    } else {
        intro->setText(create_mode
                               ? tr("Choose a master password for your new vault.\n"
                                    "It cannot be recovered if you forget it.")
                               : tr("Enter the master password for:"));
    }
    layout->addWidget(intro);

    auto* path_label = new QLabel(QDir::toNativeSeparators(vault_path), this);
    path_label->setObjectName(QStringLiteral("muted"));
    path_label->setAlignment(Qt::AlignCenter);
    path_label->setWordWrap(true);
    path_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(path_label);
    layout->addSpacing(12);

    // Password fields, centered and narrower than the window
    auto add_centered = [&](QWidget* widget, int width) {
        widget->setFixedWidth(width);
        auto* row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(widget);
        row->addStretch();
        layout->addLayout(row);
    };

    password_edit = new QLineEdit(this);
    password_edit->setEchoMode(QLineEdit::Password);
    password_edit->setPlaceholderText(create_mode ? tr("New master password") : tr("Master password"));
    add_centered(password_edit, 340);

    if (create_mode) {
        // The master password protects everything else, so show how strong it is.
        // A weak one is allowed (above the minimum length), but the user sees the warning.
        strength = new StrengthBar(this);
        add_centered(strength, 340);
        connect(password_edit, &QLineEdit::textChanged, strength, &StrengthBar::set_password);
    }

    if (create_mode) {
        confirm_edit = new QLineEdit(this);
        confirm_edit->setEchoMode(QLineEdit::Password);
        confirm_edit->setPlaceholderText(tr("Repeat the master password"));
        add_centered(confirm_edit, 340);
    }

    status_label = new QLabel(this);
    status_label->setWordWrap(true);
    status_label->setAlignment(Qt::AlignCenter);
    status_label->hide();
    layout->addWidget(status_label);

    // An indeterminate progress bar while Argon2id runs.
    progress = new QProgressBar(this);
    progress->setRange(0, 0);
    progress->setTextVisible(false);
    progress->hide();
    add_centered(progress, 340);

    layout->addSpacing(8);
    QString submit_text = create_mode ? tr("Create Vault") : tr("Open Vault");
    if (importing) {
        submit_text = tr("Continue");
    }
    submit_button = new QPushButton(submit_text, this);
    submit_button->setObjectName(QStringLiteral("primary"));
    submit_button->setDefault(true);  // Enter in a password field presses it
    add_centered(submit_button, 160);

    if (create_mode) {
        // First start on a new computer: bring the vault from the old one instead.
        import_button = new QPushButton(tr("Import an Existing Vault..."), this);
        import_button->setAutoDefault(false);
        add_centered(import_button, 240);
        connect(import_button, &QPushButton::clicked, this, &UnlockDialog::import_existing);
    }
    layout->addSpacing(12);

    // Footer: link to the source code (not needed when importing from inside the app)
    if (!importing) {
        auto* footer = new QHBoxLayout;
        footer->setSpacing(6);
        if (QFile::exists(QStringLiteral(":/icons/github.png"))) {
            auto* icon = new QLabel(this);
            icon->setPixmap(QIcon(QStringLiteral(":/icons/github.png")).pixmap(16, 16));
            footer->addWidget(icon);
        }
        auto* link = new QLabel(this);
        link->setObjectName(QStringLiteral("muted"));
        link->setTextFormat(Qt::RichText);
        link->setOpenExternalLinks(true);
        link->setText(QStringLiteral("<a href=\"%1\">%2</a>")
                              .arg(REPOSITORY_URL, tr("Source code on GitHub")));
        footer->addWidget(link);
        footer->addStretch();
        layout->addLayout(footer);
    }

    connect(submit_button, &QPushButton::clicked, this, &UnlockDialog::submit);
    connect(&watcher, &QFutureWatcher<OpenResult>::finished, this, &UnlockDialog::on_finished);

    password_edit->setFocus();
}

void UnlockDialog::import_existing() {
    if (busy) {
        return;
    }
    std::shared_ptr<Vault> imported = pick_and_unlock_vault(this, vault_path);
    if (!imported) {
        return;
    }
    try {
        // There is no vault here yet, so nothing needs a backup: save the imported one
        // as this computer's vault.
        imported->save_to(vault_path.toStdString());
    } catch (const std::exception& error) {
        show_error(tr("The vault could not be imported:\n%1").arg(QString::fromStdString(error.what())));
        return;
    }
    unlocked_vault = imported;
    emit unlocked();
    accept();
}

void UnlockDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    // Open on top of other applications (unless it was shown minimized on purpose).
    QTimer::singleShot(0, this, [this] {
        if (isVisible() && !isMinimized()) {
            bring_to_front(this);
        }
    });
}

void UnlockDialog::reject() {
    // Closing while the worker thread runs could leave a half-created vault behind.
    if (busy) {
        return;
    }
    QDialog::reject();
}

void UnlockDialog::submit() {
    if (busy) {
        return;
    }

    std::string password = password_edit->text().toStdString();

    if (create_mode) {
        if (password_edit->text().size() < static_cast<qsizetype>(MIN_MASTER_PASSWORD_LENGTH)) {
            show_error(tr("The master password must have at least %1 characters.").arg(MIN_MASTER_PASSWORD_LENGTH));
            sodium_memzero(password.data(), password.size());
            return;
        }
        if (password_edit->text() != confirm_edit->text()) {
            show_error(tr("The passwords do not match."));
            confirm_edit->clear();
            confirm_edit->setFocus();
            sodium_memzero(password.data(), password.size());
            return;
        }
    }

    password_edit->clear();
    if (confirm_edit != nullptr) {
        confirm_edit->clear();
    }
    set_busy(true, create_mode ? tr("Creating the vault...") : tr("Unlocking..."));

    const bool create = create_mode;
    const std::string path = vault_path.toStdString();

    watcher.setFuture(QtConcurrent::run([path, password, create]() mutable {
        OpenResult result;
        try {
            if (create) {
                result.vault = std::make_shared<Vault>(Vault::create(path, password));
            } else {
                auto vault = std::make_shared<Vault>(Vault::load(path));
                if (vault->unlock(password)) {
                    result.vault = vault;
                } else {
                    result.error = QStringLiteral("Wrong master password.");
                }
            }
        } catch (const std::exception& error) {
            result.error = QString::fromStdString(error.what());
        }
        sodium_memzero(password.data(), password.size());
        return result;
    }));

    sodium_memzero(password.data(), password.size());
}

void UnlockDialog::on_finished() {
    OpenResult result = watcher.result();
    set_busy(false);

    if (result.vault) {
        unlocked_vault = result.vault;
        emit unlocked();
        accept();
        return;
    }
    show_error(result.error);
    password_edit->setFocus();
}

void UnlockDialog::set_busy(bool value, const QString& message) {
    busy = value;
    password_edit->setEnabled(!value);
    if (confirm_edit != nullptr) {
        confirm_edit->setEnabled(!value);
    }
    submit_button->setEnabled(!value);
    if (import_button != nullptr) {
        import_button->setEnabled(!value);
    }
    progress->setVisible(value);
    if (value) {
        status_label->setStyleSheet(QStringLiteral("color: #6B7078;"));
        status_label->setText(message);
        status_label->show();
    }
}

void UnlockDialog::show_error(const QString& message) {
    status_label->setStyleSheet(QStringLiteral("color: #C93C34;"));
    status_label->setText(message);
    status_label->show();
}
