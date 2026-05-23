#pragma once

#include "systems/Equipment.hpp"
#include "systems/ItemTypes.hpp"

#include <cstdint>
#include <string>

namespace systems {

enum class BlacksmithServiceKind : std::uint8_t {
    TemperWeapon = 0,
    ReinforceGear,
    ReforgeBackpack,
    MasterworkEquipped,
    SoulInfusion,
    Count
};

struct BlacksmithUnlockState {
    int lifetimeMobKills{0};
    int totalBossKills{0};
    int depth{1};
};

struct BlacksmithServiceDescriptor {
    const char* label{""};
    const char* shortHint{""};
    int requiredMobKills{0};
    int requiredBossKills{0};
    int requiredDepth{1};
};

struct BlacksmithResult {
    bool success{false};
    std::string message;
};

constexpr int kMaxBlacksmithUpgradeLevel = 5;

[[nodiscard]] BlacksmithServiceDescriptor serviceDescriptor(BlacksmithServiceKind service) noexcept;
[[nodiscard]] bool isBlacksmithServiceUnlocked(
    BlacksmithServiceKind service,
    const BlacksmithUnlockState& state) noexcept;
[[nodiscard]] std::string blacksmithUnlockHint(BlacksmithServiceKind service) noexcept;

[[nodiscard]] int blacksmithSellPrice(const ItemMetadata& item) noexcept;
[[nodiscard]] int blacksmithBuyPrice(const ItemMetadata& item) noexcept;

[[nodiscard]] int temperWeaponGoldCost(int currentUpgradeLevel) noexcept;
[[nodiscard]] int reinforceGearGoldCost(int currentUpgradeLevel) noexcept;
[[nodiscard]] int masterworkGoldCost(ItemRarity rarity) noexcept;
[[nodiscard]] int soulInfusionGoldCost(int currentUpgradeLevel) noexcept;
[[nodiscard]] int soulInfusionSoulCost(int currentUpgradeLevel) noexcept;

[[nodiscard]] BlacksmithResult temperEquippedWeapon(Equipment& equipment, int& playerGold);
[[nodiscard]] BlacksmithResult reinforceEquippedGear(Equipment& equipment, int& playerGold);
[[nodiscard]] int reforgeGoldCost() noexcept;

[[nodiscard]] BlacksmithResult reforgeBackpackItem(ItemMetadata& item, std::uint32_t seed, int& playerGold);
[[nodiscard]] BlacksmithResult masterworkEquippedWeapon(Equipment& equipment, int& playerGold);
[[nodiscard]] BlacksmithResult soulInfuseEquippedWeapon(
    Equipment& equipment,
    int& playerGold,
    int& carriedSouls);

} // namespace systems
