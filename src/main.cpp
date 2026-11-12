#include <sodium.h>
#include <iostream>
#include <string>

int main() {
    if (sodium_init() < 0) {
        std::cerr << "failed to initialize libsodium" << std::endl;
        return 1;
    }

    return 0;
}