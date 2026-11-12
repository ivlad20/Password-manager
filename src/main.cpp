#include "cli/cli.h"

#include <sodium.h>
#include <exception>
#include <iostream>
#include <string>

// Usage: PasswordManager [vault file]   (default: vault.pwm in the current directory)
int main(int argc, char* argv[]) {
    if (sodium_init() < 0) {
        std::cerr << "failed to initialize libsodium" << std::endl;
        return 1;
    }

    const std::string path = argc > 1 ? argv[1] : "vault.pwm";

    try {
        Cli cli(path);
        return cli.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << std::endl;
        return 1;
    }
}
