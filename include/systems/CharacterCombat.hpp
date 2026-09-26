#pragma once

#include <cstdint>

namespace systems {

struct CombatStatInput {
    int level{1};
    int strength{10};
    int dexterity{10};
};

constexpr float kCriticalHitMultiplier = 1.75F;

[[nodiscard]] int computeDamage(const CombatStatInput& stats) noexcept;
[[nodiscard]] float computeAttacksPerSecond(const CombatStatInput& stats) noexcept;

/// Crit chance in [0.05, 0.5]: 5% base + 0.4% per dexterity point.
[[nodiscard]] float computeCriticalChance(int dexterity) noexcept;

/// Deterministic crit decision from a uniform roll in [0,1).
[[nodiscard]] bool isCriticalRoll(float roll, float critChance) noexcept;

/// Applies the crit multiplier (rounded up so crits always feel bigger).
[[nodiscard]] int applyCriticalDamage(int baseDamage) noexcept;

} // namespace systems
