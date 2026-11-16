#pragma once

#include <QList>
#include <QString>

// When the main window locks the vault by itself. These are settings of the application,
// not of one vault, so they are stored in settings.ini in the application data folder
// (on Windows %APPDATA%\PasswordManager) and never in the vault file.
struct LockSettings {
    int auto_lock_minutes = 5;         // without keyboard or mouse input; 0 = never
    bool lock_on_minimize = true;
    bool lock_on_session_lock = true;  // Windows: Win+L, switching user, lock before sleep

    static LockSettings load();
    void save() const;

    static QString file_path();
};

// The auto-lock times offered in the settings dialog, in minutes (0 = never).
const QList<int>& auto_lock_choices();
