#pragma once

#include "systems/Inventory.hpp"
#include "systems/ItemTypes.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string>

namespace systems {

enum class EquipmentSlotKind : std::uint8_t {
    Head = 0,
    Shoulders,
    Chest,
    Hands,
    Waist,
    Legs,
    Feet,
    Weapon,
    OffHand,
    Amulet,
    RingLeft,
    RingRight,
    Cloak,
    Charm,
    Relic,
    Count = 15
};

struct EquipmentActionResult {
    bool success{false};
    std::string message;
};

class Equipment {
public:
    [[nodiscard]] static EquipmentSlotKind slotForCategory(ItemCategory category) noexcept;
    [[nodiscard]] static EquipmentSlotKind resolveEquipSlot(
        ItemCategory category,
        const Equipment& equipment) noexcept;
    [[nodiscard]] static bool isEquippableCategory(ItemCategory category) noexcept;
    [[nodiscard]] static const char* slotLabel(EquipmentSlotKind slot) noexcept;
    [[nodiscard]] static char slotAbbreviation(EquipmentSlotKind slot) noexcept;

    [[nodiscard]] bool isSlotOccupied(EquipmentSlotKind slot) const noexcept;
    [[nodiscard]] const std::optional<ItemMetadata>& itemAt(EquipmentSlotKind slot) const noexcept;

    [[nodiscard]] EquipmentActionResult equipFromInventory(Inventory& inventory, int inventoryIndex);
    [[nodiscard]] EquipmentActionResult unequipToInventory(Inventory& inventory, EquipmentSlotKind slot);

    void clearAll() noexcept;
    void setSlot(EquipmentSlotKind slot, const ItemMetadata& item);
    bool modifySlot(EquipmentSlotKind slot, const std::function<void(ItemMetadata&)>& modifier);

private:
    std::array<std::optional<ItemMetadata>, static_cast<std::size_t>(EquipmentSlotKind::Count)> slots_{};
};

[[nodiscard]] ItemStatBonuses sumEquipmentBonuses(const Equipment& equipment);

} // namespace systems
