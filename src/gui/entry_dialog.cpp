#include "entry_dialog.h"
#include "message_dialog.h"
#include "strength_bar.h"
#include "crypto/password_generator.h"

#include <sodium.h>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

EntryDialog::EntryDialog(const VaultEntry* existing,
                         std::function<bool(const std::string&)> name_taken_check,
                         QWidget* parent)
    : QDialog(parent), name_taken(std::move(name_taken_check)) {
    setWindowTitle(existing ? tr("Edit entry") : tr("Add entry"));
    setFixedWidth(500);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(4);

    auto* title = new QLabel(existing ? tr("Edit entry") : tr("New entry"), this);
    title->setObjectName(QStringLiteral("title"));
    layout->addWidget(title);
    layout->addSpacing(8);

    // Each field: a small label above the input.
    auto add_label = [&](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("field_label"));
        layout->addWidget(label);
    };

    add_label(tr("NAME"));
    name_edit = new QLineEdit(this);
    name_edit->setPlaceholderText(tr("e.g. Gmail"));
    layout->addWidget(name_edit);

    add_label(tr("USERNAME"));
    username_edit = new QLineEdit(this);
    username_edit->setPlaceholderText(tr("e.g. name@example.com"));
    layout->addWidget(username_edit);

    // Password with a show/hide toggle, then the generator on its own row.
    add_label(tr("PASSWORD"));
    password_edit = new QLineEdit(this);
    password_edit->setEchoMode(QLineEdit::Password);

    auto* show_button = new QToolButton(this);
    show_button->setText(tr("Show"));
    show_button->setCheckable(true);
    show_button->setCursor(Qt::PointingHandCursor);

    auto* password_row = new QHBoxLayout;
    password_row->setSpacing(6);
    password_row->addWidget(password_edit, 1);
    password_row->addWidget(show_button);
    layout->addLayout(password_row);

    auto* strength = new StrengthBar(this);
    layout->addWidget(strength);
    connect(password_edit, &QLineEdit::textChanged, strength, &StrengthBar::set_password);

    auto* length_label = new QLabel(tr("Length"), this);
    length_label->setObjectName(QStringLiteral("muted"));

    length_spin = new QSpinBox(this);
    length_spin->setRange(static_cast<int>(MIN_PASSWORD_LENGTH), static_cast<int>(MAX_PASSWORD_LENGTH));
    length_spin->setValue(static_cast<int>(DEFAULT_PASSWORD_LENGTH));
    length_spin->setToolTip(tr("Length of the generated password"));
    // No arrow buttons: type the number, or use the mouse wheel / arrow keys.
    length_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    length_spin->setAlignment(Qt::AlignCenter);
    length_spin->setFixedWidth(64);

    auto* generate_button = new QPushButton(tr("Generate"), this);
    generate_button->setCursor(Qt::PointingHandCursor);

    auto* generator_row = new QHBoxLayout;
    generator_row->setSpacing(8);
    generator_row->addStretch();
    generator_row->addWidget(length_label);
    generator_row->addWidget(length_spin);
    generator_row->addWidget(generate_button);
    layout->addSpacing(4);
    layout->addLayout(generator_row);

    add_label(tr("WEBSITE"));
    url_edit = new QLineEdit(this);
    url_edit->setPlaceholderText(QStringLiteral("https://"));
    layout->addWidget(url_edit);

    add_label(tr("NOTES"));
    notes_edit = new QPlainTextEdit(this);
    notes_edit->setTabChangesFocus(true);
    notes_edit->setFixedHeight(90);
    layout->addWidget(notes_edit);

    error_label = new QLabel(this);
    error_label->setObjectName(QStringLiteral("error"));
    error_label->setWordWrap(true);
    error_label->hide();
    layout->addSpacing(4);
    layout->addWidget(error_label);

    auto* cancel_button = new QPushButton(tr("Cancel"), this);
    auto* save_button = new QPushButton(tr("Save"), this);
    save_button->setObjectName(QStringLiteral("primary"));
    save_button->setDefault(true);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancel_button);
    buttons->addWidget(save_button);
    layout->addSpacing(12);
    layout->addLayout(buttons);

    connect(show_button, &QToolButton::toggled, this, &EntryDialog::toggle_password_visibility);
    connect(show_button, &QToolButton::toggled, show_button,
            [show_button](bool visible) { show_button->setText(visible ? tr("Hide") : tr("Show")); });
    connect(generate_button, &QPushButton::clicked, this, &EntryDialog::generate_password);
    connect(save_button, &QPushButton::clicked, this, &EntryDialog::validate_and_accept);
    connect(cancel_button, &QPushButton::clicked, this, &EntryDialog::reject);

    if (existing != nullptr) {
        original_name = existing->name;
        original_favorite = existing->favorite;
        original_tags = existing->tags;
        name_edit->setText(QString::fromStdString(existing->name));
        username_edit->setText(QString::fromStdString(existing->username));
        password_edit->setText(QString::fromStdString(existing->password));
        url_edit->setText(QString::fromStdString(existing->url));
        notes_edit->setPlainText(QString::fromStdString(existing->notes));
    }
    initial_values = field_values();
    name_edit->setFocus();
}

EntryDialog::~EntryDialog() {
    password_edit->clear();
    initial_values.clear();
}

QStringList EntryDialog::field_values() const {
    return {name_edit->text(), username_edit->text(), password_edit->text(), url_edit->text(),
            notes_edit->toPlainText()};
}

void EntryDialog::reject() {
    // The automatic lock closes dialogs without asking: the vault is locked either way.
    const bool locking = property("closing_for_lock").toBool();
    if (!locking && field_values() != initial_values &&
        !MessageDialog::confirm_dangerous(this, tr("Discard changes?"),
                                          tr("The changes to this entry have not been saved."),
                                          tr("Discard"))) {
        return;
    }
    QDialog::reject();
}

VaultEntry EntryDialog::entry() const {
    VaultEntry result;
    result.name = name_edit->text().trimmed().toStdString();
    result.username = username_edit->text().toStdString();
    result.password = password_edit->text().toStdString();
    result.url = url_edit->text().trimmed().toStdString();
    result.notes = notes_edit->toPlainText().toStdString();
    result.favorite = original_favorite;
    result.tags = original_tags;
    return result;
}

void EntryDialog::generate_password() {
    std::string password = ::generate_password(static_cast<size_t>(length_spin->value()));
    password_edit->setText(QString::fromStdString(password));
    sodium_memzero(password.data(), password.size());
}

void EntryDialog::toggle_password_visibility(bool visible) {
    password_edit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
}

void EntryDialog::validate_and_accept() {
    const std::string name = name_edit->text().trimmed().toStdString();
    QString error;

    if (name.empty()) {
        error = tr("The name cannot be empty.");
        name_edit->setFocus();
    } else if (name != original_name && name_taken(name)) {
        error = tr("Another entry is already called \"%1\".").arg(QString::fromStdString(name));
        name_edit->setFocus();
    } else if (password_edit->text().isEmpty()) {
        error = tr("The password cannot be empty. Type one or press Generate.");
        password_edit->setFocus();
    }

    if (!error.isEmpty()) {
        error_label->setText(error);
        error_label->show();
        return;
    }
    accept();
}
