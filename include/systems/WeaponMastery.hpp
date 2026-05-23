#pragma once

#include "systems/ItemTypes.hpp"

namespace systems {

struct WeaponMasteryResult {
    bool leveledUp{false};
    int xpGained{0};
    int previousLevel{1};
    int newLevel{1};
};

[[nodiscard]] bool isWeaponMasteryEligible(ItemCategory category) noexcept;

[[nodiscard]] int weaponMasteryXpToNextLevel(int masteryLevel) noexcept;

[[nodiscard]] ItemStatBonuses weaponMasteryBonuses(const ItemMetadata& item) noexcept;

[[nodiscard]] int weaponMasteryXpForHit(int damageDealt) noexcept;

[[nodiscard]] int weaponMasteryXpForKill(int mobXpReward) noexcept;

[[nodiscard]] WeaponMasteryResult grantWeaponMasteryXp(ItemMetadata& item, int amount);

[[nodiscard]] std::string formatWeaponMasteryLine(const ItemMetadata& item);

} // namespace systems
