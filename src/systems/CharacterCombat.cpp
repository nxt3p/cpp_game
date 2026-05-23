#include "systems/CharacterCombat.hpp"

#include <algorithm>

namespace systems {

int computeDamage(const CombatStatInput& stats) noexcept {
    const int levelBonus = std::max(0, stats.level - 1) * 2;
    return stats.strength + levelBonus;
}

float computeAttacksPerSecond(const CombatStatInput& stats) noexcept {
    const float dexterityBonus = static_cast<float>(stats.dexterity) * 0.05F;
    return 1.0F + dexterityBonus;
}

} // namespace systems
