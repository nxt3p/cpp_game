#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/CombatSystem.hpp"
#include "systems/CharacterCombat.hpp"

TEST_CASE("Character combat stats scale with strength and dexterity", "[combat]") {
    const systems::CombatStatInput low{1, 10, 10};
    const systems::CombatStatInput high{5, 20, 18};

    CHECK(systems::computeDamage(low) == 10);
    CHECK(systems::computeDamage(high) == 28);
    CHECK(systems::computeAttacksPerSecond(low) == Catch::Approx(1.5F).margin(1e-4F));
    CHECK(systems::computeAttacksPerSecond(high) == Catch::Approx(1.9F).margin(1e-4F));
}

TEST_CASE("CombatSystem applies damage and awards XP on kill", "[combat]") {
    game::CombatSystem combat;
    const std::vector<gameplay::WorldEntitySnapshot> scenery{
        {200U, gameplay::EntityKind::ENEMY_MOB, gameplay::Vec3{1.0F, 0.0F, 2.0F}, true}};

    combat.syncScenery(scenery);
    combat.setTarget(200U);

    const std::optional<game::DamageResult> result = combat.applyDamage(200U, 72);
    REQUIRE(result.has_value());
    CHECK(result->killed);
    CHECK(result->xpReward == 35);
    CHECK_FALSE(combat.isMobAlive(200U));
    CHECK_FALSE(combat.hasTarget());
}
