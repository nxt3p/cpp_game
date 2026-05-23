#include "systems/RunProgression.hpp"

#include <algorithm>

namespace systems {

RunProgression::RunProgression(const std::uint32_t seed) : runSeed_(seed) {}

DifficultyModifiers RunProgression::modifiers() const noexcept {
    DifficultyModifiers mods{};
    const float depthFactor = static_cast<float>(std::max(1, depth_) - 1);
    mods.mobHpMultiplier = 1.35F + depthFactor * 0.55F;
    mods.mobXpMultiplier = 1.0F + depthFactor * 0.35F;
    mods.mobDamageMultiplier = 1.15F + depthFactor * 0.4F;
    mods.lootTierBonus = depthFactor * 0.04F;
    mods.itemLevel = depth_;
    mods.mobSpawnBudget = std::min(12, 4 + depth_);
    return mods;
}

void RunProgression::onMobKill() noexcept {
    ++mobsKilledThisDepth_;
    ++lifetimeMobKills_;
}

void RunProgression::onBossDefeated() noexcept {
    ++totalBossKills_;
    ++depth_;
    mobsKilledThisDepth_ = 0;
    runSeed_ = runSeed_ * 1664525U + 1013904223U;
}

void RunProgression::setSeed(const std::uint32_t seed) noexcept {
    runSeed_ = seed;
}

void RunProgression::applyState(
    const int depth,
    const std::uint32_t seed,
    const int totalBossKills,
    const int mobsKilledThisDepth,
    const int lifetimeMobKills) noexcept {
    depth_ = std::max(1, depth);
    runSeed_ = seed;
    totalBossKills_ = std::max(0, totalBossKills);
    mobsKilledThisDepth_ = std::max(0, mobsKilledThisDepth);
    lifetimeMobKills_ = std::max(0, lifetimeMobKills);
}

} // namespace systems
