#pragma once

#include "systems/ItemTypes.hpp"

#include <functional>
#include <optional>
#include <vector>

namespace systems {

struct InventorySlot {
    std::optional<ItemMetadata> item;
};

struct InventoryAddResult {
    bool success{false};
    int slotIndex{-1};
    std::string message;
};

class Inventory {
public:
    explicit Inventory(int columns, int rows);

    [[nodiscard]] int columns() const noexcept { return columns_; }
    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int capacity() const noexcept { return static_cast<int>(slots_.size()); }
    [[nodiscard]] int usedSlots() const noexcept;

    [[nodiscard]] const InventorySlot& slotAt(int index) const;
    [[nodiscard]] bool isSlotOccupied(int index) const;
    [[nodiscard]] bool canPlaceAt(int index) const;
    [[nodiscard]] bool isValidSlot(int index) const noexcept;

    InventoryAddResult addItem(const ItemMetadata& item);
    InventoryAddResult addItemAt(const ItemMetadata& item, int index);
    bool discardAt(int index);
    bool moveItem(int fromIndex, int toIndex);
    bool modifyAt(int index, const std::function<void(ItemMetadata&)>& modifier);

    void clearAll() noexcept;
    void applySavedSlots(const std::vector<InventorySlot>& slots);

private:
    [[nodiscard]] int indexForCell(int column, int row) const noexcept;
    [[nodiscard]] std::optional<int> firstFreeSlot() const;

    int columns_{0};
    int rows_{0};
    std::vector<InventorySlot> slots_;
};

} // namespace systems
