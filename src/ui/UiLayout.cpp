#include "ui/UiLayout.hpp"

#include "ui/HudConsoleLayout.hpp"

#include <algorithm>

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

    const float headerBottom = layout.titleBand.y + layout.titleBand.height + scale.dim(8.0F);
    const float leftWidth = panelW * 0.46F;
    const float portraitSize = scale.dim(72.0F);
    layout.portrait = {
        layout.panel.x + pad + scale.dim(8.0F),
        headerBottom,
        portraitSize,
        portraitSize};

    const float barHeight = scale.dim(12.0F);
    const float barWidth = std::max(scale.dim(80.0F), leftWidth - scale.dim(16.0F));
    float cursorY = layout.portrait.y + portraitSize + scale.dim(8.0F);
    layout.hpBar = {layout.panel.x + pad, cursorY, barWidth, barHeight};
    cursorY += barHeight + scale.dim(6.0F);
    layout.xpBar = {layout.panel.x + pad, cursorY, barWidth, barHeight};
    cursorY += barHeight + scale.dim(6.0F);
    layout.goldLabel = {layout.panel.x + pad, cursorY, barWidth, scale.dim(16.0F)};

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
    const float sectionGap = scale.dim(12.0F);
    const float left = panel.spellsPane.x + pad;
    const float width = std::max(scale.dim(40.0F), panel.spellsPane.width - pad * 2.0F);
    float cursorY = std::max(panel.goldLabel.y + panel.goldLabel.height, panel.portrait.y + panel.portrait.height) +
                    scale.dim(10.0F);
    const float bottom = panel.spellsPane.y + panel.spellsPane.height - scale.dim(6.0F);
    const float available = std::max(scale.dim(48.0F), bottom - cursorY);
    const float chrome = headerH * 3.0F + sectionGap * 3.0F + gap;
    float icon = scale.dim(40.0F);
    const float needed = chrome + icon * 3.0F;
    if (needed > available) {
        icon = std::max(scale.dim(22.0F), (available - chrome) / 3.0F);
    }

    const auto place = [&](AbilitySpellLayout& section, const int count) {
        section.header = {left, cursorY, width, headerH};
        cursorY += headerH + scale.dim(4.0F);
        section.iconCount = count;
        for (int index = 0; index < count; ++index) {
            section.icons[static_cast<std::size_t>(index)] = {
                left + static_cast<float>(index) * (icon + gap),
                cursorY,
                icon,
                icon};
        }
        cursorY += icon + sectionGap;
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

TownSceneLayout computeTownSceneLayout(const UiScale& scale) noexcept {
    TownSceneLayout layout{};
    const float width = static_cast<float>(std::max(scale.width, 1));
    const float height = static_cast<float>(std::max(scale.height, 1));
    const float margin = std::max(8.0F, width * 0.018F);
    const float gap = std::max(6.0F, width * 0.012F);
    const float top = std::clamp(height * 0.08F, 24.0F, 72.0F);
    const float consoleBand = std::min(scale.dim(kReferenceHudConsoleHeight), height * 0.34F);
    const float consoleTop = height - consoleBand;
    const float roadH = std::clamp(std::max(36.0F, scale.minTouchTarget()), 36.0F, 72.0F);
    const float usableBottom = std::max(top + roadH + gap + 48.0F, std::min(consoleTop - 6.0F, height - 4.0F));
    const float buildingW = std::max(1.0F, (width - margin * 2.0F - gap * 2.0F) / 3.0F);
    float buildingH = usableBottom - top - gap - roadH;
    if (buildingH < 48.0F) {
        buildingH = std::max(32.0F, usableBottom - top - gap - 32.0F);
    }

    const float buildingY = top;
    layout.blacksmith = {margin, buildingY, buildingW, buildingH};
    layout.healer = {margin + buildingW + gap, buildingY, buildingW, buildingH};
    layout.tavern = {margin + (buildingW + gap) * 2.0F, buildingY, buildingW, buildingH};

    const float roadW = std::min(width - margin * 2.0F, std::max(buildingW, scale.minTouchTarget() * 3.0F));
    layout.road = {(width - roadW) * 0.5F, buildingY + buildingH + gap, roadW, roadH};
    layout.notice = {margin, 6.0F, std::max(1.0F, width - margin * 2.0F), std::max(16.0F, top - 12.0F)};

    const float panelW = std::min(460.0F, std::max(160.0F, width - margin * 2.0F));
    const float panelH = std::min(260.0F, std::max(140.0F, height * 0.4F));
    layout.servicePanel = {(width - panelW) * 0.5F, std::max(8.0F, height * 0.22F), panelW, panelH};
    const float inset = 12.0F;
    layout.serviceTitle = {
        layout.servicePanel.x + inset,
        layout.servicePanel.y + 10.0F,
        std::max(1.0F, layout.servicePanel.width - inset * 2.0F),
        26.0F};
    const float buttonGap = 10.0F;
    const float innerW = std::max(1.0F, layout.servicePanel.width - inset * 2.0F);
    const float buttonH = std::min(std::max(36.0F, scale.minTouchTarget()), layout.servicePanel.height * 0.34F);
    const float actionW = std::max(48.0F, (innerW - buttonGap) * 0.62F);
    const float closeW = std::max(36.0F, innerW - buttonGap - actionW);
    layout.serviceAction = {
        layout.servicePanel.x + inset,
        layout.servicePanel.y + layout.servicePanel.height - buttonH - 12.0F,
        std::min(actionW, innerW),
        buttonH};
    layout.serviceClose = {
        layout.serviceAction.x + layout.serviceAction.width + buttonGap,
        layout.serviceAction.y,
        closeW,
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
