#include "entry_dialog.h"
#include "crypto/password_generator.h"

#include <sodium.h>
#include <QDialogButtonBox>
#include <QFormLayout>
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
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    name_edit = new QLineEdit(this);
    name_edit->setPlaceholderText(tr("e.g. Gmail"));
    form->addRow(tr("Name:"), name_edit);

    username_edit = new QLineEdit(this);
    form->addRow(tr("Username:"), username_edit);

    // Password field with a show/hide toggle, plus a generator with a length selector.
    password_edit = new QLineEdit(this);
    password_edit->setEchoMode(QLineEdit::Password);

    auto* show_button = new QToolButton(this);
    show_button->setText(tr("Show"));
    show_button->setCheckable(true);

    length_spin = new QSpinBox(this);
    length_spin->setRange(static_cast<int>(MIN_PASSWORD_LENGTH), static_cast<int>(MAX_PASSWORD_LENGTH));
    length_spin->setValue(static_cast<int>(DEFAULT_PASSWORD_LENGTH));
    length_spin->setToolTip(tr("Length of the generated password"));

    auto* generate_button = new QPushButton(tr("Generate"), this);

    auto* password_row = new QHBoxLayout;
    password_row->addWidget(password_edit, 1);
    password_row->addWidget(show_button);
    password_row->addWidget(length_spin);
    password_row->addWidget(generate_button);
    form->addRow(tr("Password:"), password_row);

    url_edit = new QLineEdit(this);
    url_edit->setPlaceholderText(QStringLiteral("https://"));
    form->addRow(tr("URL:"), url_edit);

    notes_edit = new QPlainTextEdit(this);
    notes_edit->setTabChangesFocus(true);
    notes_edit->setFixedHeight(90);
    form->addRow(tr("Notes:"), notes_edit);

    layout->addLayout(form);

    error_label = new QLabel(this);
    error_label->setStyleSheet(QStringLiteral("color: #c0392b;"));
    error_label->hide();
    layout->addWidget(error_label);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(show_button, &QToolButton::toggled, this, &EntryDialog::toggle_password_visibility);
    connect(show_button, &QToolButton::toggled, show_button,
            [show_button](bool visible) { show_button->setText(visible ? tr("Hide") : tr("Show")); });
    connect(generate_button, &QPushButton::clicked, this, &EntryDialog::generate_password);
    connect(buttons, &QDialogButtonBox::accepted, this, &EntryDialog::validate_and_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &EntryDialog::reject);

    if (existing != nullptr) {
        original_name = existing->name;
        name_edit->setText(QString::fromStdString(existing->name));
        username_edit->setText(QString::fromStdString(existing->username));
        password_edit->setText(QString::fromStdString(existing->password));
        url_edit->setText(QString::fromStdString(existing->url));
        notes_edit->setPlainText(QString::fromStdString(existing->notes));
    }
    name_edit->setFocus();
}

EntryDialog::~EntryDialog() {
    password_edit->clear();
}

VaultEntry EntryDialog::entry() const {
    VaultEntry result;
    result.name = name_edit->text().trimmed().toStdString();
    result.username = username_edit->text().toStdString();
    result.password = password_edit->text().toStdString();
    result.url = url_edit->text().trimmed().toStdString();
    result.notes = notes_edit->toPlainText().toStdString();
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
