#include "gameplay/LaneBattle.hpp"

#include <algorithm>
#include <cstdio>

namespace gameplay {

namespace {

constexpr BattleNode kNodes[LaneBattle::kNodeCount] = {
    {0, "Meadow Gate", 1, 4, false, LootCeiling::Magic, 0.06F},
    {1, "Thorn Path", 2, 4, false, LootCeiling::Magic, 0.10F},
    {2, "Old Ruins", 4, 5, false, LootCeiling::Legendary, 0.14F},
    {3, "Barrow Hill", 6, 5, true, LootCeiling::Legendary, 0.18F},
    {4, "Ash Crypt", 8, 5, false, LootCeiling::Legendary, 0.22F},
    {5, "Iron Citadel", 10, 6, false, LootCeiling::Unique, 0.28F},
    {6, "Hollow Spire", 13, 6, true, LootCeiling::Unique, 0.34F},
    {7, "Night Throne", 16, 6, true, LootCeiling::Unique, 0.42F},
};

} // namespace

const BattleNode& LaneBattle::nodeAt(const int index) noexcept {
    const int clamped = std::clamp(index, 0, kNodeCount - 1);
    return kNodes[clamped];
}

bool LaneBattle::nodeUnlocked(const int index, const int progressionDepth) noexcept {
    return index >= 0 && index < kNodeCount && index < std::max(1, progressionDepth);
}

void LaneBattle::start(const int nodeIndex, const std::uint32_t seed) {
    nodeIndex_ = std::clamp(nodeIndex, 0, kNodeCount - 1);
    phase_ = LanePhase::Running;
    groupsSpawned_ = 0;
    groupsCleared_ = 0;
    clearDelay_ = 0.0F;
    spawnQueued_ = false;
    victoryPending_ = false;
    rng_ = seed == 0U ? 1U : seed;
    refreshStatus();
}

void LaneBattle::update(const float deltaSeconds, const int livingMobs, const float nearestGap) {
    if (phase_ == LanePhase::Cleared) {
        return;
    }

    if (livingMobs > 0 && nearestGap <= kEngageGap) {
        phase_ = LanePhase::Fighting;
    }

    if (phase_ == LanePhase::Fighting && livingMobs <= 0) {
        ++groupsCleared_;
        if (groupsCleared_ >= node().groupCount) {
            phase_ = LanePhase::Cleared;
            victoryPending_ = true;
            refreshStatus();
            return;
        }
        phase_ = LanePhase::Running;
        clearDelay_ = 0.7F;
    }

    if (phase_ == LanePhase::Running && livingMobs <= 0) {
        if (clearDelay_ > 0.0F) {
            clearDelay_ = std::max(0.0F, clearDelay_ - deltaSeconds);
        } else if (!spawnQueued_ && groupsSpawned_ < node().groupCount) {
            spawnQueued_ = true;
        }
    }

    refreshStatus();
}

bool LaneBattle::consumeVictory() noexcept {
    const bool pending = victoryPending_;
    victoryPending_ = false;
    return pending;
}

LaneSpawnRequest LaneBattle::consumeSpawn() {
    LaneSpawnRequest request{};
    if (!spawnQueued_) {
        return request;
    }
    spawnQueued_ = false;
    ++groupsSpawned_;

    const BattleNode& current = node();
    const bool finale = current.bossFinale && groupsSpawned_ >= current.groupCount;
    request.enemyLevel = current.enemyLevel;
    request.boss = finale;
    request.count = finale ? std::max(2, rollInt(2, 3)) : rollInt(2, 5);
    request.eliteCount = 0;
    const int rollTargets = finale ? request.count - 1 : request.count;
    for (int index = 0; index < rollTargets; ++index) {
        if (rollUnit() < current.eliteChance) {
            ++request.eliteCount;
        }
    }
    refreshStatus();
    return request;
}

void LaneBattle::refreshStatus() {
    const char* verb = "Running";
    if (phase_ == LanePhase::Fighting) {
        verb = "Fighting";
    } else if (phase_ == LanePhase::Cleared) {
        verb = "Cleared";
    }
    std::snprintf(
        status_,
        sizeof(status_),
        "%s  %d/%d  %s",
        node().name,
        std::min(groupsCleared_ + (phase_ == LanePhase::Fighting ? 1 : 0), node().groupCount),
        node().groupCount,
        verb);
}

int LaneBattle::rollInt(const int minInclusive, const int maxInclusive) {
    if (maxInclusive <= minInclusive) {
        return minInclusive;
    }
    rng_ = rng_ * 1664525U + 1013904223U;
    const int span = maxInclusive - minInclusive + 1;
    return minInclusive + static_cast<int>(rng_ % static_cast<std::uint32_t>(span));
}

float LaneBattle::rollUnit() {
    rng_ = rng_ * 1664525U + 1013904223U;
    return static_cast<float>(rng_ >> 8) / static_cast<float>(1U << 24);
}

} // namespace gameplay
