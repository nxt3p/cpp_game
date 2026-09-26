#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "gameplay/ZoneManager.hpp"
#include "render/TownBackdrop.hpp"
#include "render/TownSilhouette.hpp"
#include "render/TownStages.hpp"
#include "systems/LootEngine.hpp"
#include "systems/SlotMachineLoot.hpp"
#include "systems/TownHub.hpp"
#include "ui/HudConsoleLayout.hpp"
#include "ui/UiHitTest.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiScale.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
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
    CHECK(gold == 164);
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
    CHECK(town.blacksmith.x + town.blacksmith.width < town.healer.x);
    CHECK(town.healer.y + 4.0F < town.blacksmith.y);
    CHECK(town.tavern.x > town.healer.x + town.healer.width - 1.0F);
    CHECK(town.road.x > town.tavern.x);
    CHECK(town.road.x + town.road.width > static_cast<float>(desktop.width) * 0.8F);
    const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(desktop);
    for (const ui::Rect& hotspot : {town.blacksmith, town.healer, town.tavern, town.road}) {
        CHECK(hotspot.y + hotspot.height <= console.panel.y + 0.5F);
    }
    CHECK(town.exitButton.x + town.exitButton.width <= static_cast<float>(desktop.width));
    CHECK(town.exitButton.y + town.exitButton.height <= town.road.y + 0.5F);
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
    CHECK(phoneTown.road.x > phoneTown.tavern.x);
    const ui::HudConsoleLayout phoneConsole = ui::computeHudConsoleLayout(mobile);
    CHECK(phoneTown.blacksmith.y + phoneTown.blacksmith.height <= phoneConsole.panel.y + 0.5F);
    CHECK(phoneTown.road.y + phoneTown.road.height <= phoneConsole.panel.y + 0.5F);

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

TEST_CASE("Town hides the circular minimap until the hero is on the road", "[town][ui]") {
    gameplay::ZoneManager zones;
    CHECK(zones.activeZone() == gameplay::WorldZone::TOWN);
    CHECK_FALSE(zones.showsMinimap());
    CHECK_FALSE(gameplay::minimapVisible(true, false));
    CHECK(gameplay::minimapVisible(true, true));
    CHECK(gameplay::minimapVisible(false, false));

    const gameplay::ZoneTransitionResult transition = zones.updatePlayerPosition(gameplay::Vec3{0.0F, 0.0F, 40.0F});
    CHECK(transition.transitioned);
    CHECK(zones.activeZone() == gameplay::WorldZone::PLAINS);
    CHECK(zones.showsMinimap());
    CHECK(gameplay::minimapVisible(!zones.showsMinimap(), false));
}

TEST_CASE("Town scene loads committed plates instead of a missing folder", "[town]") {
    const std::filesystem::path root{ENGINE_ASSETS_DIR};
    const char* files[] = {
        "backdrop.png",
        "forge_ruined.png",
        "forge_repaired.png",
        "chapel_ruined.png",
        "chapel_repaired.png",
        "tavern_ruined.png",
        "tavern_repaired.png",
        "adventure.png"};
    for (const char* file : files) {
        const std::filesystem::path path = root / "textures" / "town" / file;
        INFO(path.string());
        REQUIRE(std::filesystem::exists(path));
        CHECK(std::filesystem::file_size(path) > 500);
    }
}

TEST_CASE("Town captions sit under the building art", "[town][ui]") {
    const ui::UiScale desktop(1280, 720, ui::UiPlatformKind::Desktop);
    const ui::TownSceneLayout town = ui::computeTownSceneLayout(desktop);
    for (const ui::Rect& hotspot : {town.blacksmith, town.healer, town.tavern}) {
        const ui::Rect art = ui::townBuildingArtRect(hotspot);
        const ui::Rect caption = ui::townBuildingCaptionRect(hotspot);
        CHECK_FALSE(ui::rectsOverlap(art, caption));
        CHECK(hotspot.contains(caption.x + caption.width * 0.5F, caption.y + caption.height * 0.5F));
        CHECK(art.height > caption.height);
        CHECK(caption.height >= 56.0F);
    }
    CHECK(town.notice.height >= 36.0F);
    CHECK(ui::townHotspotIndexAt(town, town.blacksmith.x + 4.0F, town.blacksmith.y + 4.0F) == 0);
    CHECK(ui::townHotspotIndexAt(town, town.tavern.x + town.tavern.width * 0.5F, town.tavern.y + 8.0F) == 1);
    CHECK(ui::townHotspotIndexAt(town, town.healer.x + 8.0F, town.healer.y + town.healer.height * 0.5F) == 2);
    CHECK(ui::townHotspotIndexAt(town, town.road.x + town.road.width * 0.5F, town.road.y + town.road.height * 0.5F) == 3);
    CHECK(ui::townHotspotIndexAt(town, 2.0F, 2.0F) == -1);

    const ui::Rect art = ui::townBuildingArtRect(town.healer);
    const ui::Rect tight = ui::townOpaqueSpriteRect(art, 768.0F, 1024.0F, 0.25F, 0.2F, 0.7F, 0.85F);
    CHECK(art.contains(tight.x + tight.width * 0.5F, tight.y + tight.height * 0.5F));
    CHECK(tight.width < art.width);
    CHECK(tight.y + tight.height == Catch::Approx(art.y + art.height).margin(0.6F));
    CHECK_FALSE(tight.contains(art.x + 1.0F, art.y + 1.0F));
}

extern "C" {
unsigned char* stbi_load(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels);
void stbi_image_free(void* retval_from_stbi_load);
void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
}

TEST_CASE("Town plates keep a transparent margin around the sprite", "[town]") {
    stbi_set_flip_vertically_on_load(0);
    const std::filesystem::path root{ENGINE_ASSETS_DIR};
    const char* plates[] = {
        "forge_ruined.png",
        "forge_repaired.png",
        "chapel_ruined.png",
        "chapel_repaired.png",
        "tavern_ruined.png",
        "tavern_repaired.png",
        "adventure.png"};
    for (const char* file : plates) {
        const std::filesystem::path path = root / "textures" / "town" / file;
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        INFO(path.string());
        REQUIRE(pixels != nullptr);
        REQUIRE(width > 8);
        REQUIRE(height > 8);
        const auto alphaAt = [&](const int x, const int y) {
            return pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U + 3U];
        };
        CHECK(alphaAt(0, 0) == 0);
        CHECK(alphaAt(width - 1, 0) == 0);
        CHECK(alphaAt(0, height - 1) == 0);
        CHECK(alphaAt(width - 1, height - 1) == 0);
        int opaque = 0;
        const int step = 4;
        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step) {
                if (alphaAt(x, y) >= 48) {
                    ++opaque;
                }
            }
        }
        const int samples = ((width + step - 1) / step) * ((height + step - 1) / step);
        CHECK(opaque > 200);
        CHECK(opaque * 2 < samples);
        stbi_image_free(pixels);
    }

    int backdropW = 0;
    int backdropH = 0;
    int backdropChannels = 0;
    const std::filesystem::path backdrop = root / "textures" / "town" / "backdrop.png";
    unsigned char* backdropPixels = stbi_load(backdrop.string().c_str(), &backdropW, &backdropH, &backdropChannels, 4);
    REQUIRE(backdropPixels != nullptr);
    CHECK(backdropW >= 640);
    CHECK(backdropH >= 360);
    CHECK(backdropPixels[3] > 200);
    int stone = 0;
    int looked = 0;
    for (int y = backdropH / 10; y < (backdropH * 45) / 100; y += 4) {
        for (int x = 0; x < (backdropW * 28) / 100; x += 4) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(backdropW) + static_cast<std::size_t>(x)) * 4U;
            const int red = backdropPixels[index];
            const int green = backdropPixels[index + 1U];
            const int blue = backdropPixels[index + 2U];
            const int lum = (red + green + blue) / 3;
            const int sat = std::max(red, std::max(green, blue)) - std::min(red, std::min(green, blue));
            ++looked;
            if (sat < 18 && lum > 70 && lum < 180) {
                ++stone;
            }
        }
    }
    CHECK(looked > 40);
    CHECK(stone * 5 < looked);
    const auto backdropAt = [&](const int x, const int y) {
        const std::size_t index =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(backdropW) + static_cast<std::size_t>(x)) * 4U;
        return std::tuple{backdropPixels[index], backdropPixels[index + 1U], backdropPixels[index + 2U]};
    };
    const auto [padR, padG, padB] = backdropAt((backdropW * 15) / 100, (backdropH * 66) / 100);
    CHECK(padR > padB + 20);
    CHECK(padR > 80);
    stbi_image_free(backdropPixels);

    const auto checksum = [&](const char* file) {
        const std::filesystem::path path = root / "textures" / "town" / file;
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        REQUIRE(pixels != nullptr);
        unsigned sum = 0;
        const int count = width * height;
        for (int index = 0; index < count; index += 8) {
            sum += pixels[static_cast<std::size_t>(index) * 4U];
            sum += pixels[static_cast<std::size_t>(index) * 4U + 3U];
        }
        stbi_image_free(pixels);
        return sum;
    };
    CHECK(checksum("forge_ruined.png") != checksum("forge_repaired.png"));
    CHECK(checksum("chapel_ruined.png") != checksum("chapel_repaired.png"));
    CHECK(checksum("tavern_ruined.png") != checksum("tavern_repaired.png"));
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
    CHECK(lit > 40);
    CHECK(dark < lit);

    const auto sample = [&](const int x, const int y) {
        const std::size_t index =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4U;
        return std::tuple{image.rgba[index], image.rgba[index + 1U], image.rgba[index + 2U]};
    };
    const auto [skyR, skyG, skyB] = sample(8, 4);
    CHECK(skyB > skyR);
    const auto [groundR, groundG, groundB] = sample(image.width / 2, image.height - 4);
    CHECK(groundG + groundR > groundB);
    const auto [padR, padG, padB] = sample((image.width * 15) / 100, (image.height * 66) / 100);
    CHECK(padR > padB + 15);
    const int lampX = (image.width * 22) / 100;
    const int lampY = (image.height * 74) / 100;
    const auto [lampR, lampG, lampB] = sample(lampX, lampY);
    const bool lantern = lampR > 220 && lampG > 160 && lampB < 140;
    CHECK_FALSE(lantern);

    const auto warmPixels = [](const render::TownPixelBuffer& plate) {
        int warm = 0;
        int visible = 0;
        for (std::size_t index = 0; index + 3 < plate.rgba.size(); index += 4) {
            if (plate.rgba[index + 3U] < 40) {
                continue;
            }
            ++visible;
            const int red = plate.rgba[index];
            const int blue = plate.rgba[index + 2U];
            if (red > 150 && red > blue + 30) {
                ++warm;
            }
        }
        return std::pair{warm, visible};
    };
    const render::TownPixelBuffer ruinedForge = render::paintTownPlate(render::TownPlateKind::Forge, false, 80, 110);
    const render::TownPixelBuffer openForge = render::paintTownPlate(render::TownPlateKind::Forge, true, 80, 110);
    const render::TownPixelBuffer ruinedChapel = render::paintTownPlate(render::TownPlateKind::Chapel, false, 80, 110);
    const render::TownPixelBuffer openChapel = render::paintTownPlate(render::TownPlateKind::Chapel, true, 80, 110);
    const auto [ruinedWarm, ruinedVisible] = warmPixels(ruinedForge);
    const auto [openWarm, openVisible] = warmPixels(openForge);
    CHECK(openVisible > 80);
    CHECK(ruinedVisible > 40);
    CHECK(openWarm > ruinedWarm);
    CHECK(warmPixels(openChapel).first > warmPixels(ruinedChapel).first);
    const render::TownPixelBuffer road = render::paintTownPlate(render::TownPlateKind::Road, true, 120, 36);
    CHECK(warmPixels(road).second > 40);
    writeTownBmp(image, "/tmp/town_backdrop.bmp");
    writeTownBmp(ruinedForge, "/tmp/town_forge_ruined.bmp");
    writeTownBmp(openForge, "/tmp/town_forge_open.bmp");
    writeTownBmp(ruinedChapel, "/tmp/town_chapel_ruined.bmp");
    writeTownBmp(openChapel, "/tmp/town_chapel_open.bmp");
    writeTownBmp(render::paintTownPlate(render::TownPlateKind::Tavern, false, 80, 110), "/tmp/town_tavern_ruined.bmp");
    writeTownBmp(render::paintTownPlate(render::TownPlateKind::Tavern, true, 80, 110), "/tmp/town_tavern_open.bmp");
}

TEST_CASE("Town art stages map repair onto restored and fall back when a file is missing", "[town]") {
    static_assert(static_cast<int>(systems::TownBuilding::Blacksmith) == 0);
    static_assert(static_cast<int>(systems::TownBuilding::Tavern) == 1);
    static_assert(static_cast<int>(systems::TownBuilding::Healer) == 2);
    CHECK(render::townArtStage(false) == static_cast<int>(render::TownArtStage::Ruined));
    CHECK(render::townArtStage(true) == static_cast<int>(render::TownArtStage::Restored));
    const bool ruinedOnly[render::kTownArtStageCount] = {true, false, false, false};
    CHECK(render::townResolvedArtStage(static_cast<int>(render::TownArtStage::Restored), ruinedOnly) ==
          static_cast<int>(render::TownArtStage::Ruined));
    const bool shipped[render::kTownArtStageCount] = {true, false, true, false};
    CHECK(render::townResolvedArtStage(static_cast<int>(render::TownArtStage::Restored), shipped) ==
          static_cast<int>(render::TownArtStage::Restored));
    CHECK(render::townResolvedArtStage(static_cast<int>(render::TownArtStage::Ruined), shipped) ==
          static_cast<int>(render::TownArtStage::Ruined));
    const bool patched[render::kTownArtStageCount] = {true, true, false, false};
    CHECK(render::townResolvedArtStage(static_cast<int>(render::TownArtStage::Restored), patched) ==
          static_cast<int>(render::TownArtStage::Patched));
    const bool upgraded[render::kTownArtStageCount] = {true, true, true, true};
    CHECK(render::townResolvedArtStage(static_cast<int>(render::TownArtStage::Upgraded), upgraded) ==
          static_cast<int>(render::TownArtStage::Upgraded));
    CHECK(std::string(render::townStageFile(0, static_cast<int>(render::TownArtStage::Restored))) == "forge_repaired.png");
    CHECK(std::string(render::townStageFile(2, static_cast<int>(render::TownArtStage::Upgraded))) == "chapel_upgraded.png");
    CHECK(std::string(render::townStageFile(1, static_cast<int>(render::TownArtStage::Patched))) == "tavern_patched.png");
    CHECK(render::townStageRequired(static_cast<int>(render::TownArtStage::Ruined)));
    CHECK(render::townStageRequired(static_cast<int>(render::TownArtStage::Restored)));
    CHECK_FALSE(render::townStageRequired(static_cast<int>(render::TownArtStage::Patched)));
    CHECK_FALSE(render::townStageRequired(static_cast<int>(render::TownArtStage::Upgraded)));
}

namespace {

[[nodiscard]] float contourDistance2(const std::vector<float>& contour, const float x, const float y) {
    const std::size_t count = contour.size() / 2U;
    float best = 1.0e12F;
    if (count == 0U) {
        return best;
    }
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t next = (index + 1U) % count;
        const float ax = contour[index * 2U];
        const float ay = contour[index * 2U + 1U];
        const float bx = contour[next * 2U];
        const float by = contour[next * 2U + 1U];
        const float abx = bx - ax;
        const float aby = by - ay;
        const float apx = x - ax;
        const float apy = y - ay;
        const float ab2 = abx * abx + aby * aby;
        float t = 0.0F;
        if (ab2 > 1.0e-8F) {
            t = std::clamp((apx * abx + apy * aby) / ab2, 0.0F, 1.0F);
        }
        const float dx = x - (ax + abx * t);
        const float dy = y - (ay + aby * t);
        best = std::min(best, dx * dx + dy * dy);
    }
    return best;
}

} // namespace

TEST_CASE("Town hover outline follows the silhouette and ignores empty canvas", "[town]") {
    constexpr int kSize = 40;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSize * kSize * 4), 0);
    const auto plot = [&](const int x, const int y) {
        const std::size_t index = (static_cast<std::size_t>(y) * kSize + static_cast<std::size_t>(x)) * 4U;
        pixels[index] = 180;
        pixels[index + 1U] = 80;
        pixels[index + 2U] = 40;
        pixels[index + 3U] = 255;
    };
    for (int y = 4; y <= 30; ++y) {
        const float along = static_cast<float>(y - 4) / 26.0F;
        const int half = static_cast<int>(std::lround(along * 14.0F));
        for (int x = 20 - half; x <= 20 + half; ++x) {
            plot(x, y);
        }
    }
    plot(0, 0);
    plot(1, 0);
    plot(0, 1);
    plot(1, 1);

    const render::TownSilhouette outline = render::buildTownSilhouette(pixels.data(), kSize, kSize, 1.75F);
    const std::size_t points = outline.contour.size() / 2U;
    CHECK(points >= 3U);
    float top = 1000.0F;
    float topX = 0.0F;
    float bandMinX = 1000.0F;
    float bandMaxX = -1.0F;
    for (std::size_t index = 0; index < points; ++index) {
        const float x = outline.contour[index * 2U];
        const float y = outline.contour[index * 2U + 1U];
        if (y < top) {
            top = y;
            topX = x;
        }
        if (y < 10.0F) {
            bandMinX = std::min(bandMinX, x);
            bandMaxX = std::max(bandMaxX, x);
        }
    }
    CHECK(top < 8.0F);
    CHECK(topX > 16.0F);
    CHECK(topX < 24.0F);
    CHECK(bandMaxX - bandMinX < 12.0F);
    CHECK(contourDistance2(outline.contour, 6.0F, 4.0F) > 36.0F);
    CHECK(contourDistance2(outline.contour, 0.5F, 0.5F) > 9.0F);
    CHECK(render::townSpriteOpaqueAt(
        outline.alpha.data(), kSize, kSize, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 40.0F, 40.0F, 20.5F, 16.5F));
    CHECK(render::townSpriteOpaqueAt(
        outline.alpha.data(), kSize, kSize, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 40.0F, 40.0F, 0.5F, 0.5F));
    CHECK_FALSE(render::townSpriteOpaqueAt(
        outline.alpha.data(), kSize, kSize, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 40.0F, 40.0F, 8.5F, 6.5F));
    CHECK_FALSE(render::townSpriteOpaqueAt(
        outline.alpha.data(), kSize, kSize, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 40.0F, 40.0F, 39.5F, 1.0F));
}

TEST_CASE("Town plate contours track the roof instead of the sprite box", "[town]") {
    stbi_set_flip_vertically_on_load(0);
    const auto expectPeakedContour = [](const char* file, const float topBand, const float spanLimit, const float cornerGap) {
        const std::filesystem::path path =
            std::filesystem::path{ENGINE_ASSETS_DIR} / "textures" / "town" / file;
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        REQUIRE(pixels != nullptr);
        const render::TownSilhouette silhouette = render::buildTownSilhouette(pixels, width, height);
        int minX = width;
        int minY = height;
        int maxX = -1;
        int maxY = -1;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U +
                        3U] < render::kTownOpaqueAlpha) {
                    continue;
                }
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
        stbi_image_free(pixels);
        const std::size_t points = silhouette.contour.size() / 2U;
        CHECK(points > 12U);
        const float boxW = static_cast<float>(maxX - minX + 1);
        float contourTop = 100000.0F;
        float bandMinX = 100000.0F;
        float bandMaxX = -1.0F;
        for (std::size_t index = 0; index < points; ++index) {
            const float x = silhouette.contour[index * 2U];
            const float y = silhouette.contour[index * 2U + 1U];
            contourTop = std::min(contourTop, y);
            if (y < static_cast<float>(minY) + topBand) {
                bandMinX = std::min(bandMinX, x);
                bandMaxX = std::max(bandMaxX, x);
            }
        }
        CHECK(contourTop < static_cast<float>(minY) + 1.6F);
        CHECK(bandMaxX > bandMinX);
        CHECK(bandMaxX - bandMinX < boxW * spanLimit);
        const float leftCorner = contourDistance2(
            silhouette.contour, static_cast<float>(minX) + 0.5F, static_cast<float>(minY) + 0.5F);
        const float rightCorner = contourDistance2(
            silhouette.contour, static_cast<float>(maxX) + 0.5F, static_cast<float>(minY) + 0.5F);
        CHECK(leftCorner > cornerGap * cornerGap);
        CHECK(rightCorner > cornerGap * cornerGap);
    };
    expectPeakedContour("chapel_ruined.png", 36.0F, 0.35F, 40.0F);
    expectPeakedContour("forge_ruined.png", 28.0F, 0.35F, 24.0F);
}
