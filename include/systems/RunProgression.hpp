#pragma once

#include <cstdint>

namespace systems {

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

    [[nodiscard]] DifficultyModifiers modifiers() const noexcept;

    void onMobKill() noexcept;
    void onBossDefeated() noexcept;
    void setSeed(std::uint32_t seed) noexcept;

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
};

} // namespace systems
