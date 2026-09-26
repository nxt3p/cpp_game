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
    case ItemRarity::Magic:
        return "Magic";
    case ItemRarity::Rare:
        return "Rare";
    case ItemRarity::Legendary:
        return "Legendary";
    case ItemRarity::Unique:
        return "Unique";
    case ItemRarity::Mythical:
        return "Mythical";
    }
    return "Unknown";
}

RarityColor rarityColor(const ItemRarity rarity) noexcept {
    switch (rarity) {
    case ItemRarity::Magic:
        return {0.45F, 0.66F, 1.0F};
    case ItemRarity::Rare:
        return {0.95F, 0.82F, 0.28F};
    case ItemRarity::Legendary:
        return {0.55F, 0.32F, 0.14F};
    case ItemRarity::Unique:
        return {0.32F, 0.95F, 0.42F};
    case ItemRarity::Mythical:
        return {0.92F, 0.18F, 0.78F};
    case ItemRarity::Common:
        return {0.82F, 0.82F, 0.78F};
    }
    return {0.82F, 0.82F, 0.78F};
}

int rarityRank(const ItemRarity rarity) noexcept {
    switch (rarity) {
    case ItemRarity::Common:
        return 0;
    case ItemRarity::Magic:
        return 1;
    case ItemRarity::Rare:
        return 2;
    case ItemRarity::Legendary:
        return 3;
    case ItemRarity::Unique:
        return 4;
    case ItemRarity::Mythical:
        return 5;
    }
    return 0;
}

} // namespace systems
