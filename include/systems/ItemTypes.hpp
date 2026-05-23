#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace systems {

enum class ItemRarity : std::uint8_t {
    Common,
    Rare,
    Legendary
};

enum class ActionType : std::uint8_t {
    ROCK_CLICK,
    CHEST_OPEN,
    MOB_KILL,
    BOSS_KILL
};

enum class EntityTier : std::uint8_t {
    Minor,
    Standard,
    Elite,
    Boss
};

enum class ItemCategory : std::uint8_t {
    Head,
    Shoulders,
    Chest,
    Hands,
    Waist,
    Legs,
    Feet,
    Weapon,
    OffHand,
    Amulet,
    Ring,
    Cloak,
    Charm,
    Relic,
    Consumable,
    Misc
};

struct ItemStatBonuses {
    int strength{0};
    int dexterity{0};
    int vitality{0};
    int maxHealth{0};
    float attackSpeed{0.0F};
    int damage{0};
    float lightRadius{0.0F};
};

struct ItemMetadata {
    std::uint32_t itemId{0};
    std::string name;
    ItemRarity rarity{ItemRarity::Common};
    int value{0};
    ItemCategory category{ItemCategory::Misc};
    char iconLetter{'?'};
    ItemStatBonuses bonuses{};
    int itemLevel{1};
    int masteryLevel{1};
    int masteryXp{0};
    int upgradeLevel{0};
};

[[nodiscard]] int actionWeight(ActionType type) noexcept;
[[nodiscard]] const char* rarityLabel(ItemRarity rarity) noexcept;

} // namespace systems
