#pragma once

#include "ui/UiHitTest.hpp"
#include "ui/UiLayout.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ui {

enum class WidgetKind : std::uint8_t {
    None = 0,
    HudStatus,
    HudMessageStrip,
    Minimap,
    InventoryPanel,
    InventorySlot,
    EquipmentSlot,
    CharacterPanel,
    StatUpgradeButton,
    TradePlayerPanel,
    TradeVendorPanel,
    TradePlayerSellSlot,
    TradeVendorBuySlot,
    BlacksmithServiceButton,
};

struct HitRegion {
    WidgetKind kind{WidgetKind::None};
    Rect bounds{};
    int slotIndex{-1};
};

class UiInteractionRegistry {
public:
    void clear() noexcept;
    void push(WidgetKind kind, const Rect& bounds, int slotIndex = -1);

    [[nodiscard]] const HitRegion* hitTest(float mouseX, float mouseY) const noexcept;
    [[nodiscard]] std::optional<int> inventorySlotAt(float mouseX, float mouseY) const noexcept;
    [[nodiscard]] std::optional<int> equipmentSlotAt(float mouseX, float mouseY) const noexcept;
    [[nodiscard]] bool blocksWorldInput(float mouseX, float mouseY) const noexcept;

    [[nodiscard]] const std::vector<HitRegion>& regions() const noexcept { return regions_; }

private:
    std::vector<HitRegion> regions_;
};

struct InGameUiVisibility {
    bool inventoryVisible{false};
    bool characterVisible{false};
    bool trading{false};
};

void buildInGameHitRegions(
    const UiScale& scale,
    const InGameUiVisibility& visibility,
    int inventoryColumns,
    int inventoryRows,
    int tradePlayerColumns,
    int tradePlayerRows,
    int tradeVendorColumns,
    int tradeVendorRows,
    ScreenAnchor minimapAnchor,
    float minimapMarginX,
    float minimapMarginY,
    float minimapFrameSize,
    UiInteractionRegistry& registry);

} // namespace ui
