#include "systems/CharacterCombat.hpp"

#include <algorithm>
#include <cmath>

namespace systems {

int computeDamage(const CombatStatInput& stats) noexcept {
    const int levelBonus = std::max(0, stats.level - 1) * 2;
    return stats.strength + levelBonus;
}

float computeAttacksPerSecond(const CombatStatInput& stats) noexcept {
    const float dexterityBonus = static_cast<float>(stats.dexterity) * 0.05F;
    return 1.0F + dexterityBonus;
}

float computeCriticalChance(const int dexterity) noexcept {
    const float chance = 0.05F + static_cast<float>(std::max(0, dexterity)) * 0.004F;
    return std::clamp(chance, 0.05F, 0.5F);
}

bool isCriticalRoll(const float roll, const float critChance) noexcept {
    return roll < critChance;
}

int applyCriticalDamage(const int baseDamage) noexcept {
    return static_cast<int>(std::ceil(static_cast<float>(std::max(0, baseDamage)) * kCriticalHitMultiplier));
}

} // namespace systems
