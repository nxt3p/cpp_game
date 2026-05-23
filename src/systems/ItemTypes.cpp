#include "systems/ItemTypes.hpp"

namespace systems {

int actionWeight(ActionType type) noexcept {
    switch (type) {
    case ActionType::ROCK_CLICK:
        return 1;
    case ActionType::CHEST_OPEN:
        return 5;
    case ActionType::MOB_KILL:
        return 10;
    case ActionType::BOSS_KILL:
        return 100;
    }
    return 0;
}

const char* rarityLabel(ItemRarity rarity) noexcept {
    switch (rarity) {
    case ItemRarity::Common:
        return "Common";
    case ItemRarity::Rare:
        return "Rare";
    case ItemRarity::Legendary:
        return "Legendary";
    }
    return "Unknown";
}

} // namespace systems
