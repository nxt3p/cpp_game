#include "systems/Blacksmith.hpp"

#include "systems/ItemGenerator.hpp"
#include "systems/ItemStats.hpp"

#include <algorithm>
#include <sstream>

namespace systems {

namespace {

[[nodiscard]] bool isArmorCategory(ItemCategory category) noexcept {
    switch (category) {
    case ItemCategory::Head:
    case ItemCategory::Shoulders:
    case ItemCategory::Chest:
    case ItemCategory::Hands:
    case ItemCategory::Waist:
    case ItemCategory::Legs:
    case ItemCategory::Feet:
    case ItemCategory::Cloak:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::optional<EquipmentSlotKind> firstOccupiedArmorSlot(const Equipment& equipment) {
    static constexpr EquipmentSlotKind kArmorSlots[] = {
        EquipmentSlotKind::Chest,
        EquipmentSlotKind::Head,
        EquipmentSlotKind::Shoulders,
        EquipmentSlotKind::Hands,
        EquipmentSlotKind::Legs,
        EquipmentSlotKind::Feet,
        EquipmentSlotKind::Cloak,
    };

    for (const EquipmentSlotKind slot : kArmorSlots) {
        if (equipment.isSlotOccupied(slot)) {
            return slot;
        }
    }
    return std::nullopt;
}

void appendUpgradeSuffix(ItemMetadata& item) {
    if (item.upgradeLevel <= 0) {
        return;
    }

    const std::string suffix = " +" + std::to_string(item.upgradeLevel);
    if (item.name.find(suffix) == std::string::npos) {
        item.name += suffix;
    }
}

[[nodiscard]] ItemRarity nextRarity(ItemRarity rarity) noexcept {
    switch (rarity) {
    case ItemRarity::Common:
        return ItemRarity::Rare;
    case ItemRarity::Rare:
        return ItemRarity::Legendary;
    case ItemRarity::Legendary:
        return ItemRarity::Legendary;
    }
    return ItemRarity::Common;
}

} // namespace

BlacksmithServiceDescriptor serviceDescriptor(const BlacksmithServiceKind service) noexcept {
    switch (service) {
    case BlacksmithServiceKind::TemperWeapon:
        return {"Temper Blade", "+Damage on equipped weapon", 8, 0, 1};
    case BlacksmithServiceKind::ReinforceGear:
        return {"Reinforce Armor", "+Stats on equipped armor", 20, 0, 1};
    case BlacksmithServiceKind::ReforgeBackpack:
        return {"Reforge Item", "Reroll stats on bag item", 0, 1, 1};
    case BlacksmithServiceKind::MasterworkEquipped:
        return {"Masterwork", "Upgrade weapon rarity", 0, 3, 1};
    case BlacksmithServiceKind::SoulInfusion:
        return {"Soul Bind", "Souls empower weapon", 40, 0, 2};
    case BlacksmithServiceKind::Count:
        break;
    }
    return {"Unknown", "", 9999, 9999, 99};
}

bool isBlacksmithServiceUnlocked(
    const BlacksmithServiceKind service,
    const BlacksmithUnlockState& state) noexcept {
    const BlacksmithServiceDescriptor descriptor = serviceDescriptor(service);
    return state.lifetimeMobKills >= descriptor.requiredMobKills &&
           state.totalBossKills >= descriptor.requiredBossKills &&
           state.depth >= descriptor.requiredDepth;
}

std::string blacksmithUnlockHint(const BlacksmithServiceKind service) noexcept {
    const BlacksmithServiceDescriptor descriptor = serviceDescriptor(service);
    std::ostringstream hint;
    hint << "Locked: ";
    bool needsSeparator = false;
    if (descriptor.requiredMobKills > 0) {
        hint << descriptor.requiredMobKills << " mob kills";
        needsSeparator = true;
    }
    if (descriptor.requiredBossKills > 0) {
        if (needsSeparator) {
            hint << ", ";
        }
        hint << descriptor.requiredBossKills << " boss kills";
        needsSeparator = true;
    }
    if (descriptor.requiredDepth > 1) {
        if (needsSeparator) {
            hint << ", ";
        }
        hint << "depth " << descriptor.requiredDepth;
    }
    return hint.str();
}

int blacksmithSellPrice(const ItemMetadata& item) noexcept {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);
    const int base = std::max(1, resolved.value);
    return std::max(1, base * 6 / 10);
}

int blacksmithBuyPrice(const ItemMetadata& item) noexcept {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);
    return std::max(1, resolved.value);
}

int temperWeaponGoldCost(const int currentUpgradeLevel) noexcept {
    return 25 + currentUpgradeLevel * 18;
}

int reinforceGearGoldCost(const int currentUpgradeLevel) noexcept {
    return 35 + currentUpgradeLevel * 22;
}

int masterworkGoldCost(const ItemRarity rarity) noexcept {
    switch (rarity) {
    case ItemRarity::Common:
        return 120;
    case ItemRarity::Rare:
        return 280;
    case ItemRarity::Legendary:
        return 0;
    }
    return 9999;
}

int soulInfusionGoldCost(const int currentUpgradeLevel) noexcept {
    return 40 + currentUpgradeLevel * 25;
}

int soulInfusionSoulCost(const int currentUpgradeLevel) noexcept {
    return 35 + currentUpgradeLevel * 20;
}

BlacksmithResult temperEquippedWeapon(Equipment& equipment, int& playerGold) {
    if (!equipment.isSlotOccupied(EquipmentSlotKind::Weapon)) {
        return {false, "Equip a weapon first"};
    }

    const std::optional<ItemMetadata>& weapon = equipment.itemAt(EquipmentSlotKind::Weapon);
    if (!weapon.has_value()) {
        return {false, "Equip a weapon first"};
    }

    ItemMetadata resolved = *weapon;
    applyItemDefinition(resolved);
    if (resolved.category != ItemCategory::Weapon) {
        return {false, "Temper only works on weapons"};
    }
    if (resolved.upgradeLevel >= kMaxBlacksmithUpgradeLevel) {
        return {false, "Weapon is fully tempered"};
    }

    const int cost = temperWeaponGoldCost(resolved.upgradeLevel);
    if (playerGold < cost) {
        return {false, "Need " + std::to_string(cost) + " gold to temper"};
    }

    const bool modified = equipment.modifySlot(EquipmentSlotKind::Weapon, [&](ItemMetadata& item) {
        applyItemDefinition(item);
        ++item.upgradeLevel;
        item.bonuses.damage += 2;
        item.value += 8;
        appendUpgradeSuffix(item);
    });
    if (!modified) {
        return {false, "Failed to temper weapon"};
    }

    playerGold -= cost;
    return {true, "Tempered weapon (+2 damage)"};
}

BlacksmithResult reinforceEquippedGear(Equipment& equipment, int& playerGold) {
    const std::optional<EquipmentSlotKind> armorSlot = firstOccupiedArmorSlot(equipment);
    if (!armorSlot.has_value()) {
        return {false, "Equip armor first"};
    }

    const std::optional<ItemMetadata>& armor = equipment.itemAt(*armorSlot);
    if (!armor.has_value()) {
        return {false, "Equip armor first"};
    }

    ItemMetadata resolved = *armor;
    applyItemDefinition(resolved);
    if (!isArmorCategory(resolved.category)) {
        return {false, "Reinforce only works on armor"};
    }
    if (resolved.upgradeLevel >= kMaxBlacksmithUpgradeLevel) {
        return {false, "Armor is fully reinforced"};
    }

    const int cost = reinforceGearGoldCost(resolved.upgradeLevel);
    if (playerGold < cost) {
        return {false, "Need " + std::to_string(cost) + " gold to reinforce"};
    }

    const EquipmentSlotKind slot = *armorSlot;
    const bool modified = equipment.modifySlot(slot, [&](ItemMetadata& item) {
        applyItemDefinition(item);
        ++item.upgradeLevel;
        switch (item.upgradeLevel % 3) {
        case 1:
            item.bonuses.vitality += 1;
            break;
        case 2:
            item.bonuses.maxHealth += 4;
            break;
        default:
            item.bonuses.strength += 1;
            break;
        }
        item.value += 10;
        appendUpgradeSuffix(item);
    });
    if (!modified) {
        return {false, "Failed to reinforce armor"};
    }

    playerGold -= cost;
    return {true, "Reinforced " + std::string(Equipment::slotLabel(slot))};
}

int reforgeGoldCost() noexcept {
    return 75;
}

BlacksmithResult reforgeBackpackItem(ItemMetadata& item, const std::uint32_t seed, int& playerGold) {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);
    if (resolved.category == ItemCategory::Consumable || resolved.category == ItemCategory::Misc) {
        return {false, "Cannot reforge consumables"};
    }

    const int cost = reforgeGoldCost();
    if (playerGold < cost) {
        return {false, "Need " + std::to_string(cost) + " gold to reforge"};
    }

    ItemGenerator generator(seed);
    item = resolved;
    generator.rerollBonuses(item);
    playerGold -= cost;
    return {true, "Reforged " + item.name};
}

BlacksmithResult masterworkEquippedWeapon(Equipment& equipment, int& playerGold) {
    if (!equipment.isSlotOccupied(EquipmentSlotKind::Weapon)) {
        return {false, "Equip a weapon first"};
    }

    const std::optional<ItemMetadata>& weapon = equipment.itemAt(EquipmentSlotKind::Weapon);
    if (!weapon.has_value()) {
        return {false, "Equip a weapon first"};
    }

    ItemMetadata resolved = *weapon;
    applyItemDefinition(resolved);
    if (resolved.rarity == ItemRarity::Legendary) {
        return {false, "Weapon is already legendary"};
    }

    const int cost = masterworkGoldCost(resolved.rarity);
    if (cost <= 0) {
        return {false, "Weapon cannot be masterworked further"};
    }
    if (playerGold < cost) {
        return {false, "Need " + std::to_string(cost) + " gold for masterwork"};
    }

    const bool modified = equipment.modifySlot(EquipmentSlotKind::Weapon, [&](ItemMetadata& item) {
        applyItemDefinition(item);
        item.rarity = nextRarity(item.rarity);
        item.bonuses.damage += 3;
        item.bonuses.strength += 1;
        item.value = static_cast<int>(item.value * 1.35F) + 25;
        if (item.name.find("Masterwork") == std::string::npos) {
            item.name = "Masterwork " + item.name;
        }
    });
    if (!modified) {
        return {false, "Masterwork failed"};
    }

    playerGold -= cost;
    return {true, "Masterwork complete"};
}

BlacksmithResult soulInfuseEquippedWeapon(
    Equipment& equipment,
    int& playerGold,
    int& carriedSouls) {
    if (!equipment.isSlotOccupied(EquipmentSlotKind::Weapon)) {
        return {false, "Equip a weapon first"};
    }

    const std::optional<ItemMetadata>& weapon = equipment.itemAt(EquipmentSlotKind::Weapon);
    if (!weapon.has_value()) {
        return {false, "Equip a weapon first"};
    }

    ItemMetadata resolved = *weapon;
    applyItemDefinition(resolved);
    if (resolved.upgradeLevel >= kMaxBlacksmithUpgradeLevel) {
        return {false, "Weapon cannot absorb more souls"};
    }

    const int goldCost = soulInfusionGoldCost(resolved.upgradeLevel);
    const int soulCost = soulInfusionSoulCost(resolved.upgradeLevel);
    if (playerGold < goldCost) {
        return {false, "Need " + std::to_string(goldCost) + " gold for soul bind"};
    }
    if (carriedSouls < soulCost) {
        return {false, "Need " + std::to_string(soulCost) + " carried souls"};
    }

    const bool modified = equipment.modifySlot(EquipmentSlotKind::Weapon, [&](ItemMetadata& item) {
        applyItemDefinition(item);
        ++item.upgradeLevel;
        ++item.itemLevel;
        item.bonuses.damage += 3;
        item.bonuses.attackSpeed += 0.05F;
        item.value += 20;
        if (item.name.find("Soulbound") == std::string::npos) {
            item.name = "Soulbound " + item.name;
        }
    });
    if (!modified) {
        return {false, "Soul bind failed"};
    }

    playerGold -= goldCost;
    carriedSouls -= soulCost;
    return {true, "Soul bound weapon (+3 damage, +speed)"};
}

} // namespace systems
