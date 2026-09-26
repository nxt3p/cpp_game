#include <catch2/catch_test_macros.hpp>

#include "systems/Inventory.hpp"
#include "systems/SlotMachineLoot.hpp"

namespace {

/// Grinds `spins` actions of `action` against `tier` and returns the telemetry.
systems::LootTelemetry grind(
    const std::uint32_t seed,
    const int depth,
    const systems::ActionType action,
    const systems::EntityTier tier,
    const int spins,
    const float tierBonus = 0.0F) {
    systems::SlotMachineLoot loot(seed);
    loot.setZoneDepth(depth);
    loot.setLootTierBonus(tierBonus);
    for (int index = 0; index < spins; ++index) {
        loot.insertCoins(action);
        static_cast<void>(loot.spin(tier));
    }
    return loot.telemetry();
}

} // namespace

TEST_CASE("Slot machine reel odds always sum to one", "[loot][telemetry]") {
    systems::SlotMachineLoot loot(1U);
    for (int coins : {0, 10, 60, 150, 400}) {
        loot.setCoinPool(coins);
        for (int pity : {0, 20, 60, 139, 140, 500}) {
            loot.setPityCounter(pity);
            for (const systems::EntityTier tier :
                 {systems::EntityTier::Minor,
                  systems::EntityTier::Standard,
                  systems::EntityTier::Elite,
                  systems::EntityTier::Boss}) {
                const systems::LootReelOdds odds = loot.oddsFor(tier);
                const float total = odds.nothing + odds.common + odds.medium + odds.jackpot;
                CHECK(total > 0.999F);
                CHECK(total < 1.001F);
                CHECK(odds.jackpot >= 0.0F);
                if (pity >= systems::SlotMachineLoot::kPityHardCap) {
                    CHECK(odds.jackpot == 1.0F);
                }
            }
        }
    }
}

TEST_CASE("Opening 1,000 chests lands inside the tuned loot bands", "[loot][telemetry]") {
    const systems::LootTelemetry chests =
        grind(4242U, 2, systems::ActionType::CHEST_OPEN, systems::EntityTier::Standard, 1000);

    REQUIRE(chests.spins == 1000);
    CHECK(chests.coinsInserted == 5000);

    // Jackpots are the low-percentage headline: roughly 1.5% - 4% on standard content.
    CHECK(chests.jackpotRate() > 0.010F);
    CHECK(chests.jackpotRate() < 0.045F);

    // Medium reel (materials / socketed gear) should be a meaningful but not dominant slice.
    CHECK(chests.mediumRate() > 0.12F);
    CHECK(chests.mediumRate() < 0.30F);

    // Dead spins stay under a third so chests still feel worth opening.
    CHECK(chests.nothingRate() < 0.40F);

    CHECK(chests.legendaryItems + chests.uniqueItems == chests.jackpot);
    CHECK(chests.uniqueItems > 0);
    CHECK(chests.legendaryItems > chests.uniqueItems);
    CHECK(chests.socketedItems > 0);
    CHECK(chests.materials > 0);
    CHECK(chests.consumables > 0);
    CHECK(chests.goldTotal > 0);
}

TEST_CASE("Grinding 5,000 mobs never exceeds the pity hard cap", "[loot][telemetry]") {
    systems::SlotMachineLoot loot(777U);
    loot.setZoneDepth(3);

    int longestDryStreak = 0;
    int dryStreak = 0;
    for (int kill = 0; kill < 5000; ++kill) {
        loot.insertCoins(systems::ActionType::MOB_KILL);
        const systems::LootSpinResult spin = loot.spin(systems::EntityTier::Standard);
        REQUIRE(spin.spun);
        if (spin.jackpot) {
            dryStreak = 0;
            CHECK(spin.coinPoolAfter == 0);
            REQUIRE(spin.prizes.size() == 3);
        } else {
            ++dryStreak;
            longestDryStreak = std::max(longestDryStreak, dryStreak);
        }
        CHECK(spin.pityCounterAfter <= systems::SlotMachineLoot::kPityHardCap);
    }

    CHECK(longestDryStreak <= systems::SlotMachineLoot::kPityHardCap);
    const systems::LootTelemetry& telemetry = loot.telemetry();
    CHECK(telemetry.jackpotRate() > 0.012F);
    CHECK(telemetry.jackpotRate() < 0.05F);
}

TEST_CASE("Bosses pay out jackpots far more often than rocks", "[loot][telemetry]") {
    const systems::LootTelemetry rocks =
        grind(9U, 1, systems::ActionType::ROCK_CLICK, systems::EntityTier::Minor, 2000);
    const systems::LootTelemetry bosses =
        grind(11U, 5, systems::ActionType::BOSS_KILL, systems::EntityTier::Boss, 300, 0.16F);

    CHECK(rocks.jackpotRate() < 0.03F);
    CHECK(rocks.nothingRate() > 0.45F);
    CHECK(bosses.nothingRate() == 0.0F);
    CHECK(bosses.jackpotRate() > 0.30F);
    CHECK(bosses.jackpotRate() > rocks.jackpotRate() * 8.0F);
}

TEST_CASE("Slot machine payouts are deterministic per seed and fit in a bag", "[loot][telemetry]") {
    systems::SlotMachineLoot first(31337U);
    systems::SlotMachineLoot second(31337U);
    systems::Inventory bag(6, 4);

    for (int index = 0; index < 40; ++index) {
        first.insertCoins(systems::ActionType::MOB_KILL);
        second.insertCoins(systems::ActionType::MOB_KILL);
        const systems::LootSpinResult a = first.spin(systems::EntityTier::Elite);
        const systems::LootSpinResult b = second.spin(systems::EntityTier::Elite);
        REQUIRE(a.tier == b.tier);
        REQUIRE(a.prizes.size() == b.prizes.size());
        for (std::size_t prize = 0; prize < a.prizes.size(); ++prize) {
            CHECK(a.prizes[prize].kind == b.prizes[prize].kind);
            CHECK(a.prizes[prize].goldAmount == b.prizes[prize].goldAmount);
            if (a.prizes[prize].item.has_value()) {
                REQUIRE(b.prizes[prize].item.has_value());
                CHECK(a.prizes[prize].item->name == b.prizes[prize].item->name);
                CHECK(a.prizes[prize].item->itemId == b.prizes[prize].item->itemId);
                if (bag.usedSlots() < bag.capacity()) {
                    CHECK(bag.addItem(*a.prizes[prize].item).success);
                }
            }
        }
    }
}

TEST_CASE("Spinning with an empty coin pool does nothing", "[loot][telemetry]") {
    systems::SlotMachineLoot loot(5U);
    const systems::LootSpinResult spin = loot.spin(systems::EntityTier::Boss);
    CHECK_FALSE(spin.spun);
    CHECK(spin.prizes.empty());
    CHECK(loot.telemetry().spins == 0);
}
