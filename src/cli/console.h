#pragma once

#include <string>

// Prints the prompt and reads one line. Returns false on end of input (Ctrl+Z / Ctrl+D).
bool read_line(const std::string& prompt, std::string& out);

// Same as read_line, but the typed characters are not echoed to the console.
// Falls back to a normal read when input is not an interactive console (e.g. piped).
bool read_hidden(const std::string& prompt, std::string& out);

// Asks a yes/no question; anything other than "y" or "yes" counts as no.
bool confirm(const std::string& prompt);
