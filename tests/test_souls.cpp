#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "systems/SoulProgression.hpp"
#include "ui/UiTypes.hpp"

TEST_CASE("Soul upgrades only work in town and consume carried souls", "[souls]") {
    ui::CharacterScreenData stats{};
    stats.carriedSouls = 200;
    stats.statUpgradesPurchased = 0;

    const systems::SoulUpgradeResult blocked =
        systems::tryPurchaseStatUpgrade(stats, systems::SoulStatKind::Strength, false);
    CHECK_FALSE(blocked.success);
    CHECK(stats.carriedSouls == 200);

    const systems::SoulUpgradeResult success =
        systems::tryPurchaseStatUpgrade(stats, systems::SoulStatKind::Strength, true);
    REQUIRE(success.success);
    CHECK(success.soulsSpent == 75);
    CHECK(stats.carriedSouls == 125);
    CHECK(stats.strength == 11);
    CHECK(stats.level == 2);
    CHECK(stats.statUpgradesPurchased == 1);
}

TEST_CASE("Soul upgrade cost escalates with purchases", "[souls]") {
    CHECK(systems::soulUpgradeCost(0) == 75);
    CHECK(systems::soulUpgradeCost(2) == 125);
}

TEST_CASE("Mob melee damage scales with difficulty modifiers", "[souls]") {
    using gameplay::EntityKind;

    const int mobBase = systems::mobMeleeDamage(EntityKind::ENEMY_MOB, 1.0F, 1.0F);
    const int mobScaled = systems::mobMeleeDamage(EntityKind::ENEMY_MOB, 1.35F, 1.15F);
    const int bossBase = systems::mobMeleeDamage(EntityKind::ENEMY_BOSS, 1.0F, 1.0F);

    CHECK(mobBase == 12);
    CHECK(bossBase == 28);
    CHECK(mobScaled > mobBase);
}

TEST_CASE("Soul gain multiplier scales rewards and stacks on kills", "[souls]") {
    ui::CharacterScreenData stats{};
    stats.soulGainMultiplier = systems::kInitialSoulGainMultiplier;

    CHECK(systems::scaleSoulReward(35, stats.soulGainMultiplier) == 35);

    const int secondKillReward = systems::scaleSoulReward(35, 1.02F);
    CHECK(secondKillReward == 36);

    systems::registerMobSoulGain(stats);
    CHECK(stats.soulGainMultiplier == Catch::Approx(1.02F).margin(1e-4F));
    systems::registerMobSoulGain(stats);
    CHECK(stats.soulGainMultiplier == Catch::Approx(1.04F).margin(1e-4F));

    systems::registerBossSoulGain(stats);
    CHECK(stats.soulGainMultiplier == Catch::Approx(1.12F).margin(1e-4F));

    systems::resetSoulGainMultiplier(stats);
    CHECK(stats.soulGainMultiplier == Catch::Approx(1.0F).margin(1e-4F));
}
