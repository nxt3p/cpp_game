#include "game/GameDebug.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace game {

namespace {

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%H:%M:%S");
    return stream.str();
}

void writeLine(const char* level, const std::string& message) {
    std::cout << "[" << timestamp() << "][" << level << "][GameEngine] " << message << std::endl;
}

} // namespace

void logInfo(const std::string& message) {
    writeLine("INFO", message);
}

void logHelp(const std::string& message) {
    writeLine("HELP", message);
}

void logBanner() {
    std::cout << "============================================================" << std::endl;
    logInfo("Build loading...");
    logHelp("Front-end: click buttons with the mouse.");
    logHelp("Main Menu -> Play -> pick a class box -> enter Town.");
    logHelp("In-game: click ground to move | C stats | I inventory | E trade | Esc pause");
    logHelp("Pause menu Save and Exit stores progress and returns to the main menu.");
    logHelp("Web saves persist in browser localStorage; Windows saves use %LOCALAPPDATA%\\cppGame.");
    std::cout << "============================================================" << std::endl;
}

} // namespace game
