#include "ui/UiLayout.hpp"

#include <algorithm>

namespace ui {

namespace {

constexpr float kReferenceHudPortrait = 72.0F;
constexpr float kReferenceHudBarWidth = 220.0F;
constexpr float kReferenceHudBarHeight = 18.0F;

Rect anchorRect(
    const UiScale& scale,
    const ScreenAnchor anchor,
    const float marginX,
    const float marginY,
    const float width,
    const float height) noexcept {
    const float scaledMarginX = scale.x(marginX);
    const float scaledMarginY = scale.y(marginY);
    const float scaledWidth = scale.dim(width);
    const float scaledHeight = scale.dim(height);

    Rect rect{};
    rect.width = scaledWidth;
    rect.height = scaledHeight;

    switch (anchor) {
    case ScreenAnchor::TopRight:
        rect.x = static_cast<float>(scale.width) - scaledMarginX - scaledWidth;
        rect.y = scaledMarginY;
        break;
    case ScreenAnchor::TopLeft:
        rect.x = scaledMarginX;
        rect.y = scale.y(92.0F) + scaledMarginY;
        break;
    case ScreenAnchor::BottomRight:
        rect.x = static_cast<float>(scale.width) - scaledMarginX - scaledWidth;
        rect.y = static_cast<float>(scale.height) - scaledMarginY - scaledHeight;
        break;
    case ScreenAnchor::BottomLeft:
        rect.x = scaledMarginX;
        rect.y = static_cast<float>(scale.height) - scaledMarginY - scaledHeight;
        break;
    }

    return rect;
}

} // namespace

Rect InventoryGridLayout::panelRect() const noexcept {
    return {panelX, panelY, panelWidth, panelHeight};
}

Rect InventoryGridLayout::inventorySlotRect(const int index) const noexcept {
    const int column = index % columns;
    const int row = index / columns;
    const float x = panelX + padding + static_cast<float>(column) * slotSize;
    const float y = panelY + padding + titleBandHeight + static_cast<float>(row) * slotSize;
    return {x, y, slotSize - 4.0F, slotSize - 4.0F};
}

InventoryGridLayout computeInventoryGridLayout(
    const UiScale& scale,
    const int columns,
    const int rows) noexcept {
    InventoryGridLayout layout{};
    layout.columns = columns;
    layout.rows = rows;
    layout.slotSize = scale.dim(52.0F);
    layout.padding = scale.dim(12.0F);
    layout.titleBandHeight = scale.dim(32.0F);
    layout.panelWidth = static_cast<float>(columns) * layout.slotSize + layout.padding * 2.0F;
    layout.panelHeight =
        static_cast<float>(rows) * layout.slotSize + layout.padding * 2.0F + layout.titleBandHeight;
    layout.panelX = static_cast<float>(scale.width) * 0.5F - layout.panelWidth * 0.5F;
    layout.panelY = static_cast<float>(scale.height) * 0.5F - layout.panelHeight * 0.5F;
    return layout;
}

namespace {

struct SlotGridCell {
    int column{0};
    int row{0};
};

// EquipmentSlotKind order must match systems::EquipmentSlotKind.
constexpr SlotGridCell kPaperDollSlotCells[15] = {
    {1, 0}, // Head
    {0, 0}, // Shoulders
    {1, 2}, // Chest
    {0, 2}, // Hands
    {1, 3}, // Waist
    {0, 3}, // Legs
    {0, 4}, // Feet
    {0, 1}, // Weapon
    {2, 2}, // OffHand
    {2, 1}, // Amulet
    {2, 3}, // RingLeft
    {2, 4}, // RingRight
    {2, 0}, // Cloak
    {1, 4}, // Charm
    {2, 5}, // Relic — bottom-right pillar
};

} // namespace

Rect InventoryPaperDollLayout::equipmentSlotRect(const int equipmentSlotIndex) const noexcept {
    if (equipmentSlotIndex < 0 || equipmentSlotIndex >= 15) {
        return {};
    }

    const SlotGridCell cell = kPaperDollSlotCells[equipmentSlotIndex];
    const float x = dollGridLeft + static_cast<float>(cell.column) * (slotSize + slotGap);
    const float y = dollGridTop + static_cast<float>(cell.row) * (slotSize + slotGap);
    return {x, y, slotSize, slotSize};
}

Rect InventoryPaperDollLayout::inventorySlotRect(const int index) const noexcept {
    const int column = index % bagColumns;
    const int row = index / bagColumns;
    const float bagTop =
        panel.y + padding + titleBandHeight + dollBandHeight + bagSeparatorHeight;
    const float bagWidth = static_cast<float>(bagColumns) * slotSize + slotGap * static_cast<float>(bagColumns - 1);
    const float dollAreaWidth = panel.width - statsSidebarWidth - padding * 3.0F;
    const float bagLeft = panel.x + padding + (dollAreaWidth - bagWidth) * 0.5F;
    const float x = bagLeft + static_cast<float>(column) * (slotSize + slotGap);
    const float y = bagTop + static_cast<float>(row) * (slotSize + slotGap);
    return {x, y, slotSize, slotSize};
}

InventoryPaperDollLayout computeInventoryPaperDollLayout(
    const UiScale& scale,
    const int bagColumns,
    const int bagRows) noexcept {
    InventoryPaperDollLayout layout{};
    layout.bagColumns = bagColumns;
    layout.bagRows = bagRows;
    layout.slotSize = scale.dim(50.0F);
    layout.slotGap = scale.dim(6.0F);
    layout.padding = scale.dim(14.0F);
    layout.titleBandHeight = scale.dim(34.0F);
    layout.dollBandHeight = scale.dim(352.0F);
    layout.bagSeparatorHeight = scale.dim(26.0F);
    layout.statsSidebarWidth = scale.dim(168.0F);

    const float dollGridWidth = layout.slotSize * 3.0F + layout.slotGap * 2.0F;
    const float bagWidth =
        static_cast<float>(bagColumns) * layout.slotSize + layout.slotGap * static_cast<float>(bagColumns - 1);
    const float dollAreaWidth = std::max(dollGridWidth, bagWidth);
    const float panelContentWidth = dollAreaWidth + layout.statsSidebarWidth + layout.padding;
    const float panelWidth = panelContentWidth + layout.padding * 2.0F;
    const float bagHeight =
        static_cast<float>(bagRows) * layout.slotSize + layout.slotGap * static_cast<float>(bagRows - 1);
    const float panelHeight = layout.padding * 2.0F + layout.titleBandHeight + layout.dollBandHeight +
                              layout.bagSeparatorHeight + bagHeight;

    layout.panel = {
        static_cast<float>(scale.width) * 0.5F - panelWidth * 0.5F,
        static_cast<float>(scale.height) * 0.5F - panelHeight * 0.5F,
        panelWidth,
        panelHeight};

    layout.dollGridTop = layout.panel.y + layout.padding + layout.titleBandHeight;
    layout.dollGridLeft =
        layout.panel.x + layout.padding + (dollAreaWidth - dollGridWidth) * 0.5F;

    const float portraitPad = scale.dim(3.0F);
    layout.portrait = {
        layout.dollGridLeft + layout.slotSize + layout.slotGap + portraitPad,
        layout.dollGridTop + layout.slotSize + layout.slotGap + portraitPad,
        layout.slotSize - portraitPad * 2.0F,
        layout.slotSize * 2.0F + layout.slotGap - portraitPad * 2.0F};

    const float sidebarInset = scale.dim(10.0F);
    layout.statsSidebar = {
        layout.panel.x + panelWidth - layout.padding - layout.statsSidebarWidth,
        layout.dollGridTop,
        layout.statsSidebarWidth,
        panelHeight - layout.padding * 2.0F - layout.titleBandHeight};

    const float barHeight = scale.dim(16.0F);
    const float barWidth = layout.statsSidebar.width - sidebarInset * 2.0F;
    const float barX = layout.statsSidebar.x + sidebarInset;
    float sidebarCursorY = layout.statsSidebar.y + scale.dim(36.0F);
    layout.hpBar = {barX, sidebarCursorY, barWidth, barHeight};
    sidebarCursorY += barHeight + scale.dim(10.0F);
    layout.xpBar = {barX, sidebarCursorY, barWidth, barHeight};
    sidebarCursorY += barHeight + scale.dim(12.0F);
    layout.goldLabel = {
        barX,
        sidebarCursorY,
        barWidth,
        scale.dim(20.0F)};

    const float bagTop =
        layout.panel.y + layout.padding + layout.titleBandHeight + layout.dollBandHeight;
    layout.bagDivider = {
        layout.panel.x + layout.padding,
        bagTop,
        dollAreaWidth,
        scale.dim(2.0F)};
    layout.bagHeader = {
        layout.panel.x + layout.padding,
        bagTop - scale.dim(20.0F),
        dollAreaWidth,
        scale.dim(18.0F)};

    return layout;
}

CharacterPanelLayout computeCharacterPanelLayout(const UiScale& scale) noexcept {
    CharacterPanelLayout layout{};
    const float panelW = scale.dim(500.0F);
    const float panelH = scale.dim(548.0F);
    const float pad = scale.dim(18.0F);
    layout.panel = {
        static_cast<float>(scale.width) * 0.5F - panelW * 0.5F,
        static_cast<float>(scale.height) * 0.5F - panelH * 0.5F,
        panelW,
        panelH};

    layout.titleBand = {
        layout.panel.x + pad,
        layout.panel.y + scale.dim(10.0F),
        panelW - pad * 2.0F,
        scale.dim(30.0F)};

    const float headerBottom = layout.titleBand.y + layout.titleBand.height + scale.dim(6.0F);
    const float portraitSize = scale.dim(84.0F);
    layout.portrait = {layout.panel.x + pad, headerBottom, portraitSize, portraitSize};

    const float barX = layout.portrait.x + portraitSize + scale.dim(12.0F);
    const float barWidth = layout.panel.x + layout.panel.width - pad - barX;
    const float barHeight = scale.dim(15.0F);
    layout.hpBar = {barX, headerBottom + scale.dim(6.0F), barWidth, barHeight};
    layout.xpBar = {
        barX,
        layout.hpBar.y + barHeight + scale.dim(8.0F),
        barWidth,
        barHeight};
    layout.goldLabel = {
        barX,
        layout.xpBar.y + barHeight + scale.dim(8.0F),
        barWidth,
        scale.dim(18.0F)};

    const float upgradeSectionY = layout.portrait.y + portraitSize + scale.dim(14.0F);
    const float upgradeHeaderHeight = scale.dim(22.0F);
    layout.upgradeHeader = {
        layout.panel.x + pad,
        upgradeSectionY,
        panelW - pad * 2.0F,
        upgradeHeaderHeight};

    const float buttonY = upgradeSectionY + upgradeHeaderHeight + scale.dim(6.0F);
    const float buttonHeight = scale.dim(34.0F);
    const float buttonGap = scale.dim(8.0F);
    const float buttonWidth = (panelW - pad * 2.0F - buttonGap * 2.0F) / 3.0F;
    float buttonX = layout.panel.x + pad;
    layout.upgradeStrengthButton = {buttonX, buttonY, buttonWidth, buttonHeight};
    buttonX += buttonWidth + buttonGap;
    layout.upgradeDexterityButton = {buttonX, buttonY, buttonWidth, buttonHeight};
    buttonX += buttonWidth + buttonGap;
    layout.upgradeVitalityButton = {buttonX, buttonY, buttonWidth, buttonHeight};

    const float statsY = buttonY + buttonHeight + scale.dim(14.0F);
    const float footerHeight = scale.dim(22.0F);
    layout.footerHint = {
        layout.panel.x + pad,
        layout.panel.y + panelH - footerHeight - scale.dim(4.0F),
        panelW - pad * 2.0F,
        footerHeight};
    layout.statsText = {
        layout.panel.x + pad,
        statsY,
        panelW - pad * 2.0F,
        layout.footerHint.y - scale.dim(8.0F) - statsY};

    layout.titleScale = scale.dim(2.0F);
    layout.bodyScale = scale.dim(1.55F);
    layout.statLabelScale = scale.dim(1.35F);
    return layout;
}

Rect TradeWindowLayout::playerSlotRect(const int index) const noexcept {
    const int column = index % playerColumns;
    const int row = index / playerColumns;
    const float x = playerPanel.x + gridPadX + static_cast<float>(column) * (slotSize + slotGap);
    const float y = playerPanel.y + gridTopOffset + static_cast<float>(row) * (slotSize + slotGap);
    return {x, y, slotSize, slotSize};
}

Rect TradeWindowLayout::vendorSlotRect(const int index) const noexcept {
    const int column = index % vendorColumns;
    const int row = index / vendorColumns;
    const float x = vendorPanel.x + gridPadX + static_cast<float>(column) * (slotSize + slotGap);
    const float y = vendorPanel.y + gridTopOffset + static_cast<float>(row) * (slotSize + slotGap);
    return {x, y, slotSize, slotSize};
}

Rect TradeWindowLayout::serviceButtonRect(const int serviceIndex) const noexcept {
    const float buttonGap = slotGap;
    const float totalGap = buttonGap * static_cast<float>(kServiceCount - 1);
    const float buttonWidth = (servicesPanel.width - gridPadX * 2.0F - totalGap) /
                              static_cast<float>(kServiceCount);
    const float x = servicesPanel.x + gridPadX +
                    static_cast<float>(serviceIndex) * (buttonWidth + buttonGap);
    return {x, servicesButtonY, buttonWidth, servicesButtonHeight};
}

TradeWindowLayout computeTradeWindowLayout(const UiScale& scale) noexcept {
    TradeWindowLayout layout{};
    layout.slotSize = scale.dim(44.0F);
    layout.slotGap = scale.dim(6.0F);
    layout.gridPadX = scale.dim(14.0F);
    layout.gridTopOffset = scale.dim(72.0F);

    const float panelW = scale.dim(360.0F);
    const float panelH = scale.dim(330.0F);
    const float sideMargin = scale.x(72.0F);
    const float panelY = scale.y(118.0F);

    layout.playerPanel = {sideMargin, panelY, panelW, panelH};
    layout.vendorPanel = {
        static_cast<float>(scale.width) - sideMargin - panelW,
        panelY,
        panelW,
        panelH};

    const float servicesW = scale.dim(620.0F);
    const float servicesH = scale.dim(92.0F);
    layout.servicesPanel = {
        static_cast<float>(scale.width) * 0.5F - servicesW * 0.5F,
        panelY + panelH + scale.dim(12.0F),
        servicesW,
        servicesH};

    const float titlePadX = scale.dim(16.0F);
    const float titlePadY = scale.dim(12.0F);
    const float titleHeight = scale.dim(28.0F);
    layout.playerTitle = {
        layout.playerPanel.x + titlePadX,
        layout.playerPanel.y + titlePadY,
        layout.playerPanel.width - titlePadX * 2.0F,
        titleHeight};
    layout.vendorTitle = {
        layout.vendorPanel.x + titlePadX,
        layout.vendorPanel.y + titlePadY,
        layout.vendorPanel.width - titlePadX * 2.0F,
        titleHeight};
    layout.servicesTitle = {
        layout.servicesPanel.x + titlePadX,
        layout.servicesPanel.y + scale.dim(8.0F),
        layout.servicesPanel.width - titlePadX * 2.0F,
        titleHeight};
    layout.servicesButtonY = layout.servicesTitle.y + layout.servicesTitle.height + scale.dim(6.0F);
    layout.servicesButtonHeight =
        layout.servicesPanel.height - (layout.servicesButtonY - layout.servicesPanel.y) - scale.dim(8.0F);

    const float goldBandY = layout.playerPanel.y + scale.dim(44.0F);
    const float goldBandHeight = scale.dim(22.0F);
    layout.playerGoldLabel = {
        layout.playerPanel.x + titlePadX,
        goldBandY,
        layout.playerPanel.width - titlePadX * 2.0F,
        goldBandHeight};
    layout.vendorGoldLabel = {
        layout.vendorPanel.x + titlePadX,
        goldBandY,
        layout.vendorPanel.width - titlePadX * 2.0F,
        goldBandHeight};

    layout.titleScale = scale.dim(2.0F);
    layout.valueScale = scale.dim(1.7F);
    layout.serviceScale = scale.dim(1.35F);
    return layout;
}

MinimapWidgetLayout computeMinimapWidgetLayout(
    const UiScale& scale,
    const ScreenAnchor anchor,
    const float marginX,
    const float marginY,
    const float frameSize) noexcept {
    MinimapWidgetLayout layout{};
    layout.frame = anchorRect(scale, anchor, marginX, marginY, frameSize, frameSize);
    const float inset = scale.dim(10.0F);
    layout.content = {
        layout.frame.x + inset,
        layout.frame.y + inset,
        layout.frame.width - inset * 2.0F,
        layout.frame.height - inset * 2.0F};
    return layout;
}

SettingsPanelLayout computeSettingsPanelLayout(const UiScale& scale) noexcept {
    SettingsPanelLayout layout{};
    constexpr float kRefPanelWidth = 520.0F;
    constexpr float kRefPanelHeight = 420.0F;
    constexpr float kRefPadding = 36.0F;
    constexpr float kRefRowHeight = 58.0F;
    constexpr float kRefLabelHeight = 22.0F;
    constexpr float kRefControlHeight = 24.0F;
    constexpr float kRefValueWidth = 140.0F;
    constexpr float kRefTitleBand = 52.0F;

    const float panelW = scale.dim(kRefPanelWidth);
    const float panelH = scale.dim(kRefPanelHeight);
    const float padding = scale.dim(kRefPadding);
    const float rowHeight = scale.dim(kRefRowHeight);
    const float labelHeight = scale.dim(kRefLabelHeight);
    const float controlHeight = scale.dim(kRefControlHeight);
    const float valueWidth = scale.dim(kRefValueWidth);
    const float titleBand = scale.dim(kRefTitleBand);

    layout.panel = {
        static_cast<float>(scale.width) * 0.5F - panelW * 0.5F,
        scale.y(108.0F),
        panelW,
        panelH};
    layout.titleY = layout.panel.y + scale.dim(12.0F);
    layout.titleScale = scale.dim(2.6F);
    layout.labelScale = scale.dim(1.9F);
    layout.valueScale = scale.dim(1.7F);

    const float closeSize = scale.dim(32.0F);
    layout.closeButton = {
        layout.panel.x + layout.panel.width - closeSize - scale.dim(10.0F),
        layout.panel.y + scale.dim(8.0F),
        closeSize,
        closeSize};

    const float contentWidth = layout.panel.width - padding * 2.0F;
    const float labelWidth = contentWidth - valueWidth - scale.dim(8.0F);
    float rowTop = layout.panel.y + titleBand;

    const SettingsRowKind rowKinds[SettingsPanelLayout::kRowCount] = {
        SettingsRowKind::Cycle,
        SettingsRowKind::Slider,
        SettingsRowKind::Slider,
        SettingsRowKind::Cycle,
        SettingsRowKind::Slider,
        SettingsRowKind::Cycle,
    };

    for (int index = 0; index < SettingsPanelLayout::kRowCount; ++index) {
        SettingsRowLayout& row = layout.rows[index];
        row.kind = rowKinds[index];
        row.label = {
            layout.panel.x + padding,
            rowTop,
            labelWidth,
            labelHeight};
        row.value = {
            layout.panel.x + layout.panel.width - padding - valueWidth,
            rowTop,
            valueWidth,
            labelHeight};
        row.control = {
            layout.panel.x + padding,
            rowTop + labelHeight + scale.dim(4.0F),
            contentWidth,
            controlHeight};
        rowTop += rowHeight;
    }

    return layout;
}

TooltipBoxLayout computeTooltipBoxLayout(
    const UiScale& scale,
    const float anchorX,
    const float anchorY,
    const std::vector<std::string>& lines,
    const float textScale,
    const TextWidthMeasureFn& measureWidth,
    const int screenWidth,
    const int screenHeight) noexcept {
    TooltipBoxLayout layout{};
    if (lines.empty()) {
        return layout;
    }

    const TextWidthMeasureFn measure =
        measureWidth ? measureWidth : TextWidthMeasureFn(estimateTextWidth);

    layout.nineSliceBorder = scale.dim(14.0F);
    const float contentPadding = scale.dim(12.0F);
    layout.contentInsetX = layout.nineSliceBorder + contentPadding;
    layout.contentInsetY = layout.nineSliceBorder + contentPadding;
    layout.lineHeight = 12.0F * textScale + scale.dim(2.0F);

    float maxTextWidth = 0.0F;
    for (const std::string& line : lines) {
        maxTextWidth = std::max(maxTextWidth, measure(line.c_str(), textScale));
    }

    const float minInnerWidth = scale.dim(140.0F);
    layout.lineGap = scale.dim(4.0F);
    const float innerWidth = std::max(maxTextWidth, minInnerWidth);
    const float innerHeight = layout.lineHeight * static_cast<float>(lines.size()) +
                              layout.lineGap * static_cast<float>(lines.size() > 1 ? lines.size() - 1 : 0);

    const float boxW = innerWidth + layout.contentInsetX * 2.0F;
    const float boxH = innerHeight + layout.contentInsetY * 2.0F;

    float boxX = anchorX + scale.dim(12.0F);
    float boxY = anchorY;
    const float screenMargin = scale.dim(8.0F);

    if (boxX + boxW > static_cast<float>(screenWidth) - screenMargin) {
        boxX = anchorX - boxW - scale.dim(12.0F);
    }
    if (boxY + boxH > static_cast<float>(screenHeight) - screenMargin) {
        boxY = anchorY - boxH;
    }
    if (boxY < screenMargin) {
        boxY = screenMargin;
    }

    layout.box = {boxX, boxY, boxW, boxH};
    return layout;
}

HudChromeLayout computeHudChromeLayout(const UiScale& scale) noexcept {
    HudChromeLayout layout{};
    layout.statusHud = {
        scale.x(8.0F),
        scale.y(8.0F),
        scale.dim(kReferenceHudPortrait + kReferenceHudBarWidth + 28.0F),
        scale.dim(kReferenceHudPortrait + kReferenceHudBarHeight * 2.0F + 28.0F)};

    layout.messageStrip = {
        scale.x(12.0F),
        scale.y(12.0F) + scale.dim(kReferenceHudPortrait + kReferenceHudBarHeight * 2.0F + 12.0F),
        scale.dim(420.0F),
        scale.dim(26.0F)};
    return layout;
}

} // namespace ui
