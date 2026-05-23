#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "gameplay/MobController.hpp"
#include "gameplay/ZoneManager.hpp"
#include "render/SpriteSheet.hpp"
#include "render/WorldPropAssets.hpp"

#include <string>

TEST_CASE("sanitizeSpriteFrameUV preserves width for flipped frames", "[render][mobs]") {
    const render::SpriteFrameUV flipped{0.25F, 0.0F, 0.0F, 0.25F};
    const render::SpriteFrameUV fixed = render::sanitizeSpriteFrameUV(flipped, 4, 4);
    REQUIRE(fixed.u0 < fixed.u1);
    REQUIRE(fixed.v0 < fixed.v1);
    CHECK(fixed.u1 - fixed.u0 >= 0.12F);
    CHECK(fixed.v1 - fixed.v0 >= 0.12F);
}

TEST_CASE("Sprite clips stay inside compact mob sheets", "[render][mobs]") {
    const render::SpriteSheetClip walk = render::resolveSheetClip(
        render::SpriteClip::Walk, render::SpriteFacing::Down, 4, 4);
    CHECK(walk.row == 0);
    CHECK(walk.frameCount == 4);

    const render::SpriteSheetClip attack = render::resolveSheetClip(
        render::SpriteClip::Attack, render::SpriteFacing::Left, 4, 4);
    CHECK(attack.row >= 0);
    CHECK(attack.row < 4);
    CHECK(attack.frameCount <= 4);
    CHECK(attack.flipHorizontal);
}

TEST_CASE("Sprite clips use LPC rows on large class sheets", "[render][mobs]") {
    const render::SpriteSheetClip walk = render::resolveSheetClip(
        render::SpriteClip::Walk, render::SpriteFacing::Down, 22, 12);
    CHECK(walk.row == 11);
    CHECK(walk.frameCount == 6);

    const render::SpriteSheetClip attackDown = render::resolveSheetClip(
        render::SpriteClip::Attack, render::SpriteFacing::Down, 22, 12);
    CHECK(attackDown.row == 8);
    CHECK(attackDown.frameCount == 11);

    const render::SpriteSheetClip attackRight = render::resolveSheetClip(
        render::SpriteClip::Attack, render::SpriteFacing::Right, 22, 12);
    CHECK(attackRight.row == 10);
    CHECK(attackRight.frameCount == 8);
}

TEST_CASE("Warrior class sheet skips blank attack frames", "[render][mobs]") {
    render::SpriteSheet sheet;
    const std::string path = std::string(ENGINE_ASSETS_DIR) + "/textures/mobs/warrior.png";
    REQUIRE(sheet.loadFromFile(path));

    CHECK(sheet.rowCount() == 22);
    CHECK(sheet.columnCount() == 12);
    const int attackRowFrames = sheet.occupiedFrameCount(10);
    CHECK(attackRowFrames >= 6);
    CHECK(attackRowFrames <= 12);

    const render::SpriteSheetClip attackClip =
        sheet.clip(render::SpriteClip::Attack, render::SpriteFacing::Right);
    CHECK(attackClip.row == 10);
    CHECK(attackClip.frameCount == attackRowFrames);
    CHECK(attackClip.frameCount <= 8);
}

TEST_CASE("Sprite clips fall back when LPC rows are missing", "[render][mobs]") {
    const render::SpriteSheetClip walk = render::resolveSheetClip(
        render::SpriteClip::Walk, render::SpriteFacing::Up, 8, 4);
    CHECK(walk.row == 1);
    CHECK(walk.frameCount == 4);
}

TEST_CASE("World prop catalog loads sliced assets manifest", "[render][world]") {
    render::WorldPropAssets assets;
    const std::string worldDir = std::string(ENGINE_ASSETS_DIR) + "/textures/world";
    REQUIRE(assets.load(worldDir));
    CHECK(assets.variantCount(gameplay::EntityKind::ENV_TREE) >= 3);
    CHECK(assets.variantCount(gameplay::EntityKind::ENV_BUSH) >= 5);
    CHECK(assets.variantCount(gameplay::EntityKind::ENV_ROCK) >= 10);
    CHECK(assets.variantCount(gameplay::EntityKind::ENV_HOUSE) >= 2);
    CHECK(assets.variantCount(gameplay::EntityKind::ENV_CHEST) >= 1);
    CHECK(assets.variantCount(gameplay::EntityKind::ENV_MUSHROOM) >= 4);
    REQUIRE(assets.texture(gameplay::EntityKind::ENV_TREE, 0) != nullptr);
    CHECK(assets.worldHeight(gameplay::EntityKind::ENV_TREE, 0) > 2.0F);
}

TEST_CASE("MobController wanders mobs within bounds", "[gameplay][mobs]") {
    gameplay::MobController controller{42U};
    std::vector<gameplay::WorldEntitySnapshot> scenery{
        {500U, gameplay::EntityKind::ENEMY_MOB, gameplay::Vec3{0.0F, 0.0F, 80.0F}, true}};

    controller.registerMob(500U, gameplay::EntityKind::ENEMY_MOB);

    const gameplay::AxisAlignedBounds bounds{-40.0F, 40.0F, 60.0F, 120.0F};
    const gameplay::Vec3 player{0.0F, 0.0F, 60.0F};
    std::uint32_t nextId = 600U;

    gameplay::MobSpawnSettings settings{};
    settings.maxMobs = 1;
    settings.minSpawnIntervalSeconds = 999.0F;
    settings.maxSpawnIntervalSeconds = 999.0F;

    const gameplay::Vec3 start = scenery.front().position;
    for (int step = 0; step < 120; ++step) {
        controller.update(0.05F, scenery, bounds, player, nextId, settings);
    }

    const gameplay::Vec3 end = scenery.front().position;
    const float movedDistance = std::sqrt(
        (end.x - start.x) * (end.x - start.x) + (end.z - start.z) * (end.z - start.z));
    CHECK(movedDistance > 1.0F);
    CHECK(end.x >= bounds.minX);
    CHECK(end.x <= bounds.maxX);
    CHECK(end.z >= bounds.minZ);
    CHECK(end.z <= bounds.maxZ);
}

TEST_CASE("MobController spawns mobs up to cap", "[gameplay][mobs]") {
    gameplay::MobController controller{99U};
    std::vector<gameplay::WorldEntitySnapshot> scenery;
    const gameplay::AxisAlignedBounds bounds{-120.0F, 120.0F, 40.0F, 200.0F};
    const gameplay::Vec3 player{0.0F, 0.0F, 60.0F};
    std::uint32_t nextId = 700U;

    gameplay::MobSpawnSettings settings{};
    settings.maxMobs = 3;
    settings.minSpawnIntervalSeconds = 0.0F;
    settings.maxSpawnIntervalSeconds = 0.1F;
    settings.minSpawnDistanceFromPlayer = 10.0F;

    for (int step = 0; step < 80; ++step) {
        controller.update(0.1F, scenery, bounds, player, nextId, settings);
    }

    CHECK(controller.activeMobCount(scenery) <= settings.maxMobs);
    CHECK(controller.activeMobCount(scenery) >= 2);
}

TEST_CASE("MobController chases player inside aggro radius", "[gameplay][mobs]") {
    gameplay::MobController controller{7U};
    std::vector<gameplay::WorldEntitySnapshot> scenery{
        {800U, gameplay::EntityKind::ENEMY_MOB, gameplay::Vec3{20.0F, 0.0F, 80.0F}, true}};

    controller.registerMob(800U, gameplay::EntityKind::ENEMY_MOB);

    const gameplay::AxisAlignedBounds bounds{-40.0F, 40.0F, 60.0F, 120.0F};
    const gameplay::Vec3 player{0.0F, 0.0F, 80.0F};
    std::uint32_t nextId = 900U;

    gameplay::MobSpawnSettings settings{};
    settings.maxMobs = 1;
    settings.minSpawnIntervalSeconds = 999.0F;
    settings.maxSpawnIntervalSeconds = 999.0F;
    settings.aggroRadius = 30.0F;
    settings.deaggroRadius = 40.0F;
    settings.chaseSpeedMultiplier = 2.0F;

    const float startDistance = std::abs(scenery.front().position.x - player.x);
    for (int step = 0; step < 80; ++step) {
        controller.update(0.05F, scenery, bounds, player, nextId, settings);
    }

    const float endDistance = std::abs(scenery.front().position.x - player.x);
    CHECK(endDistance < startDistance);
}

TEST_CASE("ZoneManager updates plains mob simulation", "[gameplay][mobs]") {
    gameplay::ZoneManager zones;
    zones.updatePlayerPosition(gameplay::Vec3{0.0F, 0.0F, 40.0F});
    REQUIRE(zones.activeZone() == gameplay::WorldZone::PLAINS);

    const int initialMobs = zones.scenery().size();
    zones.updatePlainsSimulation(5.0F, zones.player().position());

    CHECK(zones.scenery().size() >= static_cast<std::size_t>(initialMobs));
}
