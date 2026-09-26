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
        return "Magic";
    case ItemRarity::Legendary:
        return "Legendary";
    case ItemRarity::Unique:
        return "Unique";
    }
    return "Unknown";
}

RarityColor rarityColor(const ItemRarity rarity) noexcept {
    switch (rarity) {
    case ItemRarity::Rare:
        return {0.45F, 0.66F, 1.0F};
    case ItemRarity::Legendary:
        return {1.0F, 0.62F, 0.16F};
    case ItemRarity::Unique:
        return {0.32F, 0.95F, 0.42F};
    case ItemRarity::Common:
    default:
        return {0.82F, 0.82F, 0.78F};
    }
}

int rarityRank(const ItemRarity rarity) noexcept {
    return static_cast<int>(rarity);
}

} // namespace systems
