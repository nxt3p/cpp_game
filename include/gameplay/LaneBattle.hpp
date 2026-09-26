#pragma once

#include <cstdint>

namespace gameplay {

enum class LanePhase : std::uint8_t {
    Running,
    Fighting,
    Cleared,
};

enum class LootCeiling : std::uint8_t {
    Magic,
    Legendary,
    Unique,
};

struct BattleNode {
    int index{0};
    const char* name{"Road"};
    int enemyLevel{1};
    int groupCount{4};
    bool bossFinale{false};
    LootCeiling ceiling{LootCeiling::Magic};
    float eliteChance{0.08F};
};

struct LaneSpawnRequest {
    int count{0};
    int eliteCount{0};
    bool boss{false};
    int enemyLevel{1};
};

/// Left-to-right auto-battle. The hero runs, a pack marches in from the right, they fight, then the run resumes.
class LaneBattle {
public:
    static constexpr int kNodeCount = 8;
    static constexpr float kEngageGap = 4.6F;
    static constexpr float kSpawnLead = 12.0F;
    static constexpr float kMobMarchSpeed = 3.6F;

    [[nodiscard]] static const BattleNode& nodeAt(int index) noexcept;
    [[nodiscard]] static bool nodeUnlocked(int index, int progressionDepth) noexcept;

    void start(int nodeIndex, std::uint32_t seed);
    void update(float deltaSeconds, int livingMobs, float nearestGap);

    [[nodiscard]] LanePhase phase() const noexcept { return phase_; }
    [[nodiscard]] int nodeIndex() const noexcept { return nodeIndex_; }
    [[nodiscard]] const BattleNode& node() const noexcept { return nodeAt(nodeIndex_); }
    [[nodiscard]] int groupsCleared() const noexcept { return groupsCleared_; }
    [[nodiscard]] int groupsSpawned() const noexcept { return groupsSpawned_; }
    [[nodiscard]] bool wantsSpawn() const noexcept { return spawnQueued_; }
    [[nodiscard]] bool consumeVictory() noexcept;
    [[nodiscard]] const char* status() const noexcept { return status_; }

    [[nodiscard]] LaneSpawnRequest consumeSpawn();

private:
    void refreshStatus();
    [[nodiscard]] int rollInt(int minInclusive, int maxInclusive);
    [[nodiscard]] float rollUnit();

    LanePhase phase_{LanePhase::Running};
    int nodeIndex_{0};
    int groupsSpawned_{0};
    int groupsCleared_{0};
    float clearDelay_{0.0F};
    bool spawnQueued_{false};
    bool victoryPending_{false};
    std::uint32_t rng_{1U};
    char status_[96]{"Road"};
};

} // namespace gameplay
