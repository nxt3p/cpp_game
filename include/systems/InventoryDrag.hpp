#pragma once

#include <cstdint>

namespace systems {

enum class DragOrigin : std::uint8_t {
    None,
    Inventory,
    Equipment,
};

enum class DragActionKind : std::uint8_t {
    None,
    Move,
    Swap,
    EquipFromInventory,
    UnequipToInventory,
    Unequip,
};

struct DragAction {
    DragActionKind kind{DragActionKind::None};
    int from{-1};
    int to{-1};
};

/// Pointer capture for inventory and paper-doll slots. Does not mutate bags; the caller applies `DragAction`.
class InventoryDrag {
public:
    void begin(DragOrigin origin, int index) noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool active() const noexcept { return origin_ != DragOrigin::None; }
    [[nodiscard]] DragOrigin origin() const noexcept { return origin_; }
    [[nodiscard]] int index() const noexcept { return index_; }

    /// `targetOccupied` is only consulted for inventory destinations.
    [[nodiscard]] DragAction release(DragOrigin targetOrigin, int targetIndex, bool targetOccupied) const noexcept;

private:
    DragOrigin origin_{DragOrigin::None};
    int index_{-1};
};

} // namespace systems
