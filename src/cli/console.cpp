#include "console.h"

#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

static void strip_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

bool read_line(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) {
        return false;
    }
    strip_carriage_return(out);
    return true;
}

bool read_hidden(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;

#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_mode = 0;
    const bool is_console = GetConsoleMode(input, &old_mode) != 0;
    if (is_console) {
        SetConsoleMode(input, old_mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
    }
#else
    termios old_mode{};
    const bool is_console = tcgetattr(STDIN_FILENO, &old_mode) == 0;
    if (is_console) {
        termios hidden = old_mode;
        hidden.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &hidden);
    }
#endif

    const bool ok = static_cast<bool>(std::getline(std::cin, out));

#ifdef _WIN32
    if (is_console) {
        SetConsoleMode(input, old_mode);
    }
#else
    if (is_console) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_mode);
    }
#endif

    // The Enter key was not echoed either, so move to a new line ourselves.
    if (is_console) {
        std::cout << std::endl;
    }

    if (ok) {
        strip_carriage_return(out);
    }
    return ok;
}

bool confirm(const std::string& prompt) {
    std::string answer;
    if (!read_line(prompt + " [y/N]: ", answer)) {
        return false;
    }
    return answer == "y" || answer == "Y" || answer == "yes";
}
