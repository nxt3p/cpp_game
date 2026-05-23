#include "gameplay/ZoneManager.hpp"

#include "gameplay/ProceduralZoneGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {

WorldEntitySnapshot makeEntity(
    std::uint32_t id,
    EntityKind kind,
    float x,
    float y,
    float z,
    std::uint8_t variant = 0) {
    WorldEntitySnapshot entity{id, kind, Vec3{x, y, z}, true};
    entity.variant = variant;
    return entity;
}

void appendProps(
    std::vector<WorldEntitySnapshot>& scenery,
    std::uint32_t& nextId,
    const PropSpawn* props,
    std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        const PropSpawn& prop = props[index];
        scenery.push_back(makeEntity(nextId++, prop.kind, prop.x, 0.0F, prop.z, prop.variant));
    }
}

} // namespace

ZoneManager::ZoneManager() : player_(1U, Vec3{0.0F, 0.0F, 0.0F}), blacksmith_(Vec3{8.0F, 0.0F, -6.0F}) {
    buildTownLayout();
    player_.setAttacksEnabled(false);
}

ZoneTransitionResult ZoneManager::updatePlayerPosition(const Vec3& position) {
    ZoneTransitionResult result{};
    result.fromZone = activeZone_;
    result.toZone = activeZone_;
    result.newPlayerPosition = position;

    player_.setPosition(position);

    if (activeZone_ == WorldZone::TOWN) {
        player_.setAttacksEnabled(false);
        if (exitGateBounds_.contains(position.x, position.z)) {
            shiftPlayerTownToPlains();
            activeZone_ = WorldZone::PLAINS;
            player_.setAttacksEnabled(true);
            respawnPlainsContent();
            result.transitioned = true;
            result.toZone = WorldZone::PLAINS;
            result.newPlayerPosition = player_.position();
        }
    } else {
        player_.setAttacksEnabled(true);
        if (returnGateBounds_.contains(position.x, position.z)) {
            shiftPlayerPlainsToTown();
            activeZone_ = WorldZone::TOWN;
            player_.setAttacksEnabled(false);
            buildTownLayout();
            result.transitioned = true;
            result.toZone = WorldZone::TOWN;
            result.newPlayerPosition = player_.position();
        }
    }

    return result;
}

bool ZoneManager::deactivateEntity(const std::uint32_t entityId) noexcept {
    for (WorldEntitySnapshot& entity : scenery_) {
        if (entity.id == entityId) {
            entity.active = false;
            mobController_.unregisterMob(entityId);
            ++sceneryRevision_;
            return true;
        }
    }
    return false;
}

void ZoneManager::resetMobSimulation() {
    mobController_.reset();
    for (const WorldEntitySnapshot& entity : scenery_) {
        if (entity.active && (entity.kind == EntityKind::ENEMY_MOB || entity.kind == EntityKind::ENEMY_BOSS)) {
            mobController_.registerMob(entity.id, entity.kind);
        }
    }
}

void ZoneManager::updatePlainsSimulation(const float deltaSeconds, const Vec3& playerPosition) {
    if (activeZone_ != WorldZone::PLAINS) {
        return;
    }

    const std::size_t sceneryCountBefore = scenery_.size();
    mobController_.update(
        deltaSeconds, scenery_, plainsBounds_, playerPosition, nextEntityId_, MobSpawnSettings{});
    if (scenery_.size() != sceneryCountBefore) {
        ++sceneryRevision_;
    }
}

bool ZoneManager::isInsideBlacksmithRadius(const Vec3& position) const noexcept {
    const float dx = position.x - blacksmith_.position().x;
    const float dz = position.z - blacksmith_.position().z;
    const float radius = blacksmith_.interactionRadius();
    return (dx * dx + dz * dz) <= (radius * radius);
}

void ZoneManager::respawnPlainsContent() {
    buildPlainsLayout(plainsSeed_, plainsDepth_);
}

void ZoneManager::respawnPlainsContent(const std::uint32_t seed, const int depth) {
    plainsSeed_ = seed;
    plainsDepth_ = std::max(1, depth);
    buildPlainsLayout(plainsSeed_, plainsDepth_);
}

void ZoneManager::forceRespawnInTown() {
    activeZone_ = WorldZone::TOWN;
    player_.setAttacksEnabled(false);
    player_.setPosition(Vec3{0.0F, 0.0F, 0.0F});
    buildTownLayout();
}

void ZoneManager::restoreFromSnapshot(
    const WorldZone zone,
    const Vec3& playerPosition,
    const int playerGold,
    const bool attacksEnabled,
    const std::uint32_t plainsSeed,
    const int plainsDepth,
    std::vector<WorldEntitySnapshot> scenery) {
    activeZone_ = zone;
    plainsSeed_ = plainsSeed;
    plainsDepth_ = std::max(1, plainsDepth);
    scenery_ = std::move(scenery);

    std::uint32_t maxEntityId = player_.id();
    for (const WorldEntitySnapshot& entity : scenery_) {
        maxEntityId = std::max(maxEntityId, entity.id);
    }
    nextEntityId_ = maxEntityId + 1U;

    player_.setPosition(playerPosition);
    player_.setGold(playerGold);
    player_.setAttacksEnabled(attacksEnabled);
    ++sceneryRevision_;
    resetMobSimulation();
}

void ZoneManager::buildTownLayout() {
    scenery_.clear();
    ++sceneryRevision_;
    scenery_.push_back(blacksmith_.snapshot());

    static constexpr PropSpawn kTownProps[] = {
        {EntityKind::ENV_HOUSE, -22.0F, -14.0F, 0},
        {EntityKind::ENV_HOUSE, 24.0F, -16.0F, 1},
        {EntityKind::ENV_TREE, 16.0F, -10.0F, 0},
        {EntityKind::ENV_TREE, -16.0F, 12.0F, 1},
        {EntityKind::ENV_BUSH, -14.0F, 4.0F, 2},
        {EntityKind::ENV_BUSH, 10.0F, 8.0F, 5},
        {EntityKind::ENV_BUSH, -8.0F, -12.0F, 8},
        {EntityKind::ENV_ROCK, -12.0F, 2.0F, 3},
        {EntityKind::ENV_ROCK, 14.0F, 4.0F, 7},
        {EntityKind::ENV_ROCK, -6.0F, 14.0F, 11},
        {EntityKind::ENV_MUSHROOM, 6.0F, -6.0F, 0},
        {EntityKind::ENV_MUSHROOM, -4.0F, 6.0F, 2},
        {EntityKind::ENV_CHEST, -5.0F, -4.0F, 0},
    };
    appendProps(scenery_, nextEntityId_, kTownProps, sizeof(kTownProps) / sizeof(kTownProps[0]));
}

void ZoneManager::buildPlainsLayout(const std::uint32_t seed, const int depth) {
    scenery_.clear();
    ++sceneryRevision_;

    ZoneLayoutSpec spec{};
    spec.seed = seed;
    spec.depth = depth;
    spec.bounds = plainsBounds_;
    const GeneratedZoneLayout generated = generatePlainsLayout(spec);

    appendProps(scenery_, nextEntityId_, generated.props.data(), generated.props.size());

    for (const Vec3& mobSpawn : generated.mobSpawns) {
        scenery_.push_back(
            makeEntity(nextEntityId_++, EntityKind::ENEMY_MOB, mobSpawn.x, mobSpawn.y, mobSpawn.z));
    }

    scenery_.push_back(makeEntity(
        nextEntityId_++,
        EntityKind::ENEMY_BOSS,
        generated.bossSpawn.x,
        generated.bossSpawn.y,
        generated.bossSpawn.z));
    resetMobSimulation();
}

void ZoneManager::shiftPlayerTownToPlains() {
    player_.setPosition(Vec3{0.0F, 0.0F, 60.0F});
}

void ZoneManager::shiftPlayerPlainsToTown() {
    player_.setPosition(Vec3{0.0F, 0.0F, 0.0F});
}

} // namespace gameplay
