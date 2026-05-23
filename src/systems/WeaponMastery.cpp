#include "systems/WeaponMastery.hpp"

#include <algorithm>
#include <sstream>

namespace systems {

bool isWeaponMasteryEligible(const ItemCategory category) noexcept {
    return category == ItemCategory::Weapon || category == ItemCategory::OffHand;
}

int weaponMasteryXpToNextLevel(const int masteryLevel) noexcept {
    const int safeLevel = std::max(1, masteryLevel);
    return 35 + (safeLevel - 1) * 20;
}

ItemStatBonuses weaponMasteryBonuses(const ItemMetadata& item) noexcept {
    ItemStatBonuses bonuses{};
    if (!isWeaponMasteryEligible(item.category)) {
        return bonuses;
    }

    const int levelsAboveBase = std::max(0, item.masteryLevel - 1);
    bonuses.damage = levelsAboveBase;
    if (levelsAboveBase >= 5) {
        bonuses.attackSpeed = static_cast<float>(levelsAboveBase / 5) * 0.03F;
    }
    return bonuses;
}

int weaponMasteryXpForHit(const int damageDealt) noexcept {
    return std::max(1, damageDealt / 2);
}

int weaponMasteryXpForKill(const int mobXpReward) noexcept {
    return std::max(5, mobXpReward / 2);
}

WeaponMasteryResult grantWeaponMasteryXp(ItemMetadata& item, const int amount) {
    WeaponMasteryResult result{};
    result.previousLevel = std::max(1, item.masteryLevel);

    if (!isWeaponMasteryEligible(item.category) || amount <= 0) {
        result.newLevel = result.previousLevel;
        return result;
    }

    if (item.masteryLevel < 1) {
        item.masteryLevel = 1;
    }

    item.masteryXp += amount;
    result.xpGained = amount;
    result.newLevel = item.masteryLevel;

    while (item.masteryXp >= weaponMasteryXpToNextLevel(item.masteryLevel)) {
        item.masteryXp -= weaponMasteryXpToNextLevel(item.masteryLevel);
        ++item.masteryLevel;
        result.leveledUp = true;
        result.newLevel = item.masteryLevel;
    }

    return result;
}

std::string formatWeaponMasteryLine(const ItemMetadata& item) {
    if (!isWeaponMasteryEligible(item.category)) {
        return {};
    }

    std::ostringstream line;
    line << "Weapon Mastery Lv " << std::max(1, item.masteryLevel) << "  ("
         << item.masteryXp << "/" << weaponMasteryXpToNextLevel(item.masteryLevel) << " XP)";
    return line.str();
}

} // namespace systems
