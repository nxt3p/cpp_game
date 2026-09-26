#include "systems/InventoryDrag.hpp"

namespace systems {

void InventoryDrag::begin(const DragOrigin origin, const int index) noexcept {
    if (origin == DragOrigin::None || index < 0) {
        cancel();
        return;
    }
    origin_ = origin;
    index_ = index;
}

void InventoryDrag::cancel() noexcept {
    origin_ = DragOrigin::None;
    index_ = -1;
}

DragAction InventoryDrag::release(
    const DragOrigin targetOrigin,
    const int targetIndex,
    const bool targetOccupied) const noexcept {
    DragAction action{};
    action.from = index_;
    action.to = targetIndex;
    if (!active() || targetOrigin == DragOrigin::None || targetIndex < 0) {
        return action;
    }

    if (origin_ == DragOrigin::Inventory && targetOrigin == DragOrigin::Inventory) {
        if (targetIndex == index_) {
            action.kind = DragActionKind::EquipFromInventory;
            return action;
        }
        action.kind = targetOccupied ? DragActionKind::Swap : DragActionKind::Move;
        return action;
    }

    if (origin_ == DragOrigin::Inventory && targetOrigin == DragOrigin::Equipment) {
        action.kind = DragActionKind::EquipFromInventory;
        return action;
    }

    if (origin_ == DragOrigin::Equipment && targetOrigin == DragOrigin::Inventory && !targetOccupied) {
        action.kind = DragActionKind::UnequipToInventory;
        return action;
    }

    if (origin_ == DragOrigin::Equipment && targetOrigin == DragOrigin::Equipment && targetIndex == index_) {
        action.kind = DragActionKind::Unequip;
        return action;
    }

    return action;
}

} // namespace systems
