#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "systems/LootEngine.hpp"
#include "systems/SlotMachineLoot.hpp"
#include "systems/SoulProgression.hpp"
#include "systems/TownHub.hpp"
#include "ui/UiTypes.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

namespace {

struct Pace {
    int blacksmithKill{0};
    int chapelKill{0};
    int tavernKill{0};
    int levelAtTavern{0};
    int goldLeft{0};
    bool tavernOpen{false};
};

struct LootTally {
    int bosses{0};
    int bossLegendary{0};
    int runsWithBossLegendary{0};
    int combatMythical{0};
    int common{0};
    int magic{0};
    int rare{0};
    int legendary{0};
    int unique{0};
};

[[nodiscard]] Pace marchUntilTavern(const int killBudget) {
    systems::TownHub town;
    ui::CharacterScreenData stats{};
    stats.level = 1;
    stats.experienceToNextLevel = systems::experienceRequiredForLevel(1);
    int gold = 0;
    int depth = 1;
    Pace pace{};

    for (int kill = 1; kill <= killBudget && !pace.tavernOpen; ++kill) {
        const int slot = (kill - 1) % 19;
        const bool boss = slot == 18;
        const bool elite = slot == 8 || slot == 17;
        const systems::CombatKillReward reward = systems::combatKillReward(boss, elite, depth);
        gold += reward.gold;
        static_cast<void>(systems::grantCombatExperience(stats, reward.experience));

        const auto tryBuilding = [&](const systems::TownBuilding building, int& when) {
            if (when != 0 || town.isRepaired(building)) {
                return;
            }
            if (town.tryRepair(building, gold, stats.level) == systems::TownRepairResult::Repaired) {
                when = kill;
            }
        };
        tryBuilding(systems::TownBuilding::Blacksmith, pace.blacksmithKill);
        tryBuilding(systems::TownBuilding::Healer, pace.chapelKill);
        tryBuilding(systems::TownBuilding::Tavern, pace.tavernKill);
        if (pace.tavernKill != 0) {
            pace.tavernOpen = true;
            pace.levelAtTavern = stats.level;
            pace.goldLeft = gold;
        }
        if (boss) {
            ++depth;
        }
    }
    return pace;
}

void noteItem(LootTally& tally, const systems::ItemRarity rarity, const bool bossKill) {
    switch (rarity) {
    case systems::ItemRarity::Common:
        ++tally.common;
        break;
    case systems::ItemRarity::Magic:
        ++tally.magic;
        break;
    case systems::ItemRarity::Rare:
        ++tally.rare;
        break;
    case systems::ItemRarity::Legendary:
        ++tally.legendary;
        if (bossKill) {
            ++tally.bossLegendary;
        }
        break;
    case systems::ItemRarity::Unique:
        ++tally.unique;
        break;
    case systems::ItemRarity::Mythical:
        ++tally.combatMythical;
        break;
    }
}

} // namespace

TEST_CASE("Fresh combat reels keep legendary occasional and mythical absent", "[town][loot][balance]") {
    systems::SlotMachineLoot loot(1U);
    loot.setCoinPool(0);
    loot.setPityCounter(0);
    const float bossLegendary = loot.legendaryChance(systems::EntityTier::Boss);
    const float eliteLegendary = loot.legendaryChance(systems::EntityTier::Elite);
    const float standardLegendary = loot.legendaryChance(systems::EntityTier::Standard);
    const float minorLegendary = loot.legendaryChance(systems::EntityTier::Minor);
    // A handful of bosses (about 6) should land a legendary more often than not, without every boss paying one.
    // P(at least one) at 0.14 over 6 kills is about 0.60.
    CHECK(bossLegendary > 0.10F);
    CHECK(bossLegendary < 0.20F);
    CHECK(eliteLegendary > 0.01F);
    CHECK(eliteLegendary < bossLegendary * 0.45F);
    CHECK(standardLegendary < eliteLegendary);
    CHECK(minorLegendary < standardLegendary);
    CHECK(loot.combatMythicalChance() == 0.0F);

    systems::LootEngine weights(3U);
    const systems::CombatRarityWeights boss = weights.combatRarityWeights(systems::EntityTier::Boss);
    const float legacyBossLegendary = boss.dropChance * boss.legendary;
    CHECK(legacyBossLegendary > 0.08F);
    CHECK(legacyBossLegendary < 0.22F);
    CHECK(boss.mythical == 0.0F);
}

TEST_CASE("Tavern mythical bucket stays one in ten thousand and pity ignores it", "[town][loot][balance]") {
    systems::SlotMachineLoot loot(4U);
    const systems::TavernGambleOdds fresh = loot.tavernOdds();
    CHECK(fresh.mythical == Catch::Approx(systems::SlotMachineLoot::kTavernMythicalChance).margin(1.0e-8F));
    CHECK(systems::tavernPrizeForRoll(0.0F, fresh) == systems::TavernPrize::Mythical);
    CHECK(systems::tavernPrizeForRoll(fresh.mythical - 1.0e-6F, fresh) == systems::TavernPrize::Mythical);
    CHECK(systems::tavernPrizeForRoll(fresh.mythical + 1.0e-6F, fresh) == systems::TavernPrize::Unique);
    CHECK(systems::tavernPrizeForRoll(0.999F, fresh) == systems::TavernPrize::Nothing);

    // A lucky few hundred spins can hit (about 3% at 300). Expected wait is thousands, not a pity guarantee.
    const auto atLeastOne = [](const int spins, const float chance) {
        return 1.0 - std::pow(1.0 - static_cast<double>(chance), spins);
    };
    CHECK(atLeastOne(300, fresh.mythical) > 0.025);
    CHECK(atLeastOne(300, fresh.mythical) < 0.040);
    CHECK(atLeastOne(10000, fresh.mythical) > 0.60);
    CHECK(atLeastOne(10000, fresh.mythical) < 0.70);

    loot.setTavernPitySpins(100000);
    const systems::TavernGambleOdds pitied = loot.tavernOdds();
    CHECK(pitied.mythical == Catch::Approx(fresh.mythical).margin(1.0e-8F));
    CHECK(pitied.legendary == Catch::Approx(fresh.legendary + 0.02F).margin(1.0e-4F));
    CHECK(pitied.legendary < pitied.rare);
}

TEST_CASE("Kill gold reaches blacksmith, chapel, then tavern without a stall", "[town][balance]") {
    const systems::TownBuildingDefinition forge = systems::townBuildingDefinition(systems::TownBuilding::Blacksmith);
    const systems::TownBuildingDefinition chapel = systems::townBuildingDefinition(systems::TownBuilding::Healer);
    const systems::TownBuildingDefinition tavern = systems::townBuildingDefinition(systems::TownBuilding::Tavern);
    CHECK(forge.repairGold < chapel.repairGold);
    CHECK(chapel.repairGold < tavern.repairGold);
    CHECK(forge.requiredLevel == 1);
    CHECK(chapel.requiredLevel == 2);
    CHECK(tavern.requiredLevel == 3);

    const int depthOneMob = systems::combatGoldBounty(false, false, 1);
    CHECK(depthOneMob * 12 >= forge.repairGold);
    CHECK(depthOneMob * 4 < forge.repairGold);

    const Pace pace = marchUntilTavern(160);
    std::ostringstream summary;
    summary << "pace blacksmith=" << pace.blacksmithKill << " chapel=" << pace.chapelKill
            << " tavern=" << pace.tavernKill << " level=" << pace.levelAtTavern << " goldLeft=" << pace.goldLeft;
    INFO(summary.str());
    REQUIRE(pace.tavernOpen);
    CHECK(pace.blacksmithKill >= 4);
    CHECK(pace.blacksmithKill <= 12);
    CHECK(pace.chapelKill > pace.blacksmithKill);
    CHECK(pace.chapelKill <= 36);
    CHECK(pace.tavernKill > pace.chapelKill);
    CHECK(pace.tavernKill <= 80);
    CHECK(pace.levelAtTavern >= 3);
    CHECK(pace.levelAtTavern <= 4);
    CHECK(pace.goldLeft >= 0);
    CHECK(pace.goldLeft < 180);
}

TEST_CASE("Seeded plains sim repairs town and drops occasional boss legendaries", "[town][loot][balance]") {
    constexpr int kRuns = 48;
    constexpr int kBossesTracked = 8;
    LootTally tally{};
    int blacksmith = 0;
    int chapel = 0;
    int tavern = 0;

    for (int run = 0; run < kRuns; ++run) {
        systems::SlotMachineLoot loot(1000U + static_cast<std::uint32_t>(run) * 97U);
        systems::TownHub town;
        ui::CharacterScreenData stats{};
        stats.level = 1;
        stats.experienceToNextLevel = systems::experienceRequiredForLevel(1);
        int gold = 0;
        int depth = 1;
        int bossesThisRun = 0;
        bool legendaryThisRun = false;

        for (int kill = 1; kill <= 160 && (bossesThisRun < kBossesTracked || !town.isRepaired(systems::TownBuilding::Tavern));
             ++kill) {
            const int slot = (kill - 1) % 19;
            const bool boss = slot == 18;
            const bool elite = slot == 8 || slot == 17;
            const systems::CombatKillReward reward = systems::combatKillReward(boss, elite, depth);
            gold += reward.gold;
            static_cast<void>(systems::grantCombatExperience(stats, reward.experience));
            loot.setZoneDepth(depth);
            loot.insertCoins(boss ? systems::ActionType::BOSS_KILL : systems::ActionType::MOB_KILL);
            const systems::EntityTier tier =
                boss ? systems::EntityTier::Boss : (elite ? systems::EntityTier::Elite : systems::EntityTier::Standard);
            const systems::LootSpinResult spin = loot.spin(tier);
            const bool countBoss = boss && bossesThisRun < kBossesTracked;
            if (countBoss) {
                ++tally.bosses;
                ++bossesThisRun;
            }
            for (const systems::LootPrize& prize : spin.prizes) {
                if (!prize.item.has_value()) {
                    continue;
                }
                noteItem(tally, prize.item->rarity, countBoss);
                if (countBoss && prize.item->rarity == systems::ItemRarity::Legendary) {
                    legendaryThisRun = true;
                }
            }
            static_cast<void>(town.tryRepair(systems::TownBuilding::Blacksmith, gold, stats.level));
            static_cast<void>(town.tryRepair(systems::TownBuilding::Healer, gold, stats.level));
            static_cast<void>(town.tryRepair(systems::TownBuilding::Tavern, gold, stats.level));
            if (boss) {
                ++depth;
            }
        }
        if (town.isRepaired(systems::TownBuilding::Blacksmith)) {
            ++blacksmith;
        }
        if (town.isRepaired(systems::TownBuilding::Healer)) {
            ++chapel;
        }
        if (town.isRepaired(systems::TownBuilding::Tavern)) {
            ++tavern;
        }
        if (legendaryThisRun) {
            ++tally.runsWithBossLegendary;
        }
    }

    const Pace pace = marchUntilTavern(160);
    const int items = tally.common + tally.magic + tally.rare + tally.legendary + tally.unique;
    const float bossLegendaryRate =
        tally.bosses > 0 ? static_cast<float>(tally.bossLegendary) / static_cast<float>(tally.bosses) : 0.0F;
    const float runHitRate =
        kRuns > 0 ? static_cast<float>(tally.runsWithBossLegendary) / static_cast<float>(kRuns) : 0.0F;
    std::ostringstream summary;
    summary << "runs=" << kRuns << " repaired smith/chapel/tavern " << blacksmith << "/" << chapel << "/" << tavern
            << " paceKill smith=" << pace.blacksmithKill << " chapel=" << pace.chapelKill << " tavern=" << pace.tavernKill
            << " bosses=" << tally.bosses << " bossLegendary=" << tally.bossLegendary
            << " bossLegendaryRate=" << bossLegendaryRate << " runsWithLegendary=" << tally.runsWithBossLegendary
            << " runHitRate=" << runHitRate << " items c/m/r/l/u " << tally.common << "/" << tally.magic << "/"
            << tally.rare << "/" << tally.legendary << "/" << tally.unique << " mythical=" << tally.combatMythical;
    INFO(summary.str());
    std::cout << summary.str() << '\n';

    CHECK(blacksmith == kRuns);
    CHECK(chapel == kRuns);
    CHECK(tavern == kRuns);
    CHECK(pace.tavernKill <= 80);
    CHECK(tally.combatMythical == 0);
    CHECK(tally.bosses == kRuns * kBossesTracked);
    CHECK(bossLegendaryRate > 0.06F);
    CHECK(bossLegendaryRate < 0.40F);
    CHECK(runHitRate > 0.35F);
    CHECK(runHitRate < 0.98F);
    REQUIRE(items > 0);
    const int mundane = tally.common + tally.magic + tally.rare;
    CHECK(static_cast<float>(mundane) / static_cast<float>(items) > 0.75F);
    CHECK(tally.common < (items * 9) / 10);
    CHECK(tally.magic < (items * 9) / 10);
    CHECK(tally.rare < (items * 9) / 10);
    CHECK(tally.legendary + tally.unique < items / 5);
}
