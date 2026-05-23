#pragma once

namespace systems {

struct CombatStatInput {
    int level{1};
    int strength{10};
    int dexterity{10};
};

[[nodiscard]] int computeDamage(const CombatStatInput& stats) noexcept;
[[nodiscard]] float computeAttacksPerSecond(const CombatStatInput& stats) noexcept;

} // namespace systems
