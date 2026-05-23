#pragma once

#include "ui/UiTypes.hpp"

namespace ui {

class OverlayState {
public:
    OverlayState(int inventoryColumns, int inventoryRows);

    [[nodiscard]] const CharacterScreenData& characterScreen() const noexcept {
        return characterScreen_;
    }
    [[nodiscard]] CharacterScreenData& characterScreen() noexcept { return characterScreen_; }

    [[nodiscard]] const InventoryOverlayState& inventoryOverlay() const noexcept {
        return inventoryOverlay_;
    }
    [[nodiscard]] InventoryOverlayState& inventoryOverlay() noexcept { return inventoryOverlay_; }

    void showCharacterScreen(bool visible);
    void showInventoryOverlay(bool visible);
    void setInventorySlotVisible(int slotIndex, bool visible);
    void syncInventoryVisibility(int usedSlots);

private:
    CharacterScreenData characterScreen_{};
    InventoryOverlayState inventoryOverlay_{};
};

} // namespace ui
