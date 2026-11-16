#include "settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QString auto_lock_text(int minutes) {
    if (minutes == 0) {
        return QObject::tr("Never");
    }
    if (minutes == 60) {
        return QObject::tr("After 1 hour");
    }
    if (minutes == 1) {
        return QObject::tr("After 1 minute");
    }
    return QObject::tr("After %1 minutes").arg(minutes);
}

SettingsDialog::SettingsDialog(const LockSettings& current,
                               std::function<bool(QWidget*)> change_password,
                               QWidget* parent)
    : QDialog(parent), change_master_password(std::move(change_password)) {
    setWindowTitle(tr("Settings"));
    setFixedWidth(460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Settings"), this);
    title->setObjectName(QStringLiteral("title"));
    layout->addWidget(title);
    layout->addSpacing(6);

    auto add_section = [&](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("field_label"));
        layout->addWidget(label);
    };

    // ---- Automatic lock ----
    add_section(tr("AUTO-LOCK"));
    auto* idle_row = new QHBoxLayout;
    idle_row->addWidget(new QLabel(tr("When I am inactive:"), this));
    auto_lock_combo = new QComboBox(this);
    for (int minutes : auto_lock_choices()) {
        auto_lock_combo->addItem(auto_lock_text(minutes), minutes);
    }
    const int index = auto_lock_combo->findData(current.auto_lock_minutes);
    auto_lock_combo->setCurrentIndex(index >= 0 ? index : 0);
    idle_row->addWidget(auto_lock_combo, 1);
    layout->addLayout(idle_row);

    minimize_check = new QCheckBox(tr("Lock when the window is minimized"), this);
    minimize_check->setChecked(current.lock_on_minimize);
    layout->addWidget(minimize_check);

    session_lock_check = new QCheckBox(tr("Lock when the computer is locked (Win+L)"), this);
    session_lock_check->setChecked(current.lock_on_session_lock);
    layout->addWidget(session_lock_check);
#ifndef Q_OS_WIN
    session_lock_check->hide();  // only detected on Windows for now
#endif
    layout->addSpacing(10);

    // ---- Master password ----
    add_section(tr("MASTER PASSWORD"));
    auto* password_row = new QHBoxLayout;
    auto* change_button = new QPushButton(tr("Change Master Password..."), this);
    change_button->setAutoDefault(false);
    password_row->addWidget(change_button);
    password_row->addStretch();
    layout->addLayout(password_row);

    password_status = new QLabel(this);
    password_status->setObjectName(QStringLiteral("muted"));
    password_status->hide();
    layout->addWidget(password_status);
    layout->addSpacing(12);

    // ---- Buttons ----
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel_button = new QPushButton(tr("Cancel"), this);
    cancel_button->setAutoDefault(false);
    auto* save_button = new QPushButton(tr("Save"), this);
    save_button->setObjectName(QStringLiteral("primary"));
    save_button->setDefault(true);
    buttons->addWidget(cancel_button);
    buttons->addWidget(save_button);
    layout->addLayout(buttons);

    connect(change_button, &QPushButton::clicked, this, &SettingsDialog::on_change_master_password);
    connect(cancel_button, &QPushButton::clicked, this, &SettingsDialog::reject);
    connect(save_button, &QPushButton::clicked, this, &SettingsDialog::accept);
}

LockSettings SettingsDialog::settings() const {
    LockSettings result;
    result.auto_lock_minutes = auto_lock_combo->currentData().toInt();
    result.lock_on_minimize = minimize_check->isChecked();
    result.lock_on_session_lock = session_lock_check->isChecked();
    return result;
}

void SettingsDialog::on_change_master_password() {
    if (change_master_password && change_master_password(this)) {
        password_status->setText(tr("The master password was changed."));
        password_status->show();
    }
}
