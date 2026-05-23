#pragma once

#include <cstdint>

namespace game {

enum class AppScreen : std::uint8_t {
    MAIN_MENU,
    CHARACTER_SELECT,
    LOAD_CHARACTER,
    SETTINGS,
    IN_GAME
};

enum class SettingsOrigin : std::uint8_t {
    MAIN_MENU,
    PAUSE_MENU
};

enum class CharacterClass : std::uint8_t {
    WARRIOR,
    RANGER,
    MAGE,
    NONE
};

[[nodiscard]] const char* appScreenName(AppScreen screen) noexcept;
[[nodiscard]] const char* characterClassName(CharacterClass characterClass) noexcept;

} // namespace game
