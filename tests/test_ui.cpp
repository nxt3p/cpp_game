#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "ui/GlobeFill.hpp"
#include "ui/HudConsoleLayout.hpp"
#include "ui/LootPresentation.hpp"
#include "ui/MinimapSystem.hpp"
#include "ui/UiInteraction.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiScale.hpp"
#include "ui/UiTextLayout.hpp"

namespace {

ui::ScreenAnchor mirrorAnchor() noexcept {
    return ui::ScreenAnchor::TopRight;
}

} // namespace

TEST_CASE("UiScale maps virtual coordinates across viewport sizes", "[ui][scale]") {
    const ui::UiScale hd(1920, 1080);
    CHECK(hd.scaleX == Catch::Approx(1920.0F / 1280.0F).margin(1e-4F));
    CHECK(hd.scaleY == Catch::Approx(1080.0F / 720.0F).margin(1e-4F));
    CHECK(hd.x(100.0F) == Catch::Approx(150.0F).margin(1e-3F));
    CHECK(hd.fractionX(0.5F) == Catch::Approx(960.0F).margin(1e-3F));

    const ui::UiScale qhd(2560, 1440);
    CHECK(qhd.uniform == Catch::Approx(std::min(2560.0F / 1280.0F, 1440.0F / 720.0F)).margin(1e-4F));
    CHECK(qhd.dim(52.0F) == Catch::Approx(52.0F * qhd.uniform).margin(1e-3F));
}

TEST_CASE("Character panel layout reserves non-overlapping level up buttons", "[ui][layout]") {
    const ui::UiScale scale(1280, 720);
    const ui::CharacterPanelLayout layout = ui::computeCharacterPanelLayout(scale);

    CHECK(layout.panel.contains(
        layout.upgradeStrengthButton.x + layout.upgradeStrengthButton.width * 0.5F,
        layout.upgradeStrengthButton.y + layout.upgradeStrengthButton.height * 0.5F));
    CHECK(layout.upgradeDexterityButton.x > layout.upgradeStrengthButton.x);
    CHECK(layout.upgradeVitalityButton.x > layout.upgradeDexterityButton.x);
    CHECK(layout.statsText.y >= layout.upgradeStrengthButton.y + layout.upgradeStrengthButton.height);
    CHECK(layout.footerHint.y > layout.statsText.y);
    CHECK(layout.titleBand.y + layout.titleBand.height <= layout.portrait.y);
    CHECK(layout.spellsPane.width > 0.0F);
    CHECK(layout.talentsPane.x >= layout.spellsPane.x + layout.spellsPane.width - 1.0F);
    CHECK(layout.talentsPane.contains(
        layout.upgradeStrengthButton.x + layout.upgradeStrengthButton.width * 0.5F,
        layout.upgradeStrengthButton.y + layout.upgradeStrengthButton.height * 0.5F));

    const ui::AbilityBoardLayout board = ui::computeAbilityBoardLayout(layout, scale);
    CHECK(board.basic.iconCount == 3);
    CHECK(board.strong.iconCount == 2);
    CHECK(board.specialties.iconCount == 3);
    CHECK(board.specialties.header.y > board.basic.icons[0].y);
    CHECK(layout.spellsPane.contains(
        board.basic.icons[0].x + board.basic.icons[0].width * 0.5F,
        board.basic.icons[0].y + board.basic.icons[0].height * 0.5F));
}

TEST_CASE("Paper doll inventory layout exposes fifteen equipment slots", "[ui][layout]") {
    const ui::UiScale scale(1280, 720);
    const ui::InventoryPaperDollLayout layout = ui::computeInventoryPaperDollLayout(scale, 6, 4);

    CHECK(layout.panel.width > 0.0F);
    CHECK(layout.portrait.width > 0.0F);
    CHECK(layout.portrait.height > layout.portrait.width);
    CHECK(layout.statsSidebar.width > 0.0F);
    CHECK(layout.statsSidebar.x > layout.panel.x);
    CHECK(layout.hpBar.width > 0.0F);
    CHECK(layout.xpBar.width > 0.0F);
    CHECK(layout.bagHeader.width > 0.0F);

    for (int slot = 0; slot < 15; ++slot) {
        const ui::Rect bounds = layout.equipmentSlotRect(slot);
        CHECK(bounds.width > 0.0F);
        CHECK(bounds.height > 0.0F);
        CHECK(layout.panel.contains(bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F));
    }

    const ui::Rect bagSlot = layout.inventorySlotRect(0);
    CHECK(bagSlot.y > layout.portrait.y + layout.portrait.height);

    const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(scale);
    CHECK(layout.panel.y >= console.messageStrip.y + console.messageStrip.height - 0.5F);
    CHECK(layout.panel.y + layout.panel.height <= console.panel.y + 0.5F);
    const ui::Rect lastBag = layout.inventorySlotRect(layout.bagColumns * layout.bagRows - 1);
    CHECK(lastBag.y + lastBag.height <= console.panel.y);

    for (const auto& [width, height] : {std::pair{1920, 1080}, std::pair{3840, 2160}}) {
        const ui::UiScale sized(width, height);
        const ui::InventoryPaperDollLayout sizedLayout = ui::computeInventoryPaperDollLayout(sized, 6, 4);
        const ui::HudConsoleLayout sizedConsole = ui::computeHudConsoleLayout(sized);
        CHECK(sizedLayout.panel.y >= 0.0F);
        CHECK(sizedLayout.panel.y + sizedLayout.panel.height <= sizedConsole.panel.y + 0.5F);
        const ui::CharacterPanelLayout character = ui::computeCharacterPanelLayout(sized);
        CHECK(character.panel.y + character.panel.height <= sizedConsole.panel.y + 0.5F);
    }
}

TEST_CASE("Inventory grid recenters and scales slot matrix", "[ui][layout]") {
    const ui::UiScale scale(1280, 720);
    const ui::InventoryGridLayout layout = ui::computeInventoryGridLayout(scale, 6, 4);

    CHECK(layout.panelWidth == Catch::Approx(6.0F * layout.slotSize + layout.padding * 2.0F).margin(1e-3F));
    CHECK(layout.panelX == Catch::Approx(640.0F - layout.panelWidth * 0.5F).margin(1e-3F));

    const ui::Rect first = layout.inventorySlotRect(0);
    const ui::Rect second = layout.inventorySlotRect(1);
    const ui::Rect seventh = layout.inventorySlotRect(6);
    CHECK(second.x > first.x);
    CHECK(seventh.y > first.y);

    const ui::UiScale wide(1920, 1080);
    const ui::InventoryGridLayout wideLayout = ui::computeInventoryGridLayout(wide, 6, 4);
    CHECK(wideLayout.slotSize > layout.slotSize);
    CHECK(wideLayout.panelX == Catch::Approx(960.0F - wideLayout.panelWidth * 0.5F).margin(1e-2F));
}

TEST_CASE("Text truncation applies ellipsis inside bounds", "[ui][text]") {
    const std::string source = "Scout Charm of the Northern Expanse";
    const float scale = 2.0F;
    const float maxWidth = ui::estimateTextWidth("Scout Charm", scale);

    const std::string clipped = ui::truncateWithEllipsis(source, maxWidth, scale);
    REQUIRE(!clipped.empty());
    CHECK(clipped.size() >= 3);
    CHECK(clipped.substr(clipped.size() - 3) == "...");
    CHECK(ui::estimateTextWidth(clipped.c_str(), scale) <= maxWidth + 0.5F);

    const std::string shortText = "HP";
    const float shortWidth = ui::estimateTextWidth(shortText.c_str(), scale) + 4.0F;
    CHECK(ui::truncateWithEllipsis(shortText, shortWidth, scale) == shortText);
}

TEST_CASE("Ui interaction registry resolves slots and blocking regions", "[ui][input]") {
    ui::UiInteractionRegistry registry;
    ui::InGameUiVisibility visibility{};
    visibility.inventoryVisible = true;
    visibility.characterVisible = false;
    visibility.trading = true;

    const ui::UiScale scale(1280, 720);
    ui::buildInGameHitRegions(
        scale,
        visibility,
        6,
        4,
        6,
        4,
        6,
        3,
        mirrorAnchor(),
        12.0F,
        12.0F,
        220.0F,
        registry);

    const ui::InventoryPaperDollLayout inventory = ui::computeInventoryPaperDollLayout(scale, 6, 4);
    const ui::Rect slot0 = inventory.inventorySlotRect(0);
    const float probeX = slot0.x + slot0.width * 0.5F;
    const float probeY = slot0.y + slot0.height * 0.5F;
    const std::optional<int> hovered = registry.inventorySlotAt(probeX, probeY);
    REQUIRE(hovered.has_value());
    CHECK(*hovered == 0);
    CHECK(registry.blocksWorldInput(probeX, probeY));

  const ui::TradeWindowLayout trade = ui::computeTradeWindowLayout(scale);
    const float tradeX = trade.playerPanel.x + 8.0F;
    const float tradeY = trade.playerPanel.y + 8.0F;
    CHECK(registry.blocksWorldInput(tradeX, tradeY));

    CHECK_FALSE(registry.blocksWorldInput(2.0F, 2.0F));
}

TEST_CASE("Grouped numbers and stacked loot beams", "[ui][loot]") {
    CHECK(ui::formatGroupedNumber(39007431) == "39,007,431");
    CHECK(ui::formatGroupedNumber(0) == "0");
    CHECK(ui::formatGroupedNumber(-1200) == "-1,200");

    ui::LootPresentation loot;
    std::vector<ui::LootLabel> labels = {
        {"Rusty Knife", 0.8F, 0.8F, 0.8F, 0},
        {"Infinity Edge", 1.0F, 0.6F, 0.16F, 2},
        {"Divine Brand", 0.3F, 0.95F, 0.4F, 3},
    };
    loot.spawn(1.0F, 0.0F, 2.0F, labels, 2.2F);
    REQUIRE(loot.beacons().size() == 1);
    CHECK(loot.beacons().front().labels.front().name == "Divine Brand");
    CHECK(loot.beacons().front().labels.back().name == "Rusty Knife");
    CHECK(ui::LootPresentation::beamHeight(2.2F) > ui::LootPresentation::beamHeight(0.55F));
    loot.update(7.0F);
    CHECK(loot.beacons().empty());
}

TEST_CASE("Item compare cards sit side by side on screen", "[ui][tooltip]") {
    const ui::UiScale scale(1280, 720);
    const std::vector<std::string> candidate = {"Bleeding Edge", "Divine Weapon", "826 DPS"};
    const std::vector<std::string> equipped = {"Infinity Edge", "Legendary Weapon", "646 DPS"};
    const ui::ItemCompareCards cards = ui::placeItemCompareCards(
        scale, 700.0F, 180.0F, candidate, equipped, 1.7F, ui::TextWidthMeasureFn{}, 1280, 720);
    CHECK(cards.showEquipped);
    CHECK(cards.equipped.box.x + cards.equipped.box.width <= cards.candidate.box.x + 1.0F);
    CHECK(cards.equipped.box.x >= 0.0F);
    CHECK(cards.candidate.box.x + cards.candidate.box.width <= 1280.0F);
}

TEST_CASE("Item tooltip layout reserves border inset and content padding", "[ui][tooltip]") {
    const ui::UiScale scale(1280, 720);
    const std::vector<std::string> lines = {"Health Tonic", "Common Consumable", "+25 Health"};
    const ui::TooltipBoxLayout layout = ui::computeTooltipBoxLayout(
        scale, 400.0F, 300.0F, lines, 1.85F, ui::TextWidthMeasureFn{}, 1280, 720);

    CHECK(layout.contentInsetX >= scale.dim(24.0F));
    CHECK(layout.contentInsetY >= scale.dim(24.0F));
    CHECK(
        layout.box.width >=
        ui::estimateTextWidth("Common Consumable", 1.85F) + layout.contentInsetX * 2.0F - 0.5F);
    CHECK(layout.box.height > layout.lineHeight * 3.0F + layout.contentInsetY * 2.0F);
}

TEST_CASE("Settings panel rows do not overlap at 4K scale", "[ui][settings]") {
    const ui::UiScale scale(3840, 2160);
    const ui::SettingsPanelLayout layout = ui::computeSettingsPanelLayout(scale);

    CHECK(layout.panel.width > 0.0F);
    CHECK(layout.panel.x == Catch::Approx(1920.0F - layout.panel.width * 0.5F).margin(2.0F));

    for (int row = 0; row < ui::SettingsPanelLayout::kRowCount - 1; ++row) {
        const ui::SettingsRowLayout& current = layout.rows[row];
        const ui::SettingsRowLayout& next = layout.rows[row + 1];
        CHECK(current.control.y + current.control.height <= next.label.y + 1.0F);
        CHECK(current.label.y + current.label.height <= current.control.y);
        if (current.kind == ui::SettingsRowKind::Slider) {
            CHECK(current.value.y == Catch::Approx(current.label.y).margin(0.5F));
        }
    }
}

TEST_CASE("Globe liquid stays inside the ring and skill labels clear the hotkey", "[ui][hud]") {
    constexpr float kRadius = 52.0F;
    const float liquidRadius = kRadius * ui::kGlobeLiquidRadiusFraction;
    const float wave = kRadius * ui::kGlobeWaveAmplitudeFraction;
    CHECK(liquidRadius + wave < kRadius * ui::kGlobeRingInnerRadiusFraction);

    for (int step = 0; step < 32; ++step) {
        const float phase = static_cast<float>(step) * 0.4F;
        const float surface = ui::liquidSurfaceY(100.0F, liquidRadius, 0.65F, phase, wave);
        CHECK(surface >= 100.0F - liquidRadius - 0.01F);
        CHECK(surface <= 100.0F + liquidRadius + 0.01F);
        const float half = ui::discHalfWidth(liquidRadius, surface - 100.0F);
        CHECK(half <= liquidRadius + 0.01F);
    }

    const ui::Rect slot{100.0F, 200.0F, 46.0F, 46.0F};
    const ui::Rect glyph = ui::hudGlyphRect(slot, ui::kHudHotkeyBand);
    const ui::Rect hotkey = ui::hudHotkeyRect(slot, ui::kHudHotkeyBand);
    CHECK(glyph.y + glyph.height <= hotkey.y + 0.01F);
    CHECK(hotkey.y + hotkey.height <= slot.y + slot.height + 0.01F);
    CHECK(ui::hudCooldownRadius(slot) * 2.0F < slot.width);
}

TEST_CASE("Minimap radar is isotropic around the player", "[ui][minimap]") {
    ui::MinimapSystem minimap;
    minimap.setViewport(ui::Rect2D{0.0F, 0.0F, 200.0F, 200.0F});
    minimap.setViewRadius(40.0F);

    const ui::MinimapLayer layer = minimap.buildLayer(
        10.0F,
        -4.0F,
        {{30.0F, -4.0F, ui::MinimapBlipKind::Ally}, {10.0F, 16.0F, ui::MinimapBlipKind::Loot}});

    CHECK(layer.player.pixel.x == Catch::Approx(100.0F).margin(1e-3F));
    CHECK(layer.player.pixel.y == Catch::Approx(100.0F).margin(1e-3F));
    REQUIRE(layer.entities.size() == 2);
    const float offsetX = layer.entities[0].pixel.x - layer.player.pixel.x;
    const float offsetY = layer.entities[1].pixel.y - layer.player.pixel.y;
    CHECK(offsetX == Catch::Approx(offsetY).margin(1e-2F));
    CHECK(offsetX > 0.0F);
}


TEST_CASE("Item tooltips prefer left and avoid inventory sidebar", "[ui][tooltip]") {
    const ui::UiScale scale(1280, 720);
    const std::vector<std::string> lines = {"Socketed Swift Leggings", "Rare Armor", "+12 Vitality"};
    const ui::Rect sidebar{900.0F, 80.0F, 180.0F, 420.0F};
    const ui::TooltipBoxLayout left = ui::computeTooltipBoxLayout(
        scale, 860.0F, 200.0F, lines, 1.7F, ui::TextWidthMeasureFn{}, 1280, 720, true, &sidebar);
    CHECK(left.box.x + left.box.width <= sidebar.x + 1.0F);
    CHECK(left.box.x >= 0.0F);

    const ui::ItemCompareCards cards = ui::placeItemCompareCards(
        scale,
        860.0F,
        200.0F,
        lines,
        {"Equipped Pants", "Common Armor"},
        1.7F,
        ui::TextWidthMeasureFn{},
        1280,
        720,
        true,
        &sidebar);
    CHECK(cards.candidate.box.x + cards.candidate.box.width <= sidebar.x + 1.0F);
}
