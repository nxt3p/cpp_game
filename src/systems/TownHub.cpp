#include "systems/TownHub.hpp"

#include <algorithm>

namespace systems {

namespace {

constexpr TownBuildingDefinition kBuildings[] = {
    {"Blacksmith", "Ruined Forge", "Sell gear and temper weapons", 36, 1},
    {"Tavern", "Ruined Tavern", "Gamble gold for a mystery prize", 120, 3},
    {"Healer", "Ruined Chapel", "Rest and restore health", 64, 2},
};

constexpr int kHealerTitheGold = 12;

} // namespace

TownBuildingDefinition townBuildingDefinition(const TownBuilding building) noexcept {
    const int index = std::clamp(static_cast<int>(building), 0, static_cast<int>(TownBuilding::Count) - 1);
    return kBuildings[index];
}

CombatKillReward combatKillReward(const bool boss, const bool elite, const int depth) noexcept {
    const int safeDepth = std::max(1, depth);
    CombatKillReward reward{};
    if (boss) {
        reward.gold = 28 + safeDepth * 8;
        reward.experience = 36;
    } else if (elite) {
        reward.gold = 14 + safeDepth * 3;
        reward.experience = 14;
    } else {
        reward.gold = 6 + safeDepth;
        reward.experience = 8;
    }
    return reward;
}

int combatGoldBounty(const bool boss, const bool elite, const int depth) noexcept {
    return combatKillReward(boss, elite, depth).gold;
}

int healerTitheGold() noexcept {
    return kHealerTitheGold;
}

bool TownHub::isRepaired(const TownBuilding building) const noexcept {
    if (building >= TownBuilding::Count) {
        return false;
    }
    return (repairMask_ & (1 << static_cast<int>(building))) != 0;
}

void TownHub::applyRepairMask(const int mask) noexcept {
    constexpr int kAllBuildings = (1 << static_cast<int>(TownBuilding::Count)) - 1;
    repairMask_ = mask & kAllBuildings;
}

TownRepairResult TownHub::tryRepair(const TownBuilding building, int& gold, const int level) noexcept {
    if (building >= TownBuilding::Count) {
        return TownRepairResult::NeedLevel;
    }
    if (isRepaired(building)) {
        return TownRepairResult::AlreadyOpen;
    }

    const TownBuildingDefinition definition = townBuildingDefinition(building);
    if (level < definition.requiredLevel) {
        return TownRepairResult::NeedLevel;
    }
    if (gold < definition.repairGold) {
        return TownRepairResult::NeedGold;
    }

    gold -= definition.repairGold;
    repairMask_ |= 1 << static_cast<int>(building);
    return TownRepairResult::Repaired;
}

std::string TownHub::repairMessage(const TownBuilding building, const TownRepairResult result) {
    const TownBuildingDefinition definition = townBuildingDefinition(building);
    switch (result) {
    case TownRepairResult::Repaired:
        return std::string(definition.name) + " restored. " + definition.serviceHint + ".";
    case TownRepairResult::AlreadyOpen:
        return std::string(definition.name) + " is already open.";
    case TownRepairResult::NeedLevel:
        return std::string(definition.ruinedName) + " needs level " + std::to_string(definition.requiredLevel) +
               ".";
    case TownRepairResult::NeedGold:
        return std::string(definition.ruinedName) + " costs " + std::to_string(definition.repairGold) + " gold.";
    }
    return "The ruins are silent.";
}

} // namespace systems
