#include "systems/Inventory.hpp"

#include "systems/ItemStats.hpp"

#include <stdexcept>

namespace systems {

Inventory::Inventory(int columns, int rows)
    : columns_(columns), rows_(rows), slots_(static_cast<std::size_t>(columns * rows)) {}

int Inventory::usedSlots() const noexcept {
    int count = 0;
    for (const InventorySlot& slot : slots_) {
        if (slot.item.has_value()) {
            ++count;
        }
    }
    return count;
}

const InventorySlot& Inventory::slotAt(int index) const {
    if (!isValidSlot(index)) {
        throw std::out_of_range("Inventory slot index out of range");
    }
    return slots_[static_cast<std::size_t>(index)];
}

bool Inventory::isSlotOccupied(int index) const {
    return slotAt(index).item.has_value();
}

bool Inventory::canPlaceAt(int index) const {
    return isValidSlot(index) && !isSlotOccupied(index);
}

bool Inventory::isValidSlot(int index) const noexcept {
    return index >= 0 && index < capacity();
}

InventoryAddResult Inventory::addItem(const ItemMetadata& item) {
    const std::optional<int> freeSlot = firstFreeSlot();
    if (!freeSlot.has_value()) {
        return InventoryAddResult{false, -1, "Inventory is full"};
    }
    return addItemAt(item, *freeSlot);
}

InventoryAddResult Inventory::addItemAt(const ItemMetadata& item, int index) {
    if (!isValidSlot(index)) {
        return InventoryAddResult{false, -1, "Invalid inventory slot"};
    }
    if (slots_[static_cast<std::size_t>(index)].item.has_value()) {
        return InventoryAddResult{false, -1, "Slot already occupied"};
    }

    ItemMetadata stored = item;
    applyItemDefinition(stored);
    slots_[static_cast<std::size_t>(index)].item = std::move(stored);
    return InventoryAddResult{true, index, "Item added"};
}

bool Inventory::discardAt(int index) {
    if (!isValidSlot(index) || !isSlotOccupied(index)) {
        return false;
    }
    slots_[static_cast<std::size_t>(index)].item.reset();
    return true;
}

bool Inventory::moveItem(int fromIndex, int toIndex) {
    if (!isValidSlot(fromIndex) || !isValidSlot(toIndex)) {
        return false;
    }
    if (!isSlotOccupied(fromIndex) || isSlotOccupied(toIndex)) {
        return false;
    }

    slots_[static_cast<std::size_t>(toIndex)].item =
        std::move(slots_[static_cast<std::size_t>(fromIndex)].item);
    slots_[static_cast<std::size_t>(fromIndex)].item.reset();
    return true;
}

bool Inventory::modifyAt(int index, const std::function<void(ItemMetadata&)>& modifier) {
    if (!isValidSlot(index) || !isSlotOccupied(index) || modifier == nullptr) {
        return false;
    }
    modifier(*slots_[static_cast<std::size_t>(index)].item);
    applyItemDefinition(*slots_[static_cast<std::size_t>(index)].item);
    return true;
}

int Inventory::indexForCell(int column, int row) const noexcept {
    return row * columns_ + column;
}

std::optional<int> Inventory::firstFreeSlot() const {
    for (int index = 0; index < capacity(); ++index) {
        if (!slots_[static_cast<std::size_t>(index)].item.has_value()) {
            return index;
        }
    }
    return std::nullopt;
}

void Inventory::clearAll() noexcept {
    for (InventorySlot& slot : slots_) {
        slot.item.reset();
    }
}

void Inventory::applySavedSlots(const std::vector<InventorySlot>& slots) {
    if (static_cast<int>(slots.size()) != capacity()) {
        return;
    }
    slots_ = slots;
    for (InventorySlot& slot : slots_) {
        if (slot.item.has_value()) {
            applyItemDefinition(*slot.item);
        }
    }
}

} // namespace systems
