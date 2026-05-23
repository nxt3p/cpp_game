#pragma once

#include "ui/UiHitTest.hpp"
#include "ui/UiScale.hpp"
#include "ui/UiTextLayout.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

enum class ScreenAnchor {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct InventoryGridLayout {
    float panelX{0.0F};
    float panelY{0.0F};
    float panelWidth{0.0F};
    float panelHeight{0.0F};
    float slotSize{52.0F};
    float padding{12.0F};
    float titleBandHeight{32.0F};
    int columns{6};
    int rows{4};

    [[nodiscard]] Rect panelRect() const noexcept;
    [[nodiscard]] Rect inventorySlotRect(int index) const noexcept;
};

/// Paper-doll inventory: player portrait center, 15 equipment slots around, bag grid below.
struct InventoryPaperDollLayout {
    Rect panel{};
    Rect statsSidebar{};
    Rect portrait{};
    Rect xpBar{};
    Rect hpBar{};
    Rect goldLabel{};
    Rect bagHeader{};
    Rect bagDivider{};
    float dollGridLeft{0.0F};
    float dollGridTop{0.0F};
    float slotSize{48.0F};
    float slotGap{6.0F};
    float padding{14.0F};
    float titleBandHeight{32.0F};
    float dollBandHeight{340.0F};
    float bagSeparatorHeight{10.0F};
    float statsSidebarWidth{0.0F};
    int bagColumns{6};
    int bagRows{4};

    [[nodiscard]] Rect equipmentSlotRect(int equipmentSlotIndex) const noexcept;
    [[nodiscard]] Rect inventorySlotRect(int index) const noexcept;
};

struct CharacterPanelLayout {
    Rect panel{};
    Rect titleBand{};
    Rect portrait{};
    Rect xpBar{};
    Rect hpBar{};
    Rect goldLabel{};
    Rect upgradeHeader{};
    Rect upgradeStrengthButton{};
    Rect upgradeDexterityButton{};
    Rect upgradeVitalityButton{};
    Rect statsText{};
    Rect footerHint{};
    float titleScale{2.2F};
    float bodyScale{1.75F};
    float statLabelScale{1.55F};
};

struct TradeWindowLayout {
    Rect playerPanel{};
    Rect vendorPanel{};
    Rect servicesPanel{};
    Rect playerTitle{};
    Rect vendorTitle{};
    Rect servicesTitle{};
    Rect playerGoldLabel{};
    float servicesButtonY{0.0F};
    float servicesButtonHeight{0.0F};
    Rect vendorGoldLabel{};
    float slotSize{44.0F};
    float slotGap{6.0F};
    float gridPadX{14.0F};
    float gridTopOffset{72.0F};
    int playerColumns{6};
    int playerRows{4};
    int vendorColumns{6};
    int vendorRows{3};
    static constexpr int kServiceCount = 5;
    float titleScale{2.0F};
    float valueScale{1.7F};
    float serviceScale{1.45F};

    [[nodiscard]] Rect playerSlotRect(int index) const noexcept;
    [[nodiscard]] Rect vendorSlotRect(int index) const noexcept;
    [[nodiscard]] Rect serviceButtonRect(int serviceIndex) const noexcept;
};

struct MinimapWidgetLayout {
    Rect frame{};
    Rect content{};
};

struct HudChromeLayout {
    Rect statusHud{};
    Rect messageStrip{};
};

enum class SettingsRowKind : std::uint8_t { Cycle, Slider };

struct SettingsRowLayout {
    Rect label{};
    Rect value{};
    Rect control{};
    SettingsRowKind kind{SettingsRowKind::Cycle};
};

struct SettingsPanelLayout {
    Rect panel{};
    Rect closeButton{};
    float titleY{56.0F};
    float titleScale{2.6F};
    float labelScale{1.9F};
    float valueScale{1.7F};
    static constexpr int kRowCount = 6;
    SettingsRowLayout rows[kRowCount]{};
};

[[nodiscard]] SettingsPanelLayout computeSettingsPanelLayout(const UiScale& scale) noexcept;

[[nodiscard]] InventoryGridLayout computeInventoryGridLayout(
    const UiScale& scale,
    int columns,
    int rows) noexcept;

[[nodiscard]] InventoryPaperDollLayout computeInventoryPaperDollLayout(
    const UiScale& scale,
    int bagColumns,
    int bagRows) noexcept;

[[nodiscard]] CharacterPanelLayout computeCharacterPanelLayout(const UiScale& scale) noexcept;

[[nodiscard]] TradeWindowLayout computeTradeWindowLayout(const UiScale& scale) noexcept;

[[nodiscard]] MinimapWidgetLayout computeMinimapWidgetLayout(
    const UiScale& scale,
    ScreenAnchor anchor,
    float marginX,
    float marginY,
    float frameSize) noexcept;

[[nodiscard]] HudChromeLayout computeHudChromeLayout(const UiScale& scale) noexcept;

struct TooltipBoxLayout {
    Rect box{};
    float contentInsetX{0.0F};
    float contentInsetY{0.0F};
    float lineHeight{0.0F};
    float lineGap{0.0F};
    float nineSliceBorder{0.0F};
};

[[nodiscard]] TooltipBoxLayout computeTooltipBoxLayout(
    const UiScale& scale,
    float anchorX,
    float anchorY,
    const std::vector<std::string>& lines,
    float textScale,
    const TextWidthMeasureFn& measureWidth,
    int screenWidth,
    int screenHeight) noexcept;

} // namespace ui
