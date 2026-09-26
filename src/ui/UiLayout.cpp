#include "ui/UiLayout.hpp"

#include "ui/HudConsoleLayout.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

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
        rect.y = scaledMarginY;
        break;
    case ScreenAnchor::BottomRight:
        rect.x = static_cast<float>(scale.width) - scaledMarginX - scaledWidth;
        rect.y = static_cast<float>(scale.height) - scale.dim(kReferenceHudConsoleHeight) - scaledMarginY -
                 scaledHeight;
        break;
    case ScreenAnchor::BottomLeft:
        rect.x = scaledMarginX;
        rect.y = static_cast<float>(scale.height) - scale.dim(kReferenceHudConsoleHeight) - scaledMarginY -
                 scaledHeight;
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
    {1, 5}, // Chest — kept off the portrait, which occupies column 1 rows 1-2
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
    layout.padding = scale.dim(12.0F);
    layout.titleBandHeight = scale.dim(30.0F);
    layout.bagSeparatorHeight = scale.dim(22.0F);
    layout.statsSidebarWidth = scale.dim(168.0F);

    constexpr int kDollRows = 6;
    const float topLimit = scale.dim(48.0F);
    const float bottomLimit =
        static_cast<float>(scale.height) - scale.dim(kReferenceHudConsoleHeight) - scale.dim(8.0F);
    const float maxPanelHeight = std::max(bottomLimit - topLimit, scale.dim(280.0F));
    const float dollPad = scale.dim(4.0F);
    const float fixedChrome =
        layout.padding * 2.0F + layout.titleBandHeight + layout.bagSeparatorHeight + dollPad;
    const float slotRows = static_cast<float>(kDollRows + std::max(bagRows, 1));
    const float gapRows = static_cast<float>(std::max(kDollRows - 1, 0) + std::max(bagRows - 1, 0));
    constexpr float kGapPerSlot = 6.0F / 50.0F;
    const float perSlot = slotRows + gapRows * kGapPerSlot;
    const float fittedSlot = (maxPanelHeight - fixedChrome) / std::max(perSlot, 1.0F);
    const float slotFloor = scale.dim(28.0F) * scale.touchBoost;
    const float slotCeil = scale.dim(50.0F) * std::max(1.0F, scale.touchBoost);
    layout.slotSize = std::clamp(fittedSlot, slotFloor, slotCeil);
    if (scale.platform != UiPlatformKind::Desktop) {
        layout.slotSize = std::max(layout.slotSize, std::min(scale.minTouchTarget(), fittedSlot));
    }

    const float maxPanelWidth =
        std::max(scale.dim(180.0F), static_cast<float>(scale.width) - scale.dim(8.0F));
    const auto panelWidthForSlot = [&](const float slot) {
        const float gap = slot * kGapPerSlot;
        const float bagWidthForSlot =
            static_cast<float>(bagColumns) * slot + gap * static_cast<float>(std::max(bagColumns - 1, 0));
        const float dollGrid = slot * 3.0F + gap * 2.0F;
        const float dollArea = std::max(dollGrid, bagWidthForSlot);
        return dollArea + layout.statsSidebarWidth + layout.padding * 3.0F;
    };
    for (int guard = 0; guard < 12 && panelWidthForSlot(layout.slotSize) > maxPanelWidth; ++guard) {
        layout.slotSize *= 0.92F;
    }
    if (layout.slotSize > fittedSlot && panelWidthForSlot(fittedSlot) <= maxPanelWidth) {
        layout.slotSize = std::min(layout.slotSize, fittedSlot);
    }
    layout.slotGap = layout.slotSize * kGapPerSlot;
    layout.dollBandHeight =
        static_cast<float>(kDollRows) * layout.slotSize +
        static_cast<float>(kDollRows - 1) * layout.slotGap + dollPad;

    const float dollGridWidth = layout.slotSize * 3.0F + layout.slotGap * 2.0F;
    const float bagWidth =
        static_cast<float>(bagColumns) * layout.slotSize + layout.slotGap * static_cast<float>(std::max(bagColumns - 1, 0));
    const float dollAreaWidth = std::max(dollGridWidth, bagWidth);
    const float panelContentWidth = dollAreaWidth + layout.statsSidebarWidth + layout.padding;
    const float panelWidth = panelContentWidth + layout.padding * 2.0F;
    const float bagHeight =
        static_cast<float>(std::max(bagRows, 0)) * layout.slotSize +
        layout.slotGap * static_cast<float>(std::max(bagRows - 1, 0));
    const float panelHeight = layout.padding * 2.0F + layout.titleBandHeight + layout.dollBandHeight +
                              layout.bagSeparatorHeight + bagHeight;
    const float regionHeight = std::max(bottomLimit - topLimit, panelHeight);
    const float panelY = topLimit + std::max(0.0F, (regionHeight - panelHeight) * 0.5F);

    const TradeWindowLayout trade = computeTradeWindowLayout(scale);
    const float bagLeftInPanel = layout.padding + (dollAreaWidth - bagWidth) * 0.5F;
    const float firstSlotCenterInPanel = bagLeftInPanel + layout.slotSize * 0.5F;
    const float tradeRight = trade.playerPanel.x + trade.playerPanel.width;
    float panelX = static_cast<float>(scale.width) * 0.5F - panelWidth * 0.5F;
    panelX = std::max(panelX, tradeRight - firstSlotCenterInPanel + scale.dim(6.0F));
    const float rightLimit = static_cast<float>(scale.width) - panelWidth - scale.dim(8.0F);
    panelX = std::min(panelX, std::max(rightLimit, 0.0F));

    layout.panel = {panelX, panelY, panelWidth, panelHeight};

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
        bagTop + layout.bagSeparatorHeight - scale.dim(2.0F),
        dollAreaWidth,
        scale.dim(2.0F)};
    layout.bagHeader = {
        layout.panel.x + layout.padding,
        bagTop + scale.dim(2.0F),
        dollAreaWidth,
        std::max(layout.bagSeparatorHeight - scale.dim(4.0F), scale.dim(12.0F))};

    return layout;
}

CharacterPanelLayout computeCharacterPanelLayout(const UiScale& scale) noexcept {
    CharacterPanelLayout layout{};
    const float topLimit = scale.dim(8.0F);
    const float bottomLimit =
        static_cast<float>(scale.height) - scale.dim(kReferenceHudConsoleHeight) - scale.dim(12.0F);
    const float maxHeight = std::max(scale.dim(360.0F), bottomLimit - topLimit);
    const float panelH = std::min(scale.dim(560.0F), maxHeight);
    const float panelW = std::min(scale.dim(940.0F), static_cast<float>(scale.width) - scale.dim(24.0F));
    const float centeredY = static_cast<float>(scale.height) * 0.5F - panelH * 0.5F;
    const float panelY = std::clamp(centeredY, topLimit, std::max(topLimit, bottomLimit - panelH));
    const float pad = scale.dim(16.0F);
    layout.panel = {
        (static_cast<float>(scale.width) - panelW) * 0.5F,
        panelY,
        panelW,
        panelH};

    layout.titleBand = {
        layout.panel.x + pad,
        layout.panel.y + scale.dim(8.0F),
        panelW - pad * 2.0F,
        scale.dim(28.0F)};
    const float closeW = std::min(scale.dim(78.0F), layout.titleBand.width * 0.28F);
    const float closeH = std::min(scale.dim(22.0F), layout.titleBand.height);
    layout.closeButton = {
        layout.titleBand.x + layout.titleBand.width - closeW,
        layout.titleBand.y + (layout.titleBand.height - closeH) * 0.5F,
        closeW,
        closeH};
    layout.titleBand.width = std::max(scale.dim(40.0F), layout.closeButton.x - scale.dim(8.0F) - layout.titleBand.x);

    const float headerBottom = layout.titleBand.y + layout.titleBand.height + scale.dim(8.0F);
    const float leftWidth = panelW * 0.46F;
    const float portraitSize = scale.dim(72.0F);
    layout.portrait = {
        layout.panel.x + pad + scale.dim(8.0F),
        headerBottom,
        portraitSize,
        portraitSize};

    const float labelH = scale.dim(16.0F);
    const float barHeight = scale.dim(10.0F);
    const float barX = layout.portrait.x + portraitSize + scale.dim(10.0F);
    const float barRight = layout.panel.x + pad + leftWidth - scale.dim(8.0F);
    const float barWidth = std::max(scale.dim(48.0F), barRight - barX);
    float cursorY = layout.portrait.y;
    layout.hpLabel = {barX, cursorY, barWidth, labelH};
    cursorY += labelH;
    layout.hpBar = {barX, cursorY, barWidth, barHeight};
    cursorY += barHeight + scale.dim(4.0F);
    layout.soulLabel = {barX, cursorY, barWidth, labelH};
    cursorY += labelH;
    layout.xpBar = {barX, cursorY, barWidth, barHeight};
    cursorY += barHeight + scale.dim(6.0F);
    const float goldY = std::max(cursorY, layout.portrait.y + portraitSize + scale.dim(6.0F));
    layout.goldLabel = {layout.panel.x + pad, goldY, std::max(scale.dim(40.0F), leftWidth - scale.dim(8.0F)), scale.dim(16.0F)};

    const float footerHeight = scale.dim(20.0F);
    layout.footerHint = {
        layout.panel.x + pad,
        layout.panel.y + panelH - footerHeight - scale.dim(6.0F),
        panelW - pad * 2.0F,
        footerHeight};

    const float rightX = layout.panel.x + pad + leftWidth + scale.dim(12.0F);
    const float rightW = layout.panel.x + panelW - pad - rightX;
    layout.upgradeHeader = {rightX, headerBottom, rightW, scale.dim(22.0F)};

    const float buttonHeight = scale.dim(36.0F);
    const float buttonGap = scale.dim(10.0F);
    const float inner = scale.dim(8.0F);
    const float buttonWidth = std::max(scale.dim(36.0F), (rightW - inner * 2.0F - buttonGap * 2.0F) / 3.0F);
    const float statsHeight = scale.dim(64.0F);
    const float buttonY = layout.footerHint.y - scale.dim(8.0F) - statsHeight - scale.dim(8.0F) - buttonHeight;
    float buttonX = rightX + inner;
    layout.upgradeStrengthButton = {buttonX, buttonY, buttonWidth, buttonHeight};
    buttonX += buttonWidth + buttonGap;
    layout.upgradeDexterityButton = {buttonX, buttonY, buttonWidth, buttonHeight};
    buttonX += buttonWidth + buttonGap;
    layout.upgradeVitalityButton = {buttonX, buttonY, buttonWidth, buttonHeight};

    const float statsY = buttonY + buttonHeight + scale.dim(8.0F);
    layout.statsText = {
        rightX + inner,
        statsY,
        std::max(scale.dim(40.0F), rightW - inner * 2.0F),
        std::max(scale.dim(18.0F), layout.footerHint.y - scale.dim(6.0F) - statsY)};

    layout.spellsPane = {
        layout.panel.x + pad,
        headerBottom,
        leftWidth,
        std::max(scale.dim(80.0F), layout.footerHint.y - scale.dim(6.0F) - headerBottom)};
    layout.talentsPane = {
        rightX,
        headerBottom,
        std::max(scale.dim(80.0F), rightW),
        layout.spellsPane.height};

    layout.titleScale = scale.dim(2.1F);
    layout.bodyScale = scale.dim(1.45F);
    layout.statLabelScale = scale.dim(1.25F);
    return layout;
}

AbilityBoardLayout computeAbilityBoardLayout(
    const CharacterPanelLayout& panel,
    const UiScale& scale) noexcept {
    AbilityBoardLayout board{};
    const float pad = scale.dim(10.0F);
    const float headerH = scale.dim(16.0F);
    const float gap = scale.dim(8.0F);
    const float nameH = scale.dim(14.0F);
    const float sectionGap = scale.dim(12.0F);
    const float left = panel.spellsPane.x + pad;
    const float width = std::max(scale.dim(40.0F), panel.spellsPane.width - pad * 2.0F);
    float cursorY = std::max(panel.goldLabel.y + panel.goldLabel.height, panel.portrait.y + panel.portrait.height) +
                    scale.dim(10.0F);
    const float bottom = panel.spellsPane.y + panel.spellsPane.height - scale.dim(6.0F);
    const float available = std::max(scale.dim(48.0F), bottom - cursorY);
    const float chrome = headerH * 3.0F + sectionGap * 3.0F + gap + nameH * 3.0F;
    float icon = scale.dim(40.0F);
    const float needed = chrome + icon * 3.0F;
    if (needed > available) {
        icon = std::max(scale.dim(22.0F), (available - chrome) / 3.0F);
    }

    const auto place = [&](AbilitySpellLayout& section, const int count) {
        section.header = {left, cursorY, width, headerH};
        cursorY += headerH + scale.dim(4.0F);
        section.iconCount = count;
        const float slotW = width / static_cast<float>(std::max(count, 1));
        for (int index = 0; index < count; ++index) {
            const float slotX = left + static_cast<float>(index) * slotW;
            section.icons[static_cast<std::size_t>(index)] = {
                slotX + std::max(0.0F, (slotW - icon) * 0.5F),
                cursorY,
                std::min(icon, slotW),
                icon};
        }
        cursorY += icon + nameH + sectionGap;
    };

    place(board.basic, 3);
    place(board.strong, 2);
    place(board.specialties, 3);
    return board;
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

/// Top of the first backpack cell for the default 6x4 paper doll.
/// Trade chrome stays above this so forge buttons do not steal bag clicks.
float defaultBagSlotTop(const UiScale& scale) noexcept {
    constexpr int kBagColumns = 6;
    constexpr int kBagRows = 4;
    constexpr int kDollRows = 6;
    constexpr float kGapPerSlot = 6.0F / 50.0F;

    const float padding = scale.dim(12.0F);
    const float titleBandHeight = scale.dim(30.0F);
    const float bagSeparatorHeight = scale.dim(22.0F);
    const float statsSidebarWidth = scale.dim(168.0F);
    const float topLimit = scale.dim(48.0F);
    const float bottomLimit =
        static_cast<float>(scale.height) - scale.dim(kReferenceHudConsoleHeight) - scale.dim(8.0F);
    const float maxPanelHeight = std::max(bottomLimit - topLimit, scale.dim(280.0F));
    const float dollPad = scale.dim(4.0F);
    const float fixedChrome = padding * 2.0F + titleBandHeight + bagSeparatorHeight + dollPad;
    const float slotRows = static_cast<float>(kDollRows + kBagRows);
    const float gapRows = static_cast<float>((kDollRows - 1) + (kBagRows - 1));
    const float perSlot = slotRows + gapRows * kGapPerSlot;
    const float fittedSlot = (maxPanelHeight - fixedChrome) / std::max(perSlot, 1.0F);
    const float slotFloor = scale.dim(28.0F) * scale.touchBoost;
    const float slotCeil = scale.dim(50.0F) * std::max(1.0F, scale.touchBoost);
    float slotSize = std::clamp(fittedSlot, slotFloor, slotCeil);
    if (scale.platform != UiPlatformKind::Desktop) {
        slotSize = std::max(slotSize, std::min(scale.minTouchTarget(), fittedSlot));
    }

    const float maxPanelWidth = std::max(scale.dim(180.0F), static_cast<float>(scale.width) - scale.dim(8.0F));
    const auto panelWidthForSlot = [&](const float slot) {
        const float gap = slot * kGapPerSlot;
        const float bagWidthForSlot =
            static_cast<float>(kBagColumns) * slot + gap * static_cast<float>(kBagColumns - 1);
        const float dollGrid = slot * 3.0F + gap * 2.0F;
        const float dollArea = std::max(dollGrid, bagWidthForSlot);
        return dollArea + statsSidebarWidth + padding * 3.0F;
    };
    for (int guard = 0; guard < 12 && panelWidthForSlot(slotSize) > maxPanelWidth; ++guard) {
        slotSize *= 0.92F;
    }
    if (slotSize > fittedSlot && panelWidthForSlot(fittedSlot) <= maxPanelWidth) {
        slotSize = std::min(slotSize, fittedSlot);
    }

    const float slotGap = slotSize * kGapPerSlot;
    const float dollBandHeight =
        static_cast<float>(kDollRows) * slotSize + static_cast<float>(kDollRows - 1) * slotGap + dollPad;
    const float bagHeight =
        static_cast<float>(kBagRows) * slotSize + slotGap * static_cast<float>(kBagRows - 1);
    const float panelHeight =
        padding * 2.0F + titleBandHeight + dollBandHeight + bagSeparatorHeight + bagHeight;
    const float regionHeight = std::max(bottomLimit - topLimit, panelHeight);
    const float panelY = topLimit + std::max(0.0F, (regionHeight - panelHeight) * 0.5F);
    return panelY + padding + titleBandHeight + dollBandHeight + bagSeparatorHeight;
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
    // Phones clamp uniform very low while height scale stays near 1. Vertical
    // chrome follows the taller of the two so forge buttons can hold two lines.
    const float vertical = std::min(std::max(scale.uniform, scale.scaleY), std::max(scale.uniform, 1.85F));
    layout.slotSize = scale.dim(44.0F);
    layout.slotGap = scale.dim(6.0F);
    layout.gridPadX = scale.dim(16.0F);
    layout.gridTopOffset = vertical * 78.0F;

    const float screenW = static_cast<float>(scale.width);
    const float consoleTop = static_cast<float>(scale.height) - scale.dim(kReferenceHudConsoleHeight);
    const float sideMargin = std::max(scale.dim(12.0F), scale.x(36.0F));
    const float panelW = std::min(scale.dim(380.0F), (screenW - sideMargin * 2.0F - scale.dim(16.0F)) * 0.5F);
    const float bagTop = defaultBagSlotTop(scale);
    // Sit under the town notice banner (about the top 52px at 720p).
    const float servicesTop = std::max(scale.dim(54.0F), 56.0F * std::min(vertical, 1.2F));
    float servicesH = 118.0F * vertical;
    const float servicesGap = 8.0F * vertical;
    const float servicesBottomLimit = bagTop - 6.0F * vertical;
    if (servicesTop + servicesH > servicesBottomLimit) {
        servicesH = std::max(56.0F * vertical, servicesBottomLimit - servicesTop);
    }

    const float servicesW = std::min(screenW - scale.dim(28.0F), 980.0F * std::min(vertical, 1.35F));
    layout.servicesPanel = {
        screenW * 0.5F - servicesW * 0.5F,
        servicesTop,
        servicesW,
        servicesH};

    const float panelY = servicesTop + servicesH + servicesGap;
    float panelH = std::min(318.0F * vertical, consoleTop - 8.0F * vertical - panelY);
    panelH = std::max(panelH, 0.0F);

    layout.playerPanel = {sideMargin, panelY, panelW, panelH};
    layout.vendorPanel = {screenW - sideMargin - panelW, panelY, panelW, panelH};

    const float titlePadX = scale.dim(18.0F);
    const float titlePadY = 12.0F * vertical;
    const float titleHeight = 28.0F * vertical;
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
        layout.servicesPanel.y + 8.0F * vertical,
        layout.servicesPanel.width - titlePadX * 2.0F,
        titleHeight};
    layout.servicesButtonY = layout.servicesTitle.y + layout.servicesTitle.height + 8.0F * vertical;
    const float buttonRoom =
        layout.servicesPanel.y + layout.servicesPanel.height - layout.servicesButtonY - 8.0F * vertical;
    layout.servicesButtonHeight = std::max(24.0F, buttonRoom);

    const float goldBandY = layout.playerPanel.y + 46.0F * vertical;
    const float goldBandHeight = 22.0F * vertical;
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

    layout.titleScale = 1.9F * std::min(vertical, 1.65F);
    layout.valueScale = 1.45F * std::min(vertical, 1.65F);
    layout.serviceScale = 1.15F * std::min(vertical, 1.5F);
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
    constexpr float kRefPanelHeight = 478.0F;
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
        scale.y(48.0F),
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

namespace {

void nudgeAwayFrom(Rect& box, const Rect& avoid, const float gap, const float margin, const int screenWidth) noexcept {
    if (!rectsOverlap(box, avoid)) {
        return;
    }
    const float left = avoid.x - box.width - gap;
    const float right = avoid.x + avoid.width + gap;
    if (left >= margin) {
        box.x = left;
        return;
    }
    if (right + box.width <= static_cast<float>(screenWidth) - margin) {
        box.x = right;
        return;
    }
    box.x = std::clamp(box.x, margin, std::max(margin, static_cast<float>(screenWidth) - margin - box.width));
}

} // namespace

TooltipBoxLayout computeTooltipBoxLayout(
    const UiScale& scale,
    const float anchorX,
    const float anchorY,
    const std::vector<std::string>& lines,
    const float textScale,
    const TextWidthMeasureFn& measureWidth,
    const int screenWidth,
    const int screenHeight,
    const bool preferLeft,
    const Rect* avoid) noexcept {
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

    const float screenMargin = scale.dim(8.0F);
    float boxX = preferLeft ? anchorX - boxW - scale.dim(12.0F) : anchorX + scale.dim(12.0F);
    float boxY = anchorY;

    if (boxX < screenMargin) {
        boxX = preferLeft ? anchorX + scale.dim(12.0F) : screenMargin;
    }
    if (boxX + boxW > static_cast<float>(screenWidth) - screenMargin) {
        boxX = anchorX - boxW - scale.dim(12.0F);
    }
    if (boxX < screenMargin) {
        boxX = screenMargin;
    }
    if (boxY + boxH > static_cast<float>(screenHeight) - screenMargin) {
        boxY = anchorY - boxH;
    }
    if (boxY < screenMargin) {
        boxY = screenMargin;
    }

    layout.box = {boxX, boxY, boxW, boxH};
    if (avoid != nullptr) {
        nudgeAwayFrom(layout.box, *avoid, scale.dim(10.0F), screenMargin, screenWidth);
        if (layout.box.y + layout.box.height > static_cast<float>(screenHeight) - screenMargin) {
            layout.box.y = static_cast<float>(screenHeight) - screenMargin - layout.box.height;
        }
        if (layout.box.y < screenMargin) {
            layout.box.y = screenMargin;
        }
    }
    return layout;
}

ItemCompareCards placeItemCompareCards(
    const UiScale& scale,
    const float anchorX,
    const float anchorY,
    const std::vector<std::string>& candidateLines,
    const std::vector<std::string>& equippedLines,
    const float textScale,
    const TextWidthMeasureFn& measureWidth,
    const int screenWidth,
    const int screenHeight,
    const bool preferLeft,
    const Rect* avoid) noexcept {
    ItemCompareCards cards{};
    cards.candidate = computeTooltipBoxLayout(
        scale, anchorX, anchorY, candidateLines, textScale, measureWidth, screenWidth, screenHeight, preferLeft, avoid);
    if (equippedLines.empty()) {
        return cards;
    }

    cards.showEquipped = true;
    cards.equipped = computeTooltipBoxLayout(
        scale, anchorX, anchorY, equippedLines, textScale, measureWidth, screenWidth, screenHeight, preferLeft, avoid);
    const float gap = scale.dim(10.0F);
    const float margin = scale.dim(8.0F);
    cards.equipped.box.x = cards.candidate.box.x - cards.equipped.box.width - gap;
    cards.equipped.box.y = cards.candidate.box.y;

    if (cards.equipped.box.x < margin) {
        const float shift = margin - cards.equipped.box.x;
        cards.equipped.box.x += shift;
        cards.candidate.box.x += shift;
    }
    const float right = static_cast<float>(screenWidth) - margin;
    if (cards.candidate.box.x + cards.candidate.box.width > right) {
        const float shift = cards.candidate.box.x + cards.candidate.box.width - right;
        cards.candidate.box.x -= shift;
        cards.equipped.box.x -= shift;
    }
    if (cards.equipped.box.x < margin) {
        cards.equipped.box.x = margin;
    }

    const auto clampY = [&](Rect& box) {
        const float bottom = static_cast<float>(screenHeight) - margin;
        if (box.y + box.height > bottom) {
            box.y = bottom - box.height;
        }
        if (box.y < margin) {
            box.y = margin;
        }
    };
    clampY(cards.candidate.box);
    cards.equipped.box.y = cards.candidate.box.y;
    clampY(cards.equipped.box);
    return cards;
}

HudChromeLayout computeHudChromeLayout(const UiScale& scale) noexcept {
    // The status HUD is the Diablo-style bottom console; the message strip floats above it.
    const HudConsoleLayout console = computeHudConsoleLayout(scale);
    HudChromeLayout layout{};
    layout.statusHud = console.panel;
    layout.messageStrip = console.messageStrip;
    return layout;
}

Rect townStageRect(const UiScale& scale) noexcept {
    const float width = static_cast<float>(std::max(scale.width, 1));
    const float height = static_cast<float>(std::max(scale.height, 1));
    const float aspect = width / std::max(height, 1.0F);
    constexpr float kTarget = 16.0F / 9.0F;
    if (std::abs(aspect - kTarget) <= 0.02F) {
        return {0.0F, 0.0F, width, height};
    }
    float stageW = width;
    float stageH = width / kTarget;
    if (stageH > height) {
        stageH = height;
        stageW = height * kTarget;
    }
    return {(width - stageW) * 0.5F, (height - stageH) * 0.5F, stageW, stageH};
}

TownSceneLayout computeTownSceneLayout(const UiScale& scale) noexcept {
    TownSceneLayout layout{};
    const float width = static_cast<float>(std::max(scale.width, 1));
    const float height = static_cast<float>(std::max(scale.height, 1));
    const float margin = std::max(8.0F, width * 0.018F);
    const float noticeH = std::clamp(std::max(40.0F, scale.minTouchTarget()), 40.0F, 56.0F);
    layout.notice = {margin, 4.0F, std::max(1.0F, width - margin * 2.0F), noticeH};
    const float exitW = std::min(148.0F, std::max(108.0F, scale.minTouchTarget() * 2.6F));
    layout.exitButton = {width - margin - exitW, layout.notice.y, exitW, noticeH};

    // Anchors measured from the 1280x720 plaza so pads, sprites, and resize stay locked.
    struct Anchor {
        float x;
        float y;
        float w;
        float h;
    };
    constexpr Anchor kForge{0.018F, 0.334F, 0.250F, 0.436F};
    constexpr Anchor kChapel{0.300F, 0.093F, 0.219F, 0.392F};
    constexpr Anchor kTavern{0.605F, 0.289F, 0.219F, 0.392F};
    constexpr Anchor kRoad{0.835F, 0.232F, 0.147F, 0.346F};
    const Rect stage = townStageRect(scale);
    const auto place = [&](const Anchor& anchor) {
        return Rect{
            stage.x + anchor.x * stage.width,
            stage.y + anchor.y * stage.height,
            anchor.w * stage.width,
            anchor.h * stage.height};
    };
    layout.blacksmith = place(kForge);
    layout.healer = place(kChapel);
    layout.tavern = place(kTavern);
    layout.road = place(kRoad);

    const float panelW = std::min(460.0F, std::max(160.0F, width - margin * 2.0F));
    const float panelH = std::min(260.0F, std::max(140.0F, height * 0.4F));
    layout.servicePanel = {(width - panelW) * 0.5F, std::max(8.0F, height * 0.22F), panelW, panelH};
    const float inset = 12.0F;
    layout.serviceTitle = {
        layout.servicePanel.x + inset,
        layout.servicePanel.y + 10.0F,
        std::max(1.0F, layout.servicePanel.width - inset * 2.0F),
        26.0F};
    const float serviceInnerW = std::max(1.0F, layout.servicePanel.width - inset * 2.0F);
    const float buttonH = std::min(std::max(36.0F, scale.minTouchTarget()), layout.servicePanel.height * 0.34F);
    const float dismissW = std::min(128.0F, std::max(72.0F, serviceInnerW * 0.38F));
    const float dismissH = std::max(36.0F, std::min(scale.minTouchTarget(), layout.servicePanel.height * 0.28F));
    layout.serviceClose = {
        layout.servicePanel.x + layout.servicePanel.width - inset - dismissW,
        layout.servicePanel.y + 8.0F,
        dismissW,
        dismissH};
    layout.serviceTitle.width = std::max(40.0F, layout.serviceClose.x - 8.0F - layout.serviceTitle.x);
    layout.serviceAction = {
        layout.servicePanel.x + inset,
        layout.servicePanel.y + layout.servicePanel.height - buttonH - 12.0F,
        serviceInnerW,
        buttonH};
    if (layout.serviceClose.x + layout.serviceClose.width > layout.servicePanel.x + layout.servicePanel.width - inset) {
        layout.serviceClose.width =
            std::max(1.0F, layout.servicePanel.x + layout.servicePanel.width - inset - layout.serviceClose.x);
    }
    layout.serviceBody = {
        layout.serviceTitle.x,
        layout.serviceTitle.y + layout.serviceTitle.height + 4.0F,
        layout.serviceTitle.width,
        std::max(12.0F, layout.serviceAction.y - (layout.serviceTitle.y + layout.serviceTitle.height) - 8.0F)};
    return layout;
}

Rect townBuildingArtRect(const Rect& hotspot) noexcept {
    const float cap = hotspot.height * 0.42F;
    const float captionH = std::min(hotspot.height * 0.2F, cap);
    return {hotspot.x, hotspot.y, hotspot.width, std::max(1.0F, hotspot.height - captionH - 8.0F)};
}

Rect townBuildingCaptionRect(const Rect& hotspot) noexcept {
    const Rect art = townBuildingArtRect(hotspot);
    const float width = std::min(std::max(48.0F, hotspot.width - 8.0F), 280.0F);
    const float height = std::max(24.0F, hotspot.y + hotspot.height - (art.y + art.height) - 6.0F);
    return {hotspot.x + (hotspot.width - width) * 0.5F, art.y + art.height + 4.0F, width, height};
}

Rect townOpaqueSpriteRect(
    const Rect& art,
    const float textureWidth,
    const float textureHeight,
    const float u0,
    const float v0,
    const float u1,
    const float v1) noexcept {
    const float spanU = std::clamp(u1, u0 + 0.01F, 1.0F) - std::clamp(u0, 0.0F, 1.0F);
    const float spanV = std::clamp(v1, v0 + 0.01F, 1.0F) - std::clamp(v0, 0.0F, 1.0F);
    const float pixelW = std::max(1.0F, spanU * std::max(textureWidth, 1.0F));
    const float pixelH = std::max(1.0F, spanV * std::max(textureHeight, 1.0F));
    const float aspect = pixelW / pixelH;
    float drawW = std::max(1.0F, art.width);
    float drawH = std::max(1.0F, art.height);
    if (art.width > 1.0F && art.height > 1.0F) {
        if ((art.width / art.height) > aspect) {
            drawH = art.height;
            drawW = std::min(art.width, drawH * aspect);
        } else {
            drawW = art.width;
            drawH = std::min(art.height, drawW / std::max(aspect, 0.01F));
        }
    }
    return {
        art.x + (art.width - drawW) * 0.5F,
        art.y + art.height - drawH,
        std::max(1.0F, drawW),
        std::max(1.0F, drawH)};
}

int townHotspotIndexAt(const TownSceneLayout& layout, const float x, const float y) noexcept {
    if (layout.blacksmith.contains(x, y)) {
        return 0;
    }
    if (layout.tavern.contains(x, y)) {
        return 1;
    }
    if (layout.healer.contains(x, y)) {
        return 2;
    }
    if (layout.road.contains(x, y)) {
        return 3;
    }
    return -1;
}

Rect townBuildingRect(const TownSceneLayout& layout, const int buildingIndex) noexcept {
    switch (buildingIndex) {
    case 0:
        return layout.blacksmith;
    case 1:
        return layout.tavern;
    case 2:
        return layout.healer;
    default:
        return {};
    }
}

} // namespace ui
