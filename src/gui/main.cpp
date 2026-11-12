#include "main_window.h"

#include <sodium.h>
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

// Usage: PasswordManagerGui [vault file]
// Default vault location: %APPDATA%\PasswordManager\vault.pwm
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PasswordManager"));
    QApplication::setApplicationDisplayName(QStringLiteral("Password Manager"));

    if (sodium_init() < 0) {
        QMessageBox::critical(nullptr, QStringLiteral("Password Manager"),
                              QStringLiteral("Failed to initialize libsodium."));
        return 1;
    }

    QString vault_path;
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) {
        vault_path = arguments.at(1);
    } else {
        const QString data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(data_dir);
        vault_path = QDir(data_dir).filePath(QStringLiteral("vault.pwm"));
    }

    MainWindow window(vault_path);
    if (!window.unlock()) {
        return 0;
    }
    window.show();
    return QApplication::exec();
}
