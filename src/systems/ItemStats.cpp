#include "systems/ItemStats.hpp"

#include "systems/CharacterCombat.hpp"
#include "systems/WeaponMastery.hpp"
#include "ui/UiTypes.hpp"

#include <sstream>

namespace systems {

namespace {

void setDefinition(
    ItemMetadata& item,
    ItemCategory category,
    char iconLetter,
    ItemStatBonuses bonuses) {
    item.category = category;
    item.iconLetter = iconLetter;
    item.bonuses = bonuses;
}

} // namespace

const char* itemCategoryLabel(const ItemCategory category) noexcept {
    switch (category) {
    case ItemCategory::Head:
        return "Head";
    case ItemCategory::Shoulders:
        return "Shoulders";
    case ItemCategory::Chest:
        return "Chest";
    case ItemCategory::Hands:
        return "Hands";
    case ItemCategory::Waist:
        return "Waist";
    case ItemCategory::Legs:
        return "Legs";
    case ItemCategory::Feet:
        return "Feet";
    case ItemCategory::Weapon:
        return "Weapon";
    case ItemCategory::OffHand:
        return "Off-Hand";
    case ItemCategory::Amulet:
        return "Amulet";
    case ItemCategory::Ring:
        return "Ring";
    case ItemCategory::Cloak:
        return "Cloak";
    case ItemCategory::Charm:
        return "Charm";
    case ItemCategory::Relic:
        return "Relic";
    case ItemCategory::Consumable:
        return "Consumable";
    case ItemCategory::Misc:
        return "Misc";
    }
    return "Item";
}

void applyItemDefinition(ItemMetadata& item) {
    if (item.itemId >= 4000U) {
        if (item.iconLetter == '?') {
            item.iconLetter = item.name.empty() ? '?' : item.name.front();
        }
        return;
    }

    switch (item.itemId) {
    case 101U:
        setDefinition(item, ItemCategory::Weapon, 'S', {4, 0, 0, 0, 0.0F, 2, 0.0F});
        break;
    case 102U:
        setDefinition(item, ItemCategory::Consumable, 'H', {0, 0, 0, 25, 0.0F, 0, 0.0F});
        break;
    case 201U:
        setDefinition(item, ItemCategory::Charm, 'C', {0, 3, 0, 0, 0.15F, 0, 5.0F});
        break;
    case 301U:
        setDefinition(item, ItemCategory::Weapon, 'F', {6, 2, 0, 0, 0.1F, 4, 2.0F});
        break;
    case 302U:
        setDefinition(item, ItemCategory::Chest, 'A', {0, 0, 5, 30, 0.0F, 0, 3.0F});
        break;
    case 401U:
        setDefinition(item, ItemCategory::Relic, 'D', {2, 2, 2, 20, 0.2F, 3, 8.0F});
        break;
    default:
        if (item.iconLetter == '?') {
            item.iconLetter = item.name.empty() ? '?' : item.name.front();
        }
        if (item.rarity == ItemRarity::Rare) {
            item.bonuses = {2, 2, 1, 10, 0.05F, 1, 2.0F};
        } else if (item.rarity == ItemRarity::Legendary) {
            item.bonuses = {4, 3, 2, 20, 0.1F, 3, 4.0F};
        } else {
            item.bonuses = {1, 0, 0, 0, 0.0F, 0, 0.0F};
        }
        break;
    }
}

ItemStatBonuses sumInventoryBonuses(const Inventory& inventory) {
    ItemStatBonuses total{};
    for (int index = 0; index < inventory.capacity(); ++index) {
        if (!inventory.isSlotOccupied(index)) {
            continue;
        }

        ItemMetadata item = *inventory.slotAt(index).item;
        applyItemDefinition(item);

        if (item.category == ItemCategory::Consumable) {
            continue;
        }

        total.strength += item.bonuses.strength;
        total.dexterity += item.bonuses.dexterity;
        total.vitality += item.bonuses.vitality;
        total.maxHealth += item.bonuses.maxHealth;
        total.attackSpeed += item.bonuses.attackSpeed;
        total.damage += item.bonuses.damage;
        total.lightRadius += item.bonuses.lightRadius;
    }
    return total;
}

EffectiveCharacterStats computeEffectiveStats(
    const ui::CharacterScreenData& baseStats,
    const Equipment& equipment) {
    const ItemStatBonuses gear = sumEquipmentBonuses(equipment);

    EffectiveCharacterStats effective{};
    effective.strength = baseStats.strength + gear.strength;
    effective.dexterity = baseStats.dexterity + gear.dexterity;
    effective.vitality = baseStats.vitality + gear.vitality;
    effective.maxHealth = 50 + effective.vitality * 5 + gear.maxHealth;

    const CombatStatInput combat{
        baseStats.level, effective.strength + gear.damage, effective.dexterity};
    effective.damage = computeDamage(combat);
    effective.attacksPerSecond = computeAttacksPerSecond(combat) + gear.attackSpeed;
    effective.lightRadius = kBaseLightRadius + gear.lightRadius;

    return effective;
}

std::vector<std::string> formatItemStatLines(const ItemMetadata& item) {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);

    std::vector<std::string> lines;
    if (resolved.bonuses.strength != 0) {
        lines.push_back("+" + std::to_string(resolved.bonuses.strength) + " Strength");
    }
    if (resolved.bonuses.dexterity != 0) {
        lines.push_back("+" + std::to_string(resolved.bonuses.dexterity) + " Dexterity");
    }
    if (resolved.bonuses.vitality != 0) {
        lines.push_back("+" + std::to_string(resolved.bonuses.vitality) + " Vitality");
    }
    if (resolved.bonuses.maxHealth != 0) {
        lines.push_back("+" + std::to_string(resolved.bonuses.maxHealth) + " Health");
    }
    if (resolved.bonuses.damage != 0) {
        lines.push_back("+" + std::to_string(resolved.bonuses.damage) + " Damage");
    }
    if (resolved.bonuses.attackSpeed > 0.001F) {
        std::ostringstream line;
        line << "+" << resolved.bonuses.attackSpeed << " Attack Speed";
        lines.push_back(line.str());
    }
    if (resolved.bonuses.lightRadius > 0.001F) {
        std::ostringstream line;
        line << "+" << resolved.bonuses.lightRadius << " Light Radius";
        lines.push_back(line.str());
    }

    const ItemStatBonuses mastery = weaponMasteryBonuses(resolved);
    if (mastery.damage > 0) {
        lines.push_back("+" + std::to_string(mastery.damage) + " Mastery Damage");
    }
    if (mastery.attackSpeed > 0.001F) {
        std::ostringstream line;
        line << "+" << mastery.attackSpeed << " Mastery Attack Speed";
        lines.push_back(line.str());
    }

    const std::string masteryLine = formatWeaponMasteryLine(resolved);
    if (!masteryLine.empty()) {
        lines.push_back(masteryLine);
    }

    if (lines.empty() && resolved.category == ItemCategory::Consumable) {
        lines.push_back("Restores health when used");
    }

    return lines;
}

std::string formatItemTooltip(const ItemMetadata& item) {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);

    std::ostringstream tooltip;
    tooltip << resolved.name << '\n';
    tooltip << rarityLabel(resolved.rarity) << " " << itemCategoryLabel(resolved.category);
    if (resolved.itemLevel > 1) {
        tooltip << "  (Lv " << resolved.itemLevel << ')';
    }
    if (resolved.upgradeLevel > 0) {
        tooltip << "  [+" << resolved.upgradeLevel << ']';
    }
    for (const std::string& line : formatItemStatLines(resolved)) {
        tooltip << '\n' << line;
    }
    return tooltip.str();
}

} // namespace systems
