#include "systems/TradeSystem.hpp"

#include <algorithm>

namespace systems {

TradeSystem::TradeSystem(Inventory& playerInventory, Inventory& vendorInventory)
    : playerInventory_(playerInventory), vendorInventory_(vendorInventory) {}

void TradeSystem::bindVendor(const std::string& vendorName, int vendorGold) {
    vendorName_ = vendorName;
    vendorGold_ = std::max(0, vendorGold);
}

void TradeSystem::clearVendorBinding() {
    vendorName_.reset();
    vendorGold_ = 0;
}

void TradeSystem::setPlayerGold(int amount) {
    playerGold_ = std::max(0, amount);
}

TradeResult TradeSystem::executeSwap(const TradeOffer& offer) {
    if (!hasActiveVendor()) {
        return TradeResult{false, "No vendor bound"};
    }

    if (!playerInventory_.isValidSlot(offer.playerSlotIndex) ||
        !vendorInventory_.isValidSlot(offer.vendorSlotIndex)) {
        return TradeResult{false, "Invalid trade slot"};
    }

    if (!playerInventory_.isSlotOccupied(offer.playerSlotIndex) ||
        !vendorInventory_.isSlotOccupied(offer.vendorSlotIndex)) {
        return TradeResult{false, "Both slots must contain items"};
    }

    if (offer.goldCost > 0 && !canAfford(offer.goldCost)) {
        return TradeResult{false, "Insufficient player gold"};
    }

    const ItemMetadata playerItem = *playerInventory_.slotAt(offer.playerSlotIndex).item;
    const ItemMetadata vendorItem = *vendorInventory_.slotAt(offer.vendorSlotIndex).item;

    if (!playerInventory_.discardAt(offer.playerSlotIndex) ||
        !vendorInventory_.discardAt(offer.vendorSlotIndex)) {
        return TradeResult{false, "Failed to clear trade slots"};
    }

    if (!playerInventory_.addItemAt(vendorItem, offer.playerSlotIndex).success ||
        !vendorInventory_.addItemAt(playerItem, offer.vendorSlotIndex).success) {
        return TradeResult{false, "Trade swap failed during item transfer"};
    }

    if (offer.goldCost > 0) {
        playerGold_ -= offer.goldCost;
        vendorGold_ += offer.goldCost;
    }

    return TradeResult{true, "Trade completed"};
}

TradeResult TradeSystem::buyVendorSlot(int vendorSlotIndex, int goldCost) {
    if (!hasActiveVendor()) {
        return TradeResult{false, "No vendor bound"};
    }
    if (!vendorInventory_.isSlotOccupied(vendorSlotIndex)) {
        return TradeResult{false, "Vendor slot is empty"};
    }
    if (!canAfford(goldCost)) {
        return TradeResult{false, "Insufficient player gold"};
    }

    const ItemMetadata item = *vendorInventory_.slotAt(vendorSlotIndex).item;
    const InventoryAddResult added = playerInventory_.addItem(item);
    if (!added.success) {
        return TradeResult{false, added.message};
    }

    if (!vendorInventory_.discardAt(vendorSlotIndex)) {
        playerInventory_.discardAt(added.slotIndex);
        return TradeResult{false, "Failed to remove vendor item"};
    }

    playerGold_ -= goldCost;
    vendorGold_ += goldCost;
    return TradeResult{true, "Purchase completed"};
}

TradeResult TradeSystem::sellPlayerSlot(int playerSlotIndex, int goldValue) {
    if (!playerInventory_.isSlotOccupied(playerSlotIndex)) {
        return TradeResult{false, "Player slot is empty"};
    }
    if (goldValue < 0) {
        return TradeResult{false, "Invalid gold value"};
    }

    if (!playerInventory_.discardAt(playerSlotIndex)) {
        return TradeResult{false, "Failed to remove sold item"};
    }

    playerGold_ += goldValue;
    if (hasActiveVendor()) {
        vendorGold_ = std::max(0, vendorGold_ - goldValue / 2);
    }
    return TradeResult{true, "Sale completed"};
}

bool TradeSystem::canAfford(int goldCost) const noexcept {
    return goldCost >= 0 && playerGold_ >= goldCost;
}

} // namespace systems
