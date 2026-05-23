#include "ui/UiInteraction.hpp"

#include "systems/Equipment.hpp"

namespace ui {

void UiInteractionRegistry::clear() noexcept {
    regions_.clear();
}

void UiInteractionRegistry::push(const WidgetKind kind, const Rect& bounds, const int slotIndex) {
    regions_.push_back(HitRegion{kind, bounds, slotIndex});
}

const HitRegion* UiInteractionRegistry::hitTest(const float mouseX, const float mouseY) const noexcept {
    for (auto iterator = regions_.rbegin(); iterator != regions_.rend(); ++iterator) {
        if (iterator->bounds.contains(mouseX, mouseY)) {
            return &(*iterator);
        }
    }
    return nullptr;
}

std::optional<int> UiInteractionRegistry::inventorySlotAt(
    const float mouseX,
    const float mouseY) const noexcept {
    const HitRegion* region = hitTest(mouseX, mouseY);
    if (region == nullptr || region->kind != WidgetKind::InventorySlot) {
        return std::nullopt;
    }
    return region->slotIndex;
}

std::optional<int> UiInteractionRegistry::equipmentSlotAt(
    const float mouseX,
    const float mouseY) const noexcept {
    const HitRegion* region = hitTest(mouseX, mouseY);
    if (region == nullptr || region->kind != WidgetKind::EquipmentSlot) {
        return std::nullopt;
    }
    return region->slotIndex;
}

bool UiInteractionRegistry::blocksWorldInput(const float mouseX, const float mouseY) const noexcept {
    const HitRegion* region = hitTest(mouseX, mouseY);
    return region != nullptr && region->kind != WidgetKind::None;
}

void buildInGameHitRegions(
    const UiScale& scale,
    const InGameUiVisibility& visibility,
    const int inventoryColumns,
    const int inventoryRows,
    const int tradePlayerColumns,
    const int tradePlayerRows,
    const int tradeVendorColumns,
    const int tradeVendorRows,
    const ScreenAnchor minimapAnchor,
    const float minimapMarginX,
    const float minimapMarginY,
    const float minimapFrameSize,
    UiInteractionRegistry& registry) {
    registry.clear();

    const HudChromeLayout hud = computeHudChromeLayout(scale);
    registry.push(WidgetKind::HudStatus, hud.statusHud);
    registry.push(WidgetKind::HudMessageStrip, hud.messageStrip);

    const MinimapWidgetLayout minimap = computeMinimapWidgetLayout(
        scale, minimapAnchor, minimapMarginX, minimapMarginY, minimapFrameSize);
    registry.push(WidgetKind::Minimap, minimap.frame);

    if (visibility.inventoryVisible) {
        const InventoryPaperDollLayout inventory =
            computeInventoryPaperDollLayout(scale, inventoryColumns, inventoryRows);
        registry.push(WidgetKind::InventoryPanel, inventory.panel);
        for (int equipmentIndex = 0;
             equipmentIndex < static_cast<int>(systems::EquipmentSlotKind::Count);
             ++equipmentIndex) {
            registry.push(
                WidgetKind::EquipmentSlot,
                inventory.equipmentSlotRect(equipmentIndex),
                equipmentIndex);
        }
        for (int index = 0; index < inventory.bagColumns * inventory.bagRows; ++index) {
            registry.push(WidgetKind::InventorySlot, inventory.inventorySlotRect(index), index);
        }
    }

    if (visibility.characterVisible) {
        const CharacterPanelLayout character = computeCharacterPanelLayout(scale);
        registry.push(WidgetKind::CharacterPanel, character.panel);
        registry.push(WidgetKind::StatUpgradeButton, character.upgradeStrengthButton, 0);
        registry.push(WidgetKind::StatUpgradeButton, character.upgradeDexterityButton, 1);
        registry.push(WidgetKind::StatUpgradeButton, character.upgradeVitalityButton, 2);
    }

    if (visibility.trading) {
        const TradeWindowLayout trade = computeTradeWindowLayout(scale);
        registry.push(WidgetKind::TradePlayerPanel, trade.playerPanel);
        registry.push(WidgetKind::TradeVendorPanel, trade.vendorPanel);
        for (int index = 0; index < tradePlayerColumns * tradePlayerRows; ++index) {
            registry.push(WidgetKind::TradePlayerSellSlot, trade.playerSlotRect(index), index);
        }
        for (int index = 0; index < tradeVendorColumns * tradeVendorRows; ++index) {
            registry.push(WidgetKind::TradeVendorBuySlot, trade.vendorSlotRect(index), index);
        }
        for (int serviceIndex = 0; serviceIndex < TradeWindowLayout::kServiceCount; ++serviceIndex) {
            registry.push(
                WidgetKind::BlacksmithServiceButton,
                trade.serviceButtonRect(serviceIndex),
                serviceIndex);
        }
    }
}

} // namespace ui
