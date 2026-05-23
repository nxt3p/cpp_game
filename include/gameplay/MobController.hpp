#pragma once

#include "gameplay/GameTypes.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace gameplay {

struct MobSpawnSettings {
    int maxMobs{8};
    float minSpawnIntervalSeconds{6.0F};
    float maxSpawnIntervalSeconds{14.0F};
    float minSpawnDistanceFromPlayer{18.0F};
    float mobWanderSpeed{3.0F};
    float bossWanderSpeed{1.2F};
    float boundsMargin{4.0F};
    float aggroRadius{16.0F};
    float deaggroRadius{24.0F};
    float chaseSpeedMultiplier{1.65F};
};

class MobController {
public:
    explicit MobController(std::uint32_t seed = 1U);

    void reset();
    void registerMob(std::uint32_t entityId, EntityKind kind);
    void unregisterMob(std::uint32_t entityId);

    void update(
        float deltaSeconds,
        std::vector<WorldEntitySnapshot>& scenery,
        const AxisAlignedBounds& bounds,
        const Vec3& playerPosition,
        std::uint32_t& nextEntityId,
        const MobSpawnSettings& settings = MobSpawnSettings{});

    [[nodiscard]] float spawnCooldownSeconds() const noexcept { return spawnCooldownSeconds_; }
    [[nodiscard]] int activeMobCount(const std::vector<WorldEntitySnapshot>& scenery) const noexcept;

private:
    struct WanderState {
        float headingRadians{0.0F};
        float speed{3.0F};
        float directionChangeTimer{0.0F};
        bool isAggro{false};
    };

    [[nodiscard]] bool isMobileMob(EntityKind kind) const noexcept;
    [[nodiscard]] float wanderSpeedFor(EntityKind kind, const MobSpawnSettings& settings) const noexcept;
    void refreshWanderDirection(WanderState& state);
    [[nodiscard]] float randomRange(float minValue, float maxValue);
    [[nodiscard]] bool trySpawnMob(
        std::vector<WorldEntitySnapshot>& scenery,
        const AxisAlignedBounds& bounds,
        const Vec3& playerPosition,
        std::uint32_t& nextEntityId,
        const MobSpawnSettings& settings);

    std::unordered_map<std::uint32_t, WanderState> wanderById_{};
    float spawnCooldownSeconds_{0.0F};
    std::uint32_t rngState_{1U};
};

} // namespace gameplay
