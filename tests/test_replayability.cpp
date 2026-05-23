#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/CombatSystem.hpp"
#include "gameplay/ProceduralZoneGenerator.hpp"
#include "gameplay/ZoneManager.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemGenerator.hpp"
#include "systems/LootEngine.hpp"
#include "systems/RunProgression.hpp"

TEST_CASE("ItemGenerator rolls equippable categories with affix names", "[replayability][items]") {
    systems::ItemGenerator generator(777U);
    systems::ItemGenerationContext context{};
    context.rarity = systems::ItemRarity::Rare;
    context.tier = systems::EntityTier::Standard;
    context.zoneDepth = 3;

    const systems::ItemMetadata item = generator.generate(context);
    CHECK(item.itemId >= 4000U);
    CHECK(item.itemLevel >= 3);
    CHECK((item.name.find(' ') != std::string::npos));
    const bool equippableCategory =
        item.category != systems::ItemCategory::Consumable &&
        item.category != systems::ItemCategory::Misc;
    CHECK(equippableCategory);
    CHECK(item.bonuses.strength + item.bonuses.dexterity + item.bonuses.damage > 0);
}

TEST_CASE("RunProgression scales mob difficulty with depth", "[replayability][progression]") {
    systems::RunProgression run(42U);
    const systems::DifficultyModifiers depth1 = run.modifiers();
    CHECK(depth1.mobHpMultiplier == Catch::Approx(1.35F).margin(1e-3F));
    CHECK(depth1.mobSpawnBudget == 5);

    run.onBossDefeated();
    run.onBossDefeated();
    const systems::DifficultyModifiers depth3 = run.modifiers();
    CHECK(depth3.mobHpMultiplier == Catch::Approx(2.45F).margin(1e-3F));
    CHECK(depth3.itemLevel == 3);
    CHECK(depth3.mobSpawnBudget == 7);
    CHECK(run.runSeed() != 42U);
}

TEST_CASE("Procedural plains layout grows with depth and stays seeded", "[replayability][zone]") {
    gameplay::ZoneLayoutSpec spec{};
    spec.bounds = gameplay::AxisAlignedBounds{-120.0F, 120.0F, 40.0F, 200.0F};
    spec.seed = 9001U;
    spec.depth = 1;

    const gameplay::GeneratedZoneLayout shallow = gameplay::generatePlainsLayout(spec);
    spec.depth = 5;
    const gameplay::GeneratedZoneLayout deep = gameplay::generatePlainsLayout(spec);

    CHECK(shallow.mobSpawns.size() >= 4);
    CHECK(deep.mobSpawns.size() > shallow.mobSpawns.size());
    CHECK(deep.props.size() > shallow.props.size());
    CHECK(deep.bossSpawn.z >= spec.bounds.minZ);

    spec.depth = 5;
    const gameplay::GeneratedZoneLayout deepAgain = gameplay::generatePlainsLayout(spec);
    CHECK(deep.mobSpawns.size() == deepAgain.mobSpawns.size());
    CHECK(deep.props.size() == deepAgain.props.size());
}

TEST_CASE("Loot loop deposits generated gear into inventory", "[replayability][loot]") {
    systems::LootEngine loot(1234U);
    systems::Inventory inventory(6, 4);
    loot.setZoneDepth(2);
    loot.registerAction(systems::ActionType::MOB_KILL);
    loot.registerAction(systems::ActionType::CHEST_OPEN);

    const systems::LootDropResult drop = loot.triggerDropCheck(systems::EntityTier::Standard);
    REQUIRE(drop.dropped);
    CHECK(drop.item.itemId >= 4000U);

    const systems::InventoryAddResult added = inventory.addItem(drop.item);
    REQUIRE(added.success);
    CHECK(inventory.usedSlots() == 1);
}

TEST_CASE("CombatSystem applies difficulty multipliers to mob profiles", "[replayability][combat]") {
    game::CombatSystem combat;
    systems::DifficultyModifiers modifiers{};
    modifiers.mobHpMultiplier = 2.0F;
    modifiers.mobXpMultiplier = 1.5F;
    combat.setDifficultyModifiers(modifiers);

    std::vector<gameplay::WorldEntitySnapshot> scenery;
    scenery.push_back({500U, gameplay::EntityKind::ENEMY_MOB, gameplay::Vec3{}, true});
    combat.syncScenery(scenery);

    const std::optional<game::MobHealthSnapshot> health = combat.mobHealth(500U);
    REQUIRE(health.has_value());
    CHECK(health->maxHp == 144);

    const std::optional<game::DamageResult> result = combat.applyDamage(500U, 144);
    REQUIRE(result.has_value());
    CHECK(result->killed);
    CHECK(result->xpReward == 53);
}

TEST_CASE("ZoneManager procedural respawn increases content at higher depth", "[replayability][zone]") {
    gameplay::ZoneManager zones;
    zones.respawnPlainsContent(4242U, 1);
    const std::size_t depth1Count = zones.scenery().size();

    zones.respawnPlainsContent(4242U, 4);
    const std::size_t depth4Count = zones.scenery().size();

    CHECK(depth4Count > depth1Count);
    CHECK(zones.scenery().size() >= 8);
}
