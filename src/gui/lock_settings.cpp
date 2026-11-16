#include "lock_settings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

static const QString AUTO_LOCK_MINUTES_KEY = QStringLiteral("lock/auto_lock_minutes");
static const QString LOCK_ON_MINIMIZE_KEY = QStringLiteral("lock/on_minimize");
static const QString LOCK_ON_SESSION_LOCK_KEY = QStringLiteral("lock/on_session_lock");

const QList<int>& auto_lock_choices() {
    static const QList<int> choices = {1, 5, 15, 30, 60, 0};
    return choices;
}

QString LockSettings::file_path() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("settings.ini"));
}

LockSettings LockSettings::load() {
    const QSettings stored(file_path(), QSettings::IniFormat);
    LockSettings settings;

    // An edited or old file must not set a time the dialog cannot show.
    const int minutes = stored.value(AUTO_LOCK_MINUTES_KEY, settings.auto_lock_minutes).toInt();
    if (auto_lock_choices().contains(minutes)) {
        settings.auto_lock_minutes = minutes;
    }
    settings.lock_on_minimize = stored.value(LOCK_ON_MINIMIZE_KEY, settings.lock_on_minimize).toBool();
    settings.lock_on_session_lock = stored.value(LOCK_ON_SESSION_LOCK_KEY, settings.lock_on_session_lock).toBool();
    return settings;
}

void LockSettings::save() const {
    QDir().mkpath(QFileInfo(file_path()).absolutePath());
    QSettings stored(file_path(), QSettings::IniFormat);
    stored.setValue(AUTO_LOCK_MINUTES_KEY, auto_lock_minutes);
    stored.setValue(LOCK_ON_MINIMIZE_KEY, lock_on_minimize);
    stored.setValue(LOCK_ON_SESSION_LOCK_KEY, lock_on_session_lock);
    stored.sync();
}
