#pragma once

#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemTypes.hpp"

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
[[nodiscard]] std::string formatItemTooltip(const ItemMetadata& item);

} // namespace systems
