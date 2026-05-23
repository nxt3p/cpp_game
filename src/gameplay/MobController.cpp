#include "gameplay/MobController.hpp"

#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {

constexpr float kTwoPi = 6.2831853F;
constexpr float kMinDirectionChangeSeconds = 1.5F;
constexpr float kMaxDirectionChangeSeconds = 3.5F;
constexpr float kPlayerAvoidRadius = 5.0F;

std::uint32_t nextRandom(std::uint32_t& state) noexcept {
    state = state * 1664525U + 1013904223U;
    return state;
}

float randomUnit(std::uint32_t& state) noexcept {
    return static_cast<float>(nextRandom(state)) / static_cast<float>(UINT32_MAX);
}

bool isAttackableMob(const EntityKind kind) noexcept {
    return kind == EntityKind::ENEMY_MOB || kind == EntityKind::ENEMY_BOSS;
}

} // namespace

MobController::MobController(const std::uint32_t seed) : rngState_(seed != 0U ? seed : 1U) {}

void MobController::reset() {
    wanderById_.clear();
    spawnCooldownSeconds_ = 0.0F;
}

void MobController::registerMob(const std::uint32_t entityId, const EntityKind kind) {
    if (!isAttackableMob(kind)) {
        return;
    }

    WanderState state{};
    refreshWanderDirection(state);
    wanderById_[entityId] = state;
}

void MobController::unregisterMob(const std::uint32_t entityId) {
    wanderById_.erase(entityId);
}

int MobController::activeMobCount(const std::vector<WorldEntitySnapshot>& scenery) const noexcept {
    int count = 0;
    for (const WorldEntitySnapshot& entity : scenery) {
        if (entity.active && entity.kind == EntityKind::ENEMY_MOB) {
            ++count;
        }
    }
    return count;
}

bool MobController::isMobileMob(const EntityKind kind) const noexcept {
    return kind == EntityKind::ENEMY_MOB || kind == EntityKind::ENEMY_BOSS;
}

float MobController::wanderSpeedFor(const EntityKind kind, const MobSpawnSettings& settings) const noexcept {
    return kind == EntityKind::ENEMY_BOSS ? settings.bossWanderSpeed : settings.mobWanderSpeed;
}

void MobController::refreshWanderDirection(WanderState& state) {
    state.headingRadians = randomUnit(rngState_) * kTwoPi;
    state.directionChangeTimer =
        kMinDirectionChangeSeconds +
        randomRange(0.0F, kMaxDirectionChangeSeconds - kMinDirectionChangeSeconds);
}

float MobController::randomRange(const float minValue, const float maxValue) {
    if (maxValue <= minValue) {
        return minValue;
    }
    return minValue + randomUnit(rngState_) * (maxValue - minValue);
}

bool MobController::trySpawnMob(
    std::vector<WorldEntitySnapshot>& scenery,
    const AxisAlignedBounds& bounds,
    const Vec3& playerPosition,
    std::uint32_t& nextEntityId,
    const MobSpawnSettings& settings) {
    if (activeMobCount(scenery) >= settings.maxMobs) {
        return false;
    }

    constexpr int kMaxAttempts = 12;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const float spawnX = randomRange(
            bounds.minX + settings.boundsMargin, bounds.maxX - settings.boundsMargin);
        const float spawnZ = randomRange(
            bounds.minZ + settings.boundsMargin, bounds.maxZ - settings.boundsMargin);

        const float dx = spawnX - playerPosition.x;
        const float dz = spawnZ - playerPosition.z;
        const float distanceSquared = dx * dx + dz * dz;
        if (distanceSquared < settings.minSpawnDistanceFromPlayer * settings.minSpawnDistanceFromPlayer) {
            continue;
        }

        WorldEntitySnapshot mob{};
        mob.id = nextEntityId++;
        mob.kind = EntityKind::ENEMY_MOB;
        mob.position = Vec3{spawnX, 0.0F, spawnZ};
        mob.active = true;
        scenery.push_back(mob);
        registerMob(mob.id, mob.kind);
        return true;
    }

    return false;
}

void MobController::update(
    const float deltaSeconds,
    std::vector<WorldEntitySnapshot>& scenery,
    const AxisAlignedBounds& bounds,
    const Vec3& playerPosition,
    std::uint32_t& nextEntityId,
    const MobSpawnSettings& settings) {
    if (deltaSeconds <= 0.0F) {
        return;
    }

    for (WorldEntitySnapshot& entity : scenery) {
        if (!entity.active || !isMobileMob(entity.kind)) {
            continue;
        }

        auto iterator = wanderById_.find(entity.id);
        if (iterator == wanderById_.end()) {
            registerMob(entity.id, entity.kind);
            iterator = wanderById_.find(entity.id);
            if (iterator == wanderById_.end()) {
                continue;
            }
        }

        WanderState& state = iterator->second;
        state.speed = wanderSpeedFor(entity.kind, settings);

        const float toPlayerX = playerPosition.x - entity.position.x;
        const float toPlayerZ = playerPosition.z - entity.position.z;
        const float playerDistanceSq = toPlayerX * toPlayerX + toPlayerZ * toPlayerZ;
        const float aggroRadiusSq = settings.aggroRadius * settings.aggroRadius;
        const float deaggroRadiusSq = settings.deaggroRadius * settings.deaggroRadius;

        if (playerDistanceSq <= aggroRadiusSq) {
            state.isAggro = true;
        } else if (playerDistanceSq >= deaggroRadiusSq) {
            state.isAggro = false;
        }

        const float playerDistance =
            (state.isAggro || playerDistanceSq <= kPlayerAvoidRadius * kPlayerAvoidRadius)
                ? std::sqrt(playerDistanceSq)
                : 0.0F;
        if (state.isAggro && playerDistance > 0.35F) {
            state.headingRadians = std::atan2(toPlayerZ, toPlayerX);
            const float chaseSpeed = state.speed * settings.chaseSpeedMultiplier;
            entity.position.x += std::cos(state.headingRadians) * chaseSpeed * deltaSeconds;
            entity.position.z += std::sin(state.headingRadians) * chaseSpeed * deltaSeconds;
        } else {
            state.directionChangeTimer -= deltaSeconds;
            if (state.directionChangeTimer <= 0.0F) {
                refreshWanderDirection(state);
            }

            const float stepX = std::cos(state.headingRadians) * state.speed * deltaSeconds;
            const float stepZ = std::sin(state.headingRadians) * state.speed * deltaSeconds;
            entity.position.x += stepX;
            entity.position.z += stepZ;

            if (playerDistance < kPlayerAvoidRadius && playerDistance > 0.001F) {
                const float awayX = -toPlayerX / playerDistance;
                const float awayZ = -toPlayerZ / playerDistance;
                const float pushStrength = (kPlayerAvoidRadius - playerDistance) * 0.35F;
                entity.position.x += awayX * pushStrength;
                entity.position.z += awayZ * pushStrength;
            }
        }

        const float minX = bounds.minX + settings.boundsMargin;
        const float maxX = bounds.maxX - settings.boundsMargin;
        const float minZ = bounds.minZ + settings.boundsMargin;
        const float maxZ = bounds.maxZ - settings.boundsMargin;

        if (entity.position.x < minX) {
            entity.position.x = minX;
            state.headingRadians = kTwoPi - state.headingRadians;
        } else if (entity.position.x > maxX) {
            entity.position.x = maxX;
            state.headingRadians = kTwoPi - state.headingRadians;
        }

        if (entity.position.z < minZ) {
            entity.position.z = minZ;
            state.headingRadians = -state.headingRadians;
        } else if (entity.position.z > maxZ) {
            entity.position.z = maxZ;
            state.headingRadians = -state.headingRadians;
        }
    }

    spawnCooldownSeconds_ -= deltaSeconds;
    if (spawnCooldownSeconds_ > 0.0F) {
        return;
    }

    if (trySpawnMob(scenery, bounds, playerPosition, nextEntityId, settings)) {
        spawnCooldownSeconds_ = randomRange(
            settings.minSpawnIntervalSeconds, settings.maxSpawnIntervalSeconds);
    } else {
        spawnCooldownSeconds_ = 2.0F;
    }
}

} // namespace gameplay
