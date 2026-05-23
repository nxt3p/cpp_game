#include "systems/SoulProgression.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace systems {

int scaleSoulReward(const int baseSouls, const float soulGainMultiplier) noexcept {
    if (baseSouls <= 0) {
        return 0;
    }

    const float scaled = static_cast<float>(baseSouls) * soulGainMultiplier;
    return std::max(1, static_cast<int>(std::lround(scaled)));
}

void registerMobSoulGain(ui::CharacterScreenData& stats) noexcept {
    stats.soulGainMultiplier += kSoulGainMultiplierPerMobKill;
}

void registerBossSoulGain(ui::CharacterScreenData& stats) noexcept {
    stats.soulGainMultiplier += kSoulGainMultiplierPerBossKill;
}

void resetSoulGainMultiplier(ui::CharacterScreenData& stats) noexcept {
    stats.soulGainMultiplier = kInitialSoulGainMultiplier;
}

std::string formatSoulGainMultiplier(const float soulGainMultiplier) {
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(2) << "x" << soulGainMultiplier;
    return formatted.str();
}

bool canSpendSoulsInTown(const bool isInTown) noexcept {
    return isInTown;
}

int soulUpgradeCost(const int upgradesPurchased) noexcept {
    return 75 + std::max(0, upgradesPurchased) * 25;
}

SoulUpgradeResult tryPurchaseStatUpgrade(
    ui::CharacterScreenData& stats,
    const SoulStatKind stat,
    const bool isInTown) {
    SoulUpgradeResult result{};
    result.remainingSouls = stats.carriedSouls;

    if (!canSpendSoulsInTown(isInTown)) {
        result.message = "Stat upgrades are only available in Town.";
        return result;
    }

    const int cost = soulUpgradeCost(stats.statUpgradesPurchased);
    if (stats.carriedSouls < cost) {
        result.message = "Not enough souls. Need " + std::to_string(cost) + " souls.";
        return result;
    }

    stats.carriedSouls -= cost;
    ++stats.statUpgradesPurchased;
    ++stats.level;

    switch (stat) {
    case SoulStatKind::Strength:
        stats.strength += 1;
        break;
    case SoulStatKind::Dexterity:
        stats.dexterity += 1;
        break;
    case SoulStatKind::Vitality:
        stats.vitality += 1;
        break;
    }

    result.success = true;
    result.soulsSpent = cost;
    result.remainingSouls = stats.carriedSouls;
    result.message = "Stat increased for " + std::to_string(cost) + " souls.";
    return result;
}

int mobMeleeDamage(
    const gameplay::EntityKind kind,
    const float mobHpMultiplier,
    const float mobDamageMultiplier) noexcept {
    int baseDamage = 0;
    switch (kind) {
    case gameplay::EntityKind::ENEMY_MOB:
        baseDamage = 12;
        break;
    case gameplay::EntityKind::ENEMY_BOSS:
        baseDamage = 28;
        break;
    default:
        return 0;
    }

    const float scaled =
        static_cast<float>(baseDamage) * mobHpMultiplier * mobDamageMultiplier;
    return std::max(1, static_cast<int>(scaled + 0.5F));
}

} // namespace systems
