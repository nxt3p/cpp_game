#include "game/AppFlow.hpp"

namespace game {

const char* appScreenName(AppScreen screen) noexcept {
    switch (screen) {
    case AppScreen::MAIN_MENU:
        return "MainMenu";
    case AppScreen::CHARACTER_SELECT:
        return "CharacterSelect";
    case AppScreen::LOAD_CHARACTER:
        return "LoadCharacter";
    case AppScreen::SETTINGS:
        return "Settings";
    case AppScreen::IN_GAME:
        return "InGame";
    }
    return "Unknown";
}

const char* characterClassName(CharacterClass characterClass) noexcept {
    switch (characterClass) {
    case CharacterClass::WARRIOR:
        return "Warrior";
    case CharacterClass::RANGER:
        return "Ranger";
    case CharacterClass::MAGE:
        return "Mage";
    case CharacterClass::NONE:
        return "None";
    }
    return "Unknown";
}

} // namespace game
