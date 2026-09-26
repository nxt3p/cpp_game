#pragma once

#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ui {
struct CharacterScreenData;
}

namespace systems {

struct EffectiveCharacterStats {
    int strength{0};
    int dexterity{0};
    int vitality{0};
    int maxHealth{0};
    int damage{0};
    float attacksPerSecond{0.0F};
    float lightRadius{10.0F};
};

/// Base torch reach before gear bonuses (world units on XZ).
constexpr float kBaseLightRadius = 10.0F;

[[nodiscard]] const char* itemCategoryLabel(ItemCategory category) noexcept;

void applyItemDefinition(ItemMetadata& item);

[[nodiscard]] ItemStatBonuses sumInventoryBonuses(const Inventory& inventory);
[[nodiscard]] EffectiveCharacterStats computeEffectiveStats(
    const ui::CharacterScreenData& baseStats,
    const Equipment& equipment);
[[nodiscard]] std::vector<std::string> formatItemStatLines(const ItemMetadata& item);

enum class TooltipTone : std::uint8_t {
    Title,
    Meta,
    Headline,
    Attribute,
    Effect,
    Footer,
    Compare,
};

struct TooltipLine {
    std::string text;
    TooltipTone tone{TooltipTone::Meta};
};

struct ItemTooltipCard {
    ItemRarity rarity{ItemRarity::Common};
    std::vector<TooltipLine> lines;
};

/// Structured compare card. `banner` is an optional lead line such as "Equipped".
[[nodiscard]] ItemTooltipCard buildItemTooltipCard(const ItemMetadata& item, const char* banner = nullptr);

/// Weapon/off-hand presentation DPS from rolled damage, speed, item level, and rarity. 0 otherwise.
[[nodiscard]] int estimateWeaponDps(const ItemMetadata& item);

[[nodiscard]] std::string formatItemTooltip(const ItemMetadata& item);

/// Signed stat deltas of `candidate` versus `equipped` (candidate - equipped), mastery included.
[[nodiscard]] ItemStatBonuses compareItemBonuses(const ItemMetadata& candidate, const ItemMetadata& equipped);

/// Human readable "vs equipped" lines, e.g. "+3 Damage  (vs Sturdy Blade)"; empty when identical.
[[nodiscard]] std::vector<std::string> formatItemComparisonLines(
    const ItemMetadata& candidate,
    const ItemMetadata& equipped);

} // namespace systems
