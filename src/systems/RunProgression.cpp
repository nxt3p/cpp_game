#include "systems/RunProgression.hpp"

#include <algorithm>

namespace systems {

const char* difficultyTierLabel(const DifficultyTier tier) noexcept {
    switch (tier) {
    case DifficultyTier::Normal:
        return "Normal";
    case DifficultyTier::Nightmare:
        return "Nightmare";
    case DifficultyTier::Hell:
        return "Hell";
    }
    return "Normal";
}

DifficultyTier difficultyTierFromIndex(const int index) noexcept {
    return static_cast<DifficultyTier>(std::clamp(index, 0, kDifficultyTierCount - 1));
}

DifficultyTierModifiers difficultyTierModifiers(const DifficultyTier tier) noexcept {
    DifficultyTierModifiers mods{};
    switch (tier) {
    case DifficultyTier::Normal:
        break;
    case DifficultyTier::Nightmare:
        mods.hpMultiplier = 2.2F;
        mods.damageMultiplier = 1.85F;
        mods.xpMultiplier = 1.75F;
        mods.lootTierBonus = 0.05F;
        mods.itemLevelBonus = 3;
        mods.spawnBudgetBonus = 3;
        break;
    case DifficultyTier::Hell:
        mods.hpMultiplier = 3.8F;
        mods.damageMultiplier = 2.8F;
        mods.xpMultiplier = 2.75F;
        mods.lootTierBonus = 0.1F;
        mods.itemLevelBonus = 7;
        mods.spawnBudgetBonus = 6;
        break;
    }
    return mods;
}

RunProgression::RunProgression(const std::uint32_t seed) : runSeed_(seed) {}

DifficultyModifiers RunProgression::modifiers() const noexcept {
    const DifficultyTierModifiers tierMods = difficultyTierModifiers(tier_);
    DifficultyModifiers mods{};
    const float depthFactor = static_cast<float>(std::max(1, depth_) - 1);
    mods.mobHpMultiplier = (1.7F + depthFactor * 0.7F) * tierMods.hpMultiplier;
    mods.mobXpMultiplier = (1.0F + depthFactor * 0.35F) * tierMods.xpMultiplier;
    mods.mobDamageMultiplier = (1.45F + depthFactor * 0.5F) * tierMods.damageMultiplier;
    mods.lootTierBonus = depthFactor * 0.03F + tierMods.lootTierBonus;
    mods.itemLevel = depth_ + tierMods.itemLevelBonus;
    mods.mobSpawnBudget = std::min(24, 8 + depth_ + tierMods.spawnBudgetBonus);
    return mods;
}

void RunProgression::onMobKill() noexcept {
    ++mobsKilledThisDepth_;
    ++lifetimeMobKills_;
}

void RunProgression::noteNodeCleared(const int nodeIndex) noexcept {
    const int unlockedDepth = std::max(0, nodeIndex) + 2;
    if (unlockedDepth > depth_) {
        depth_ = unlockedDepth;
    }
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

void RunProgression::setTier(const DifficultyTier tier) noexcept {
    tier_ = tier;
}

bool RunProgression::isTierUnlocked(const DifficultyTier tier) const noexcept {
    switch (tier) {
    case DifficultyTier::Normal:
        return true;
    case DifficultyTier::Nightmare:
        return totalBossKills_ >= 1;
    case DifficultyTier::Hell:
        return totalBossKills_ >= 3;
    }
    return false;
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
