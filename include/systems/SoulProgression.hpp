#pragma once

#include "gameplay/GameTypes.hpp"
#include "ui/UiTypes.hpp"

namespace systems {

enum class SoulStatKind : std::uint8_t {
    Strength,
    Dexterity,
    Vitality
};

struct SoulUpgradeResult {
    bool success{false};
    int soulsSpent{0};
    int remainingSouls{0};
    std::string message;
};

constexpr float kInitialSoulGainMultiplier = 1.0F;
constexpr float kSoulGainMultiplierPerMobKill = 0.02F;
constexpr float kSoulGainMultiplierPerBossKill = 0.08F;

[[nodiscard]] int scaleSoulReward(int baseSouls, float soulGainMultiplier) noexcept;

void registerMobSoulGain(ui::CharacterScreenData& stats) noexcept;

void registerBossSoulGain(ui::CharacterScreenData& stats) noexcept;

void resetSoulGainMultiplier(ui::CharacterScreenData& stats) noexcept;

[[nodiscard]] std::string formatSoulGainMultiplier(float soulGainMultiplier);

[[nodiscard]] bool canSpendSoulsInTown(bool isInTown) noexcept;

[[nodiscard]] int soulUpgradeCost(int upgradesPurchased) noexcept;

/// Kills pay very little experience. The next level costs more than the last, so gear is the real power.
[[nodiscard]] int experienceRequiredForLevel(int level) noexcept;

struct ExperienceGrant {
    int experienceGained{0};
    int levelsGained{0};
};

[[nodiscard]] ExperienceGrant grantCombatExperience(ui::CharacterScreenData& stats, int amount) noexcept;

[[nodiscard]] SoulUpgradeResult tryPurchaseStatUpgrade(
    ui::CharacterScreenData& stats,
    SoulStatKind stat,
    bool isInTown);

[[nodiscard]] int mobMeleeDamage(
    gameplay::EntityKind kind,
    float mobHpMultiplier,
    float mobDamageMultiplier) noexcept;

} // namespace systems
