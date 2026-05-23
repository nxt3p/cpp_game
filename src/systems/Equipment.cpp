#include "systems/Equipment.hpp"

#include "systems/ItemStats.hpp"
#include "systems/WeaponMastery.hpp"

namespace systems {

EquipmentSlotKind Equipment::slotForCategory(const ItemCategory category) noexcept {
    switch (category) {
    case ItemCategory::Head:
        return EquipmentSlotKind::Head;
    case ItemCategory::Shoulders:
        return EquipmentSlotKind::Shoulders;
    case ItemCategory::Chest:
        return EquipmentSlotKind::Chest;
    case ItemCategory::Hands:
        return EquipmentSlotKind::Hands;
    case ItemCategory::Waist:
        return EquipmentSlotKind::Waist;
    case ItemCategory::Legs:
        return EquipmentSlotKind::Legs;
    case ItemCategory::Feet:
        return EquipmentSlotKind::Feet;
    case ItemCategory::Weapon:
        return EquipmentSlotKind::Weapon;
    case ItemCategory::OffHand:
        return EquipmentSlotKind::OffHand;
    case ItemCategory::Amulet:
        return EquipmentSlotKind::Amulet;
    case ItemCategory::Ring:
        return EquipmentSlotKind::RingLeft;
    case ItemCategory::Cloak:
        return EquipmentSlotKind::Cloak;
    case ItemCategory::Charm:
        return EquipmentSlotKind::Charm;
    case ItemCategory::Relic:
        return EquipmentSlotKind::Relic;
    default:
        return EquipmentSlotKind::Weapon;
    }
}

EquipmentSlotKind Equipment::resolveEquipSlot(
    const ItemCategory category,
    const Equipment& equipment) noexcept {
    if (category == ItemCategory::Ring) {
        if (!equipment.isSlotOccupied(EquipmentSlotKind::RingLeft)) {
            return EquipmentSlotKind::RingLeft;
        }
        if (!equipment.isSlotOccupied(EquipmentSlotKind::RingRight)) {
            return EquipmentSlotKind::RingRight;
        }
        return EquipmentSlotKind::RingLeft;
    }
    return slotForCategory(category);
}

bool Equipment::isEquippableCategory(const ItemCategory category) noexcept {
    switch (category) {
    case ItemCategory::Head:
    case ItemCategory::Shoulders:
    case ItemCategory::Chest:
    case ItemCategory::Hands:
    case ItemCategory::Waist:
    case ItemCategory::Legs:
    case ItemCategory::Feet:
    case ItemCategory::Weapon:
    case ItemCategory::OffHand:
    case ItemCategory::Amulet:
    case ItemCategory::Ring:
    case ItemCategory::Cloak:
    case ItemCategory::Charm:
    case ItemCategory::Relic:
        return true;
    default:
        return false;
    }
}

const char* Equipment::slotLabel(const EquipmentSlotKind slot) noexcept {
    switch (slot) {
    case EquipmentSlotKind::Head:
        return "Head";
    case EquipmentSlotKind::Shoulders:
        return "Shoulders";
    case EquipmentSlotKind::Chest:
        return "Chest";
    case EquipmentSlotKind::Hands:
        return "Hands";
    case EquipmentSlotKind::Waist:
        return "Waist";
    case EquipmentSlotKind::Legs:
        return "Legs";
    case EquipmentSlotKind::Feet:
        return "Feet";
    case EquipmentSlotKind::Weapon:
        return "Weapon";
    case EquipmentSlotKind::OffHand:
        return "Off-Hand";
    case EquipmentSlotKind::Amulet:
        return "Amulet";
    case EquipmentSlotKind::RingLeft:
        return "Ring (L)";
    case EquipmentSlotKind::RingRight:
        return "Ring (R)";
    case EquipmentSlotKind::Cloak:
        return "Cloak";
    case EquipmentSlotKind::Charm:
        return "Charm";
    case EquipmentSlotKind::Relic:
        return "Relic";
    case EquipmentSlotKind::Count:
        break;
    }
    return "Slot";
}

char Equipment::slotAbbreviation(const EquipmentSlotKind slot) noexcept {
    switch (slot) {
    case EquipmentSlotKind::Head:
        return 'H';
    case EquipmentSlotKind::Shoulders:
        return 'S';
    case EquipmentSlotKind::Chest:
        return 'C';
    case EquipmentSlotKind::Hands:
        return 'G';
    case EquipmentSlotKind::Waist:
        return 'B';
    case EquipmentSlotKind::Legs:
        return 'L';
    case EquipmentSlotKind::Feet:
        return 'F';
    case EquipmentSlotKind::Weapon:
        return 'W';
    case EquipmentSlotKind::OffHand:
        return 'O';
    case EquipmentSlotKind::Amulet:
        return 'A';
    case EquipmentSlotKind::RingLeft:
        return '1';
    case EquipmentSlotKind::RingRight:
        return '2';
    case EquipmentSlotKind::Cloak:
        return 'K';
    case EquipmentSlotKind::Charm:
        return 'M';
    case EquipmentSlotKind::Relic:
        return 'R';
    case EquipmentSlotKind::Count:
        break;
    }
    return '?';
}

bool Equipment::isSlotOccupied(const EquipmentSlotKind slot) const noexcept {
    return slots_[static_cast<std::size_t>(slot)].has_value();
}

const std::optional<ItemMetadata>& Equipment::itemAt(const EquipmentSlotKind slot) const noexcept {
    return slots_[static_cast<std::size_t>(slot)];
}

EquipmentActionResult Equipment::equipFromInventory(Inventory& inventory, const int inventoryIndex) {
    if (!inventory.isValidSlot(inventoryIndex) || !inventory.isSlotOccupied(inventoryIndex)) {
        return {false, "No item in that inventory slot"};
    }

    ItemMetadata incoming = *inventory.slotAt(inventoryIndex).item;
    applyItemDefinition(incoming);

    if (!isEquippableCategory(incoming.category)) {
        return {false, "That item cannot be equipped"};
    }

    const EquipmentSlotKind targetSlot = resolveEquipSlot(incoming.category, *this);
    const std::size_t slotIndex = static_cast<std::size_t>(targetSlot);
    std::optional<ItemMetadata> previous = slots_[slotIndex];

    if (!inventory.discardAt(inventoryIndex)) {
        return {false, "Failed to remove item from inventory"};
    }

    slots_[slotIndex] = std::move(incoming);

    if (previous.has_value()) {
        const InventoryAddResult returned = inventory.addItemAt(*previous, inventoryIndex);
        if (!returned.success) {
            const InventoryAddResult fallback = inventory.addItem(*previous);
            if (!fallback.success) {
                ItemMetadata rollback = std::move(*slots_[slotIndex]);
                slots_[slotIndex] = std::move(previous);
                inventory.addItemAt(rollback, inventoryIndex);
                return {false, "Inventory full — cannot swap equipped item"};
            }
        }
    }

    return {true, "Item equipped"};
}

EquipmentActionResult Equipment::unequipToInventory(Inventory& inventory, const EquipmentSlotKind slot) {
    if (!isSlotOccupied(slot)) {
        return {false, "Equipment slot is empty"};
    }

    ItemMetadata item = *itemAt(slot);
    const InventoryAddResult placed = inventory.addItem(item);
    if (!placed.success) {
        return {false, placed.message.empty() ? "Inventory full" : placed.message};
    }

    slots_[static_cast<std::size_t>(slot)].reset();
    return {true, "Item returned to inventory"};
}

void Equipment::clearAll() noexcept {
    for (std::optional<ItemMetadata>& slot : slots_) {
        slot.reset();
    }
}

void Equipment::setSlot(const EquipmentSlotKind slot, const ItemMetadata& item) {
    ItemMetadata stored = item;
    applyItemDefinition(stored);
    slots_[static_cast<std::size_t>(slot)] = std::move(stored);
}

bool Equipment::modifySlot(
    const EquipmentSlotKind slot,
    const std::function<void(ItemMetadata&)>& modifier) {
    if (!isSlotOccupied(slot) || !modifier) {
        return false;
    }

    ItemMetadata item = *itemAt(slot);
    modifier(item);
    applyItemDefinition(item);
    slots_[static_cast<std::size_t>(slot)] = std::move(item);
    return true;
}

ItemStatBonuses sumEquipmentBonuses(const Equipment& equipment) {
    ItemStatBonuses total{};
    for (std::size_t index = 0; index < static_cast<std::size_t>(EquipmentSlotKind::Count); ++index) {
        const EquipmentSlotKind slot = static_cast<EquipmentSlotKind>(index);
        if (!equipment.isSlotOccupied(slot)) {
            continue;
        }

        ItemMetadata item = *equipment.itemAt(slot);
        applyItemDefinition(item);

        total.strength += item.bonuses.strength;
        total.dexterity += item.bonuses.dexterity;
        total.vitality += item.bonuses.vitality;
        total.maxHealth += item.bonuses.maxHealth;
        total.attackSpeed += item.bonuses.attackSpeed;
        total.damage += item.bonuses.damage;
        total.lightRadius += item.bonuses.lightRadius;

        const ItemStatBonuses mastery = weaponMasteryBonuses(item);
        total.strength += mastery.strength;
        total.dexterity += mastery.dexterity;
        total.vitality += mastery.vitality;
        total.maxHealth += mastery.maxHealth;
        total.attackSpeed += mastery.attackSpeed;
        total.damage += mastery.damage;
        total.lightRadius += mastery.lightRadius;
    }
    return total;
}

} // namespace systems
