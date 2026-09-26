#include "systems/ItemStats.hpp"

#include "systems/CharacterCombat.hpp"
#include "systems/WeaponMastery.hpp"
#include "ui/UiTypes.hpp"

#include <cmath>
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
    case ItemCategory::Material:
        return "Material";
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
        if (item.rarity == ItemRarity::Magic) {
            item.bonuses = {2, 2, 1, 10, 0.05F, 1, 2.0F};
        } else if (item.rarity == ItemRarity::Rare) {
            item.bonuses = {3, 2, 2, 16, 0.08F, 2, 3.0F};
        } else if (item.rarity == ItemRarity::Legendary) {
            item.bonuses = {4, 3, 2, 20, 0.1F, 3, 4.0F};
        } else if (item.rarity == ItemRarity::Unique) {
            item.bonuses = {6, 5, 4, 40, 0.15F, 6, 6.0F};
        } else if (item.rarity == ItemRarity::Mythical) {
            item.bonuses = {8, 7, 6, 60, 0.22F, 9, 8.0F};
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

        if (item.category == ItemCategory::Consumable || item.category == ItemCategory::Material ||
            item.category == ItemCategory::Misc) {
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

    if (resolved.sockets > 0) {
        lines.push_back(std::to_string(resolved.sockets) + (resolved.sockets == 1 ? " empty socket" : " empty sockets"));
    }

    if (lines.empty() && resolved.category == ItemCategory::Consumable) {
        lines.push_back("Restores health when used (Q)");
    }
    if (lines.empty() && resolved.category == ItemCategory::Material) {
        lines.push_back("Crafting material — sells well");
    }
    if (lines.empty() && resolved.category == ItemCategory::Misc) {
        lines.push_back("Junk. Sell it.");
    }

    return lines;
}

namespace {

[[nodiscard]] ItemStatBonuses totalBonuses(const ItemMetadata& item) {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);
    ItemStatBonuses total = resolved.bonuses;
    const ItemStatBonuses mastery = weaponMasteryBonuses(resolved);
    total.damage += mastery.damage;
    total.attackSpeed += mastery.attackSpeed;
    return total;
}

void appendSignedLine(std::vector<std::string>& lines, const int delta, const char* label) {
    if (delta == 0) {
        return;
    }
    lines.push_back((delta > 0 ? "+" : "") + std::to_string(delta) + " " + label);
}

void appendSignedLine(std::vector<std::string>& lines, const float delta, const char* label) {
    if (delta > -0.001F && delta < 0.001F) {
        return;
    }
    std::ostringstream line;
    line.setf(std::ios::fixed);
    line.precision(2);
    line << (delta > 0.0F ? "+" : "") << delta << ' ' << label;
    lines.push_back(line.str());
}

} // namespace

ItemStatBonuses compareItemBonuses(const ItemMetadata& candidate, const ItemMetadata& equipped) {
    const ItemStatBonuses a = totalBonuses(candidate);
    const ItemStatBonuses b = totalBonuses(equipped);
    ItemStatBonuses delta{};
    delta.strength = a.strength - b.strength;
    delta.dexterity = a.dexterity - b.dexterity;
    delta.vitality = a.vitality - b.vitality;
    delta.maxHealth = a.maxHealth - b.maxHealth;
    delta.attackSpeed = a.attackSpeed - b.attackSpeed;
    delta.damage = a.damage - b.damage;
    delta.lightRadius = a.lightRadius - b.lightRadius;
    return delta;
}

std::vector<std::string> formatItemComparisonLines(
    const ItemMetadata& candidate,
    const ItemMetadata& equipped) {
    const ItemStatBonuses delta = compareItemBonuses(candidate, equipped);
    std::vector<std::string> lines;
    appendSignedLine(lines, delta.strength, "Strength");
    appendSignedLine(lines, delta.dexterity, "Dexterity");
    appendSignedLine(lines, delta.vitality, "Vitality");
    appendSignedLine(lines, delta.maxHealth, "Health");
    appendSignedLine(lines, delta.damage, "Damage");
    appendSignedLine(lines, delta.attackSpeed, "Attack Speed");
    appendSignedLine(lines, delta.lightRadius, "Light Radius");
    if (lines.empty()) {
        return lines;
    }
    lines.insert(lines.begin(), "vs " + equipped.name + ":");
    return lines;
}

int estimateWeaponDps(const ItemMetadata& item) {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);
    if (resolved.category != ItemCategory::Weapon && resolved.category != ItemCategory::OffHand) {
        return 0;
    }

    const float speed = std::max(0.45F, 0.85F + resolved.bonuses.attackSpeed);
    const float rarityScale = 3.0F + static_cast<float>(static_cast<int>(resolved.rarity)) * 2.0F;
    const float raw =
        (8.0F + static_cast<float>(resolved.bonuses.damage) + static_cast<float>(std::max(resolved.itemLevel, 1)) * 1.5F) *
        speed * rarityScale;
    return std::max(1, static_cast<int>(std::lround(raw)));
}

ItemTooltipCard buildItemTooltipCard(const ItemMetadata& item, const char* const banner) {
    ItemMetadata resolved = item;
    applyItemDefinition(resolved);

    ItemTooltipCard card{};
    card.rarity = resolved.rarity;
    if (banner != nullptr && banner[0] != '\0') {
        card.lines.push_back(TooltipLine{banner, TooltipTone::Meta});
    }

    card.lines.push_back(TooltipLine{resolved.name, TooltipTone::Title});

    std::ostringstream meta;
    meta << rarityLabel(resolved.rarity) << ' ' << itemCategoryLabel(resolved.category) << "  ilvl "
         << std::max(resolved.itemLevel, 1);
    if (resolved.upgradeLevel > 0) {
        meta << "  +" << resolved.upgradeLevel;
    }
    card.lines.push_back(TooltipLine{meta.str(), TooltipTone::Meta});

    if (resolved.category == ItemCategory::Weapon || resolved.category == ItemCategory::OffHand) {
        card.lines.push_back(TooltipLine{std::to_string(estimateWeaponDps(resolved)) + " DPS", TooltipTone::Headline});
        const float speed = std::max(0.45F, 0.85F + resolved.bonuses.attackSpeed);
        std::ostringstream speedLine;
        speedLine.setf(std::ios::fixed);
        speedLine.precision(2);
        speedLine << speed << " Weapon Speed";
        card.lines.push_back(TooltipLine{speedLine.str(), TooltipTone::Meta});
    }

    const auto pushStat = [&](const int value, const char* label, const TooltipTone tone) {
        if (value == 0) {
            return;
        }
        card.lines.push_back(TooltipLine{(value > 0 ? "+" : "") + std::to_string(value) + " " + label, tone});
    };

    pushStat(resolved.bonuses.strength, "Strength", TooltipTone::Attribute);
    pushStat(resolved.bonuses.dexterity, "Dexterity", TooltipTone::Attribute);
    pushStat(resolved.bonuses.vitality, "Vitality", TooltipTone::Attribute);
    pushStat(resolved.bonuses.maxHealth, "Health", TooltipTone::Attribute);
    pushStat(resolved.bonuses.damage, "Damage", TooltipTone::Effect);
    if (resolved.bonuses.attackSpeed > 0.001F) {
        std::ostringstream line;
        line << "+" << resolved.bonuses.attackSpeed << " Attack Speed";
        card.lines.push_back(TooltipLine{line.str(), TooltipTone::Attribute});
    }
    if (resolved.bonuses.lightRadius > 0.001F) {
        std::ostringstream line;
        line << "+" << resolved.bonuses.lightRadius << " Light Radius";
        card.lines.push_back(TooltipLine{line.str(), TooltipTone::Effect});
    }

    const ItemStatBonuses mastery = weaponMasteryBonuses(resolved);
    if (mastery.damage > 0) {
        card.lines.push_back(
            TooltipLine{"+" + std::to_string(mastery.damage) + " Mastery Damage", TooltipTone::Effect});
    }
    if (mastery.attackSpeed > 0.001F) {
        std::ostringstream line;
        line << "+" << mastery.attackSpeed << " Mastery Attack Speed";
        card.lines.push_back(TooltipLine{line.str(), TooltipTone::Effect});
    }
    const std::string masteryLine = formatWeaponMasteryLine(resolved);
    if (!masteryLine.empty()) {
        card.lines.push_back(TooltipLine{masteryLine, TooltipTone::Effect});
    }
    if (resolved.sockets > 0) {
        card.lines.push_back(TooltipLine{
            std::to_string(resolved.sockets) + (resolved.sockets == 1 ? " empty socket" : " empty sockets"),
            TooltipTone::Effect});
    }
    if (resolved.category == ItemCategory::Consumable) {
        card.lines.push_back(TooltipLine{"Restores health when used (Q)", TooltipTone::Effect});
    } else if (resolved.category == ItemCategory::Material) {
        card.lines.push_back(TooltipLine{"Crafting material — sells well", TooltipTone::Effect});
    } else if (resolved.category == ItemCategory::Misc && resolved.bonuses.damage == 0 && resolved.bonuses.strength == 0) {
        card.lines.push_back(TooltipLine{"Junk. Sell it.", TooltipTone::Meta});
    }

    if (resolved.value > 0) {
        card.lines.push_back(TooltipLine{"Price " + std::to_string(resolved.value) + " gold", TooltipTone::Footer});
    }
    return card;
}

std::string formatItemTooltip(const ItemMetadata& item) {
    const ItemTooltipCard card = buildItemTooltipCard(item, nullptr);
    std::ostringstream tooltip;
    for (std::size_t index = 0; index < card.lines.size(); ++index) {
        if (index > 0) {
            tooltip << '\n';
        }
        tooltip << card.lines[index].text;
    }
    return tooltip.str();
}

} // namespace systems
