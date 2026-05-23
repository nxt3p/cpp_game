#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

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

TEST_CASE("Minimap preserves terrain aspect inside square widget", "[ui][minimap]") {
    ui::MinimapSystem minimap;
    minimap.setViewport(ui::Rect2D{0.0F, 0.0F, 200.0F, 200.0F});
    minimap.setTerrainBounds(ui::TerrainBounds{0.0F, 200.0F, 0.0F, 100.0F});

    const ui::MinimapLayer layer = minimap.buildLayer(100.0F, 50.0F, {{50.0F, 25.0F}});

    CHECK(layer.player.normalized.x == Catch::Approx(0.5F).margin(1e-4F));
    CHECK(layer.player.normalized.y == Catch::Approx(0.5F).margin(1e-4F));
    CHECK(layer.player.pixel.x == Catch::Approx(100.0F).margin(1e-3F));
    CHECK(layer.player.pixel.y == Catch::Approx(100.0F).margin(1e-3F));

    const float entityOffsetX = layer.entities.front().pixel.x - layer.player.pixel.x;
    const float entityOffsetY = layer.entities.front().pixel.y - layer.player.pixel.y;
    CHECK(std::abs(entityOffsetX) == Catch::Approx(std::abs(entityOffsetY * 2.0F)).margin(1e-2F));
}
