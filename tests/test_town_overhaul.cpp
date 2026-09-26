#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render/TownBackdrop.hpp"
#include "systems/LootEngine.hpp"
#include "systems/SlotMachineLoot.hpp"
#include "systems/TownHub.hpp"
#include "ui/UiHitTest.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiScale.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool slotsSeparated(const ui::Rect& a, const ui::Rect& b) {
    return !ui::rectsOverlap(a, b);
}

void expectRarityCurve(const systems::CombatRarityWeights& weights) {
    CHECK(weights.mythical == 0.0F);
    CHECK(weights.common > weights.magic);
    CHECK(weights.magic > weights.rare);
    CHECK(weights.rare > weights.legendary);
    CHECK(weights.legendary > weights.unique);
    CHECK(weights.unique > weights.mythical);
    const float mix = weights.common + weights.magic + weights.rare + weights.legendary + weights.unique + weights.mythical;
    CHECK(mix == Catch::Approx(1.0F).margin(0.02F));
    CHECK(weights.dropChance < 0.75F);
}

void writeTownBmp(const render::TownPixelBuffer& image, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return;
    }
    const int rowStride = image.width * 3;
    const int padded = (rowStride + 3) & ~3;
    const int pixelBytes = padded * image.height;
    const int fileSize = 54 + pixelBytes;
    unsigned char header[54] = {};
    header[0] = 'B';
    header[1] = 'M';
    header[2] = static_cast<unsigned char>(fileSize);
    header[3] = static_cast<unsigned char>(fileSize >> 8);
    header[4] = static_cast<unsigned char>(fileSize >> 16);
    header[5] = static_cast<unsigned char>(fileSize >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = static_cast<unsigned char>(image.width);
    header[19] = static_cast<unsigned char>(image.width >> 8);
    header[22] = static_cast<unsigned char>(image.height);
    header[23] = static_cast<unsigned char>(image.height >> 8);
    header[26] = 1;
    header[28] = 24;
    out.write(reinterpret_cast<const char*>(header), 54);
    const std::vector<unsigned char> pad(static_cast<std::size_t>(std::max(0, padded - rowStride)), 0);
    for (int y = image.height - 1; y >= 0; --y) {
        for (int x = 0; x < image.width; ++x) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4U;
            const unsigned char bgr[3] = {image.rgba[index + 2U], image.rgba[index + 1U], image.rgba[index]};
            out.write(reinterpret_cast<const char*>(bgr), 3);
        }
        if (!pad.empty()) {
            out.write(reinterpret_cast<const char*>(pad.data()), static_cast<std::streamsize>(pad.size()));
        }
    }
}

} // namespace

TEST_CASE("Combat rarity weights favor common gear and exclude mythical", "[town][loot]") {
    systems::LootEngine loot(7U);
    const systems::CombatRarityWeights minor = loot.combatRarityWeights(systems::EntityTier::Minor);
    const systems::CombatRarityWeights standard = loot.combatRarityWeights(systems::EntityTier::Standard);
    const systems::CombatRarityWeights boss = loot.combatRarityWeights(systems::EntityTier::Boss);
    expectRarityCurve(minor);
    expectRarityCurve(standard);
    expectRarityCurve(boss);
    CHECK(minor.dropChance < standard.dropChance);
    CHECK(standard.dropChance < boss.dropChance);
    CHECK(minor.dropChance < 0.30F);

    const systems::RarityColor legendary = systems::rarityColor(systems::ItemRarity::Legendary);
    CHECK(legendary.red > legendary.green);
    CHECK(legendary.green > legendary.blue);
    CHECK(legendary.red < 0.75F);
    const systems::RarityColor mythical = systems::rarityColor(systems::ItemRarity::Mythical);
    CHECK(mythical.red > 0.7F);
    CHECK(mythical.blue > 0.6F);
    CHECK(mythical.green < 0.4F);
    CHECK(std::string(systems::rarityLabel(systems::ItemRarity::Magic)) == "Magic");
    CHECK(std::string(systems::rarityLabel(systems::ItemRarity::Mythical)) == "Mythical");
}

TEST_CASE("Town repairs spend gold and unlock buildings in level order", "[town]") {
    systems::TownHub town;
    CHECK_FALSE(town.isRepaired(systems::TownBuilding::Blacksmith));
    CHECK_FALSE(town.isRepaired(systems::TownBuilding::Healer));
    CHECK_FALSE(town.isRepaired(systems::TownBuilding::Tavern));

    int gold = 200;
    CHECK(town.tryRepair(systems::TownBuilding::Tavern, gold, 1) == systems::TownRepairResult::NeedLevel);
    CHECK(gold == 200);
    CHECK(town.tryRepair(systems::TownBuilding::Healer, gold, 1) == systems::TownRepairResult::NeedLevel);

    CHECK(town.tryRepair(systems::TownBuilding::Blacksmith, gold, 1) == systems::TownRepairResult::Repaired);
    CHECK(gold == 160);
    CHECK(town.isRepaired(systems::TownBuilding::Blacksmith));
    CHECK(town.tryRepair(systems::TownBuilding::Blacksmith, gold, 5) == systems::TownRepairResult::AlreadyOpen);

    CHECK(town.tryRepair(systems::TownBuilding::Healer, gold, 2) == systems::TownRepairResult::Repaired);
    CHECK(town.isRepaired(systems::TownBuilding::Healer));
    CHECK_FALSE(town.isRepaired(systems::TownBuilding::Tavern));

    int poor = 10;
    systems::TownHub second;
    second.applyRepairMask(town.repairMask());
    CHECK(second.isRepaired(systems::TownBuilding::Blacksmith));
    CHECK(second.tryRepair(systems::TownBuilding::Tavern, poor, 3) == systems::TownRepairResult::NeedGold);

    int rich = 200;
    CHECK(second.tryRepair(systems::TownBuilding::Tavern, rich, 3) == systems::TownRepairResult::Repaired);
    CHECK(second.isRepaired(systems::TownBuilding::Tavern));
    CHECK(systems::combatGoldBounty(false, false, 1) < systems::combatGoldBounty(true, false, 1));
    CHECK(systems::healerTitheGold() > 0);
    CHECK(systems::healerTitheGold() < systems::townBuildingDefinition(systems::TownBuilding::Healer).repairGold);
}

TEST_CASE("Tavern gamble keeps mythical odds near one in ten thousand", "[town][loot]") {
    systems::SlotMachineLoot loot(99U);
    const systems::TavernGambleOdds odds = loot.tavernOdds();
    const float total = odds.nothing + odds.gold + odds.common + odds.magic + odds.rare + odds.legendary + odds.unique +
                        odds.mythical;
    CHECK(total == Catch::Approx(1.0F).margin(0.002F));
    CHECK(odds.mythical == Catch::Approx(systems::SlotMachineLoot::kTavernMythicalChance).margin(1.0e-8F));
    CHECK(odds.mythical < odds.unique);
    CHECK(odds.unique < odds.legendary);
    CHECK(odds.legendary < odds.rare);
    CHECK(odds.rare < odds.magic);
    CHECK(odds.magic < odds.common);
    CHECK(loot.combatMythicalChance() == 0.0F);

    constexpr int kSpins = 20000;
    int gold = systems::SlotMachineLoot::kTavernSpinCost * kSpins;
    int mythical = 0;
    int paid = 0;
    for (int spin = 0; spin < kSpins; ++spin) {
        const systems::TavernGambleResult result = loot.gambleTavern(gold);
        if (!result.paid) {
            break;
        }
        ++paid;
        if (result.grantedItem && result.rarity == systems::ItemRarity::Mythical) {
            ++mythical;
        }
    }
    REQUIRE(paid == kSpins);
    CHECK(mythical <= 16);
    CHECK(static_cast<float>(mythical) / static_cast<float>(kSpins) < 0.0008F);
    CHECK(loot.tavernOdds().mythical == Catch::Approx(systems::SlotMachineLoot::kTavernMythicalChance).margin(1.0e-8F));
}

TEST_CASE("Town hotspots and inventory slots do not overlap", "[town][ui]") {
    const ui::UiScale desktop(1280, 720, ui::UiPlatformKind::Desktop);
    const ui::TownSceneLayout town = ui::computeTownSceneLayout(desktop);
    CHECK_FALSE(ui::rectsOverlap(town.blacksmith, town.healer));
    CHECK_FALSE(ui::rectsOverlap(town.blacksmith, town.tavern));
    CHECK_FALSE(ui::rectsOverlap(town.healer, town.tavern));
    CHECK_FALSE(ui::rectsOverlap(town.road, town.blacksmith));
    CHECK_FALSE(ui::rectsOverlap(town.road, town.healer));
    CHECK_FALSE(ui::rectsOverlap(town.road, town.tavern));
    CHECK(town.blacksmith.width >= 48.0F);
    CHECK(town.servicePanel.contains(
        town.serviceAction.x + town.serviceAction.width * 0.5F,
        town.serviceAction.y + town.serviceAction.height * 0.5F));
    CHECK(town.servicePanel.contains(
        town.serviceClose.x + town.serviceClose.width * 0.5F, town.serviceClose.y + town.serviceClose.height * 0.5F));
    CHECK_FALSE(ui::rectsOverlap(town.serviceAction, town.serviceClose));

    const ui::UiScale mobile(390, 844, ui::UiPlatformKind::Mobile);
    CHECK(mobile.platform == ui::UiPlatformKind::Mobile);
    CHECK(mobile.touchBoost > 1.0F);
    const ui::TownSceneLayout phoneTown = ui::computeTownSceneLayout(mobile);
    CHECK_FALSE(ui::rectsOverlap(phoneTown.blacksmith, phoneTown.healer));
    CHECK_FALSE(ui::rectsOverlap(phoneTown.healer, phoneTown.tavern));
    CHECK_FALSE(ui::rectsOverlap(phoneTown.road, phoneTown.healer));
    CHECK(phoneTown.blacksmith.width >= 44.0F);
    CHECK(phoneTown.road.height >= 36.0F);

    const ui::UiScale browser(1280, 720, ui::UiPlatformKind::Browser);
    CHECK(browser.platform == ui::UiPlatformKind::Browser);
    const ui::UiScale narrowBrowser(800, 600, ui::UiPlatformKind::Browser);
    CHECK(narrowBrowser.platform == ui::UiPlatformKind::Mobile);

    const ui::InventoryPaperDollLayout phoneBag = ui::computeInventoryPaperDollLayout(mobile, 6, 4);
    CHECK(phoneBag.panel.width <= static_cast<float>(mobile.width) + 1.0F);
    CHECK(phoneBag.slotSize >= 32.0F);
    for (int slot = 0; slot < 15; ++slot) {
        const ui::Rect equip = phoneBag.equipmentSlotRect(slot);
        CHECK(slotsSeparated(equip, phoneBag.portrait));
        CHECK(phoneBag.panel.contains(equip.x + equip.width * 0.5F, equip.y + equip.height * 0.5F));
        for (int other = slot + 1; other < 15; ++other) {
            CHECK(slotsSeparated(equip, phoneBag.equipmentSlotRect(other)));
        }
    }

    const ui::InventoryPaperDollLayout desktopBag = ui::computeInventoryPaperDollLayout(desktop, 6, 4);
    for (int slot = 0; slot < 15; ++slot) {
        CHECK(slotsSeparated(desktopBag.equipmentSlotRect(slot), desktopBag.portrait));
    }
    const int bagCount = desktopBag.bagColumns * desktopBag.bagRows;
    for (int index = 0; index < bagCount; ++index) {
        const ui::Rect bagSlot = desktopBag.inventorySlotRect(index);
        for (int equip = 0; equip < 15; ++equip) {
            CHECK(slotsSeparated(bagSlot, desktopBag.equipmentSlotRect(equip)));
        }
        for (int other = index + 1; other < bagCount; ++other) {
            CHECK(slotsSeparated(bagSlot, desktopBag.inventorySlotRect(other)));
        }
    }
}

TEST_CASE("Procedural town backdrop paints distinct original color", "[town]") {
    const render::TownPixelBuffer image = render::paintTownBackdrop(160, 90);
    REQUIRE(image.width == 160);
    REQUIRE(static_cast<int>(image.rgba.size()) == 160 * 90 * 4);
    int lit = 0;
    int dark = 0;
    for (std::size_t index = 0; index + 3 < image.rgba.size(); index += 4) {
        const int sum = image.rgba[index] + image.rgba[index + 1] + image.rgba[index + 2];
        if (sum > 180) {
            ++lit;
        }
        if (sum < 80) {
            ++dark;
        }
    }
    CHECK(lit > 20);
    CHECK(dark > 100);
    writeTownBmp(image, "/tmp/town_backdrop.bmp");
}
