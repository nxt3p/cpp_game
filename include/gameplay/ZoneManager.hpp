#pragma once

#include "gameplay/Entity.hpp"
#include "gameplay/GameTypes.hpp"
#include "gameplay/MobController.hpp"

#include <vector>

namespace gameplay {

struct ZoneTransitionResult {
    bool transitioned{false};
    WorldZone fromZone{WorldZone::TOWN};
    WorldZone toZone{WorldZone::TOWN};
    Vec3 newPlayerPosition{};
};

class ZoneManager {
public:
    ZoneManager();

    [[nodiscard]] WorldZone activeZone() const noexcept { return activeZone_; }
    [[nodiscard]] const PlayerEntity& player() const noexcept { return player_; }
    [[nodiscard]] PlayerEntity& player() noexcept { return player_; }

    [[nodiscard]] const std::vector<WorldEntitySnapshot>& scenery() const noexcept {
        return scenery_;
    }

    [[nodiscard]] std::uint32_t sceneryRevision() const noexcept { return sceneryRevision_; }

    [[nodiscard]] bool deactivateEntity(std::uint32_t entityId) noexcept;

    [[nodiscard]] const AxisAlignedBounds& townBounds() const noexcept { return townBounds_; }
    [[nodiscard]] const AxisAlignedBounds& plainsBounds() const noexcept { return plainsBounds_; }

    ZoneTransitionResult updatePlayerPosition(const Vec3& position);
    [[nodiscard]] bool isInsideBlacksmithRadius(const Vec3& position) const noexcept;

    void respawnPlainsContent();
    void respawnPlainsContent(std::uint32_t seed, int depth);

    void forceRespawnInTown();

    [[nodiscard]] std::uint32_t plainsSeed() const noexcept { return plainsSeed_; }
    [[nodiscard]] int plainsDepth() const noexcept { return plainsDepth_; }

    void restoreFromSnapshot(
        WorldZone zone,
        const Vec3& playerPosition,
        int playerGold,
        bool attacksEnabled,
        std::uint32_t plainsSeed,
        int plainsDepth,
        std::vector<WorldEntitySnapshot> scenery);

    void updatePlainsSimulation(float deltaSeconds, const Vec3& playerPosition);
    void resetMobSimulation();

private:
    void buildTownLayout();
    void buildPlainsLayout(std::uint32_t seed, int depth);
    void shiftPlayerTownToPlains();
    void shiftPlayerPlainsToTown();

    WorldZone activeZone_{WorldZone::TOWN};
    PlayerEntity player_;
    NpcBlacksmithEntity blacksmith_;
    AxisAlignedBounds townBounds_{-40.0F, 40.0F, -40.0F, 40.0F};
    AxisAlignedBounds plainsBounds_{-120.0F, 120.0F, 40.0F, 200.0F};
    AxisAlignedBounds exitGateBounds_{-6.0F, 6.0F, 38.0F, 42.0F};
    AxisAlignedBounds returnGateBounds_{-6.0F, 6.0F, 38.0F, 42.0F};
    std::vector<WorldEntitySnapshot> scenery_;
    MobController mobController_;
    std::uint32_t nextEntityId_{100};
    std::uint32_t plainsSeed_{0x9A100001U};
    int plainsDepth_{1};
    std::uint32_t sceneryRevision_{0};
};

} // namespace gameplay
