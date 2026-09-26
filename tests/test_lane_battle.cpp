#include <catch2/catch_test_macros.hpp>

#include "gameplay/IsometricCamera.hpp"
#include "gameplay/LaneBattle.hpp"
#include "systems/SlotMachineLoot.hpp"

#include <glm/gtc/matrix_transform.hpp>

TEST_CASE("Lane packs spawn in groups and the road clears after the last pack", "[lane]") {
    gameplay::LaneBattle lane;
    lane.start(0, 7U);
    CHECK(lane.phase() == gameplay::LanePhase::Running);
    CHECK(gameplay::LaneBattle::nodeUnlocked(0, 1));
    CHECK_FALSE(gameplay::LaneBattle::nodeUnlocked(1, 1));

    int spawned = 0;
    int elites = 0;
    for (int step = 0; step < 40 && lane.phase() != gameplay::LanePhase::Cleared; ++step) {
        lane.update(0.2F, 0, 99.0F);
        if (!lane.wantsSpawn()) {
            continue;
        }
        const gameplay::LaneSpawnRequest request = lane.consumeSpawn();
        CHECK(request.count >= 2);
        CHECK(request.count <= 5);
        elites += request.eliteCount;
        ++spawned;
        lane.update(0.1F, request.count, 1.0F);
        CHECK(lane.phase() == gameplay::LanePhase::Fighting);
        lane.update(0.1F, 0, 99.0F);
    }

    CHECK(spawned == gameplay::LaneBattle::nodeAt(0).groupCount);
    CHECK(lane.phase() == gameplay::LanePhase::Cleared);
    CHECK(lane.consumeVictory());
    CHECK(elites >= 0);
}

TEST_CASE("Later nodes raise the loot ceiling and the finale can include a boss", "[lane]") {
    const gameplay::BattleNode& meadow = gameplay::LaneBattle::nodeAt(0);
    const gameplay::BattleNode& throne = gameplay::LaneBattle::nodeAt(7);
    CHECK(meadow.ceiling == gameplay::LootCeiling::Magic);
    CHECK(throne.ceiling == gameplay::LootCeiling::Unique);
    CHECK(throne.enemyLevel > meadow.enemyLevel);
    CHECK(throne.bossFinale);

    gameplay::LaneBattle lane;
    lane.start(3, 99U);
    bool sawBoss = false;
    for (int step = 0; step < 30 && !sawBoss; ++step) {
        lane.update(1.0F, 0, 99.0F);
        if (!lane.wantsSpawn()) {
            continue;
        }
        const gameplay::LaneSpawnRequest request = lane.consumeSpawn();
        sawBoss = sawBoss || request.boss;
        lane.update(0.1F, 0, 99.0F);
    }
    CHECK(sawBoss);
}

TEST_CASE("Lane camera keeps the run axis on screen right", "[lane]") {
    gameplay::IsometricCamera camera;
    camera.setViewportSize(1280, 720);
    camera.setFollowOffset(glm::vec3(0.0F, 30.0F, 16.0F));
    camera.setOrthoHeight(16.0F);
    const gameplay::CameraMatrices matrices = camera.matricesForTarget(gameplay::Vec3{5.0F, 0.0F, 60.0F});
    const glm::mat4 viewProjection = matrices.projection * matrices.view;

    const auto screenX = [&](const float worldX) {
        const glm::vec4 clip = viewProjection * glm::vec4(worldX, 0.0F, 60.0F, 1.0F);
        return (glm::vec3(clip) / clip.w).x;
    };

    CHECK(screenX(12.0F) > screenX(0.0F));
    CHECK(screenX(0.0F) > -0.95F);
    CHECK(screenX(12.0F) < 0.95F);
}

TEST_CASE("Magic roads cannot pay legendary or unique jackpots", "[lane][loot]") {
    systems::SlotMachineLoot loot(11U);
    loot.setLootCeiling(systems::LootCeiling::Magic);
    loot.setCoinPool(400);
    loot.setPityCounter(systems::SlotMachineLoot::kPityHardCap);

    for (int spin = 0; spin < 40; ++spin) {
        const systems::LootSpinResult result = loot.spin(systems::EntityTier::Boss);
        CHECK(result.tier != systems::LootReelTier::Jackpot);
        for (const systems::LootPrize& prize : result.prizes) {
            CHECK(prize.kind != systems::LootPrizeKind::Legendary);
            CHECK(prize.kind != systems::LootPrizeKind::Unique);
        }
    }
}
