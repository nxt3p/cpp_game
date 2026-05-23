#include "ui/OverlayState.hpp"

namespace ui {

OverlayState::OverlayState(int inventoryColumns, int inventoryRows) {
    inventoryOverlay_.columns = inventoryColumns;
    inventoryOverlay_.rows = inventoryRows;
    inventoryOverlay_.slotVisible.assign(
        static_cast<std::size_t>(inventoryColumns * inventoryRows), false);
}

void OverlayState::showCharacterScreen(bool visible) {
    characterScreen_.visible = visible;
}

void OverlayState::showInventoryOverlay(bool visible) {
    inventoryOverlay_.visible = visible;
}

void OverlayState::setInventorySlotVisible(int slotIndex, bool visible) {
    if (slotIndex < 0 ||
        slotIndex >= static_cast<int>(inventoryOverlay_.slotVisible.size())) {
        return;
    }
    inventoryOverlay_.slotVisible[static_cast<std::size_t>(slotIndex)] = visible;
}

void OverlayState::syncInventoryVisibility(int usedSlots) {
    const int capacity = static_cast<int>(inventoryOverlay_.slotVisible.size());
    for (int index = 0; index < capacity; ++index) {
        inventoryOverlay_.slotVisible[static_cast<std::size_t>(index)] = index < usedSlots;
    }
}

} // namespace ui
