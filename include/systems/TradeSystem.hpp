#pragma once

#include "systems/Inventory.hpp"

#include <optional>
#include <string>

namespace systems {

struct TradeOffer {
    int playerSlotIndex{-1};
    int vendorSlotIndex{-1};
    int goldCost{0};
};

struct TradeResult {
    bool success{false};
    std::string message;
};

class TradeSystem {
public:
    TradeSystem(Inventory& playerInventory, Inventory& vendorInventory);

    void bindVendor(const std::string& vendorName, int vendorGold);
    void clearVendorBinding();

    [[nodiscard]] bool hasActiveVendor() const noexcept { return vendorName_.has_value(); }
    [[nodiscard]] int playerGold() const noexcept { return playerGold_; }
    [[nodiscard]] int vendorGold() const noexcept { return vendorGold_; }

    void setPlayerGold(int amount);
    TradeResult executeSwap(const TradeOffer& offer);
    TradeResult buyVendorSlot(int vendorSlotIndex, int goldCost);
    TradeResult sellPlayerSlot(int playerSlotIndex, int goldValue);

private:
    [[nodiscard]] bool canAfford(int goldCost) const noexcept;

    Inventory& playerInventory_;
    Inventory& vendorInventory_;
    std::optional<std::string> vendorName_;
    int playerGold_{0};
    int vendorGold_{0};
};

} // namespace systems
