#pragma once

#include <cstdint>

namespace systems {

/// Diablo-style global difficulty. Multiplies depth scaling and unlocks better loot tiers.
enum class DifficultyTier : std::uint8_t {
    Normal = 0,
    Nightmare = 1,
    Hell = 2,
};

constexpr int kDifficultyTierCount = 3;

[[nodiscard]] const char* difficultyTierLabel(DifficultyTier tier) noexcept;
[[nodiscard]] DifficultyTier difficultyTierFromIndex(int index) noexcept;

struct DifficultyTierModifiers {
    float hpMultiplier{1.0F};
    float damageMultiplier{1.0F};
    float xpMultiplier{1.0F};
    float lootTierBonus{0.0F};
    int itemLevelBonus{0};
    int spawnBudgetBonus{0};
};

[[nodiscard]] DifficultyTierModifiers difficultyTierModifiers(DifficultyTier tier) noexcept;

struct DifficultyModifiers {
    float mobHpMultiplier{1.0F};
    float mobXpMultiplier{1.0F};
    float mobDamageMultiplier{1.0F};
    float lootTierBonus{0.0F};
    int itemLevel{1};
    int mobSpawnBudget{4};
};

class RunProgression {
public:
    explicit RunProgression(std::uint32_t seed = 0xCAFE0001U);

    [[nodiscard]] int depth() const noexcept { return depth_; }
    [[nodiscard]] std::uint32_t runSeed() const noexcept { return runSeed_; }
    [[nodiscard]] int totalBossKills() const noexcept { return totalBossKills_; }
    [[nodiscard]] int mobsKilledThisDepth() const noexcept { return mobsKilledThisDepth_; }
    [[nodiscard]] int lifetimeMobKills() const noexcept { return lifetimeMobKills_; }
    [[nodiscard]] DifficultyTier tier() const noexcept { return tier_; }

    [[nodiscard]] DifficultyModifiers modifiers() const noexcept;

    void onMobKill() noexcept;
    void onBossDefeated() noexcept;
    /// First clear of a node unlocks the next road. Farming the same node does not raise depth again.
    void noteNodeCleared(int nodeIndex) noexcept;
    void setSeed(std::uint32_t seed) noexcept;
    void setTier(DifficultyTier tier) noexcept;

    /// Nightmare unlocks after the first boss kill; Hell after three.
    [[nodiscard]] bool isTierUnlocked(DifficultyTier tier) const noexcept;

    void applyState(
        int depth,
        std::uint32_t seed,
        int totalBossKills,
        int mobsKilledThisDepth,
        int lifetimeMobKills) noexcept;

private:
    int depth_{1};
    std::uint32_t runSeed_{0xCAFE0001U};
    int totalBossKills_{0};
    int mobsKilledThisDepth_{0};
    int lifetimeMobKills_{0};
    DifficultyTier tier_{DifficultyTier::Normal};
};

} // namespace systems
