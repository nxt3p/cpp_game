#include "systems/ItemGenerator.hpp"

#include "systems/ItemStats.hpp"

#include <algorithm>
#include <array>
#include <sstream>

namespace systems {

namespace {

constexpr const char* kPrefixes[] = {
    "Swift", "Heavy", "Sturdy", "Arcane", "Vital", "Savage", "Ancient", "Gilded"};
constexpr const char* kWeaponBases[] = {"Blade", "Axe", "Sword", "Mace", "Spear"};
constexpr const char* kHeadBases[] = {"Helm", "Hood", "Crown", "Mask"};
constexpr const char* kShoulderBases[] = {"Pads", "Spaulders", "Mantle", "Guard"};
constexpr const char* kChestBases[] = {"Vest", "Plate", "Mail", "Robe"};
constexpr const char* kHandBases[] = {"Gloves", "Gauntlets", "Wraps", "Grasp"};
constexpr const char* kWaistBases[] = {"Belt", "Sash", "Girdle", "Cord"};
constexpr const char* kLegBases[] = {"Greaves", "Leggings", "Chaps", "Breeches"};
constexpr const char* kFeetBases[] = {"Boots", "Sabatons", "Treads", "Shoes"};
constexpr const char* kOffHandBases[] = {"Shield", "Tome", "Orb", "Focus"};
constexpr const char* kAmuletBases[] = {"Amulet", "Pendant", "Collar", "Locket"};
constexpr const char* kRingBases[] = {"Ring", "Band", "Loop", "Signet"};
constexpr const char* kCloakBases[] = {"Cloak", "Cape", "Shroud", "Mantle"};
constexpr const char* kCharmBases[] = {"Charm", "Talisman", "Sigil", "Idol"};
constexpr const char* kRelicBases[] = {"Relic", "Artifact", "Keepsake", "Heirloom"};
constexpr const char* kSuffixes[] = {
    "of Strength",
    "of Dexterity",
    "of Vitality",
    "of Fury",
    "of Light",
    "of the Hunt",
    "of Endurance"};

constexpr ItemCategory kGearCategories[] = {
    ItemCategory::Head,
    ItemCategory::Shoulders,
    ItemCategory::Chest,
    ItemCategory::Hands,
    ItemCategory::Waist,
    ItemCategory::Legs,
    ItemCategory::Feet,
    ItemCategory::Weapon,
    ItemCategory::OffHand,
    ItemCategory::Amulet,
    ItemCategory::Ring,
    ItemCategory::Cloak,
    ItemCategory::Charm,
    ItemCategory::Relic,
};

int pickIndex(std::mt19937& rng, const int count) {
    std::uniform_int_distribution<int> distribution(0, count - 1);
    return distribution(rng);
}

} // namespace

ItemGenerator::ItemGenerator(const std::uint32_t seed) : rng_(seed) {}

void ItemGenerator::setSeed(const std::uint32_t seed) {
    rng_.seed(seed);
}

ItemCategory ItemGenerator::rollCategory() {
    return kGearCategories[pickIndex(rng_, static_cast<int>(std::size(kGearCategories)))];
}

char ItemGenerator::iconForCategory(const ItemCategory category) const noexcept {
    switch (category) {
    case ItemCategory::Head:
        return 'H';
    case ItemCategory::Shoulders:
        return 'S';
    case ItemCategory::Chest:
        return 'C';
    case ItemCategory::Hands:
        return 'G';
    case ItemCategory::Waist:
        return 'B';
    case ItemCategory::Legs:
        return 'L';
    case ItemCategory::Feet:
        return 'F';
    case ItemCategory::Weapon:
        return 'W';
    case ItemCategory::OffHand:
        return 'O';
    case ItemCategory::Amulet:
        return 'A';
    case ItemCategory::Ring:
        return 'R';
    case ItemCategory::Cloak:
        return 'K';
    case ItemCategory::Charm:
        return 'M';
    case ItemCategory::Relic:
        return 'X';
    default:
        return '?';
    }
}

ItemStatBonuses ItemGenerator::rollBonuses(
    const ItemCategory category,
    const ItemRarity rarity,
    const int itemLevel) {
    ItemStatBonuses bonuses{};
    const int levelScale = std::max(1, itemLevel);

    int rarityScale = 1;
    float speedScale = 0.05F;
    float lightScale = 1.0F;
    switch (rarity) {
    case ItemRarity::Rare:
        rarityScale = 2;
        speedScale = 0.1F;
        lightScale = 2.5F;
        break;
    case ItemRarity::Legendary:
        rarityScale = 4;
        speedScale = 0.18F;
        lightScale = 5.0F;
        break;
    default:
        break;
    }

    const int rollCount = rarityScale + (levelScale / 3);
    for (int roll = 0; roll < rollCount; ++roll) {
        std::uniform_int_distribution<int> statRoll(0, 5);
        switch (statRoll(rng_)) {
        case 0:
            bonuses.strength += 1 + levelScale / 2;
            break;
        case 1:
            bonuses.dexterity += 1 + levelScale / 3;
            break;
        case 2:
            bonuses.vitality += 1 + levelScale / 3;
            break;
        case 3:
            bonuses.damage += 1 + levelScale / 2;
            break;
        case 4:
            bonuses.maxHealth += 4 + levelScale * 2;
            break;
        default:
            bonuses.attackSpeed += speedScale;
            bonuses.lightRadius += lightScale;
            break;
        }
    }

    if (category == ItemCategory::Weapon || category == ItemCategory::OffHand) {
        bonuses.damage += levelScale + rarityScale;
        bonuses.strength += rarityScale;
    } else if (
        category == ItemCategory::Chest || category == ItemCategory::Head ||
        category == ItemCategory::Legs || category == ItemCategory::Feet ||
        category == ItemCategory::Hands || category == ItemCategory::Shoulders ||
        category == ItemCategory::Waist || category == ItemCategory::Cloak) {
        bonuses.vitality += levelScale;
        bonuses.maxHealth += levelScale * 4;
    } else {
        bonuses.lightRadius += lightScale * static_cast<float>(rarityScale);
        bonuses.dexterity += rarityScale;
    }

    return bonuses;
}

std::string ItemGenerator::buildName(
    const ItemCategory category,
    const ItemRarity rarity,
    const std::string& prefix,
    const std::string& suffix) {
    const char* const* bases = kCharmBases;
    int baseCount = static_cast<int>(std::size(kCharmBases));
    switch (category) {
    case ItemCategory::Head:
        bases = kHeadBases;
        baseCount = static_cast<int>(std::size(kHeadBases));
        break;
    case ItemCategory::Shoulders:
        bases = kShoulderBases;
        baseCount = static_cast<int>(std::size(kShoulderBases));
        break;
    case ItemCategory::Chest:
        bases = kChestBases;
        baseCount = static_cast<int>(std::size(kChestBases));
        break;
    case ItemCategory::Hands:
        bases = kHandBases;
        baseCount = static_cast<int>(std::size(kHandBases));
        break;
    case ItemCategory::Waist:
        bases = kWaistBases;
        baseCount = static_cast<int>(std::size(kWaistBases));
        break;
    case ItemCategory::Legs:
        bases = kLegBases;
        baseCount = static_cast<int>(std::size(kLegBases));
        break;
    case ItemCategory::Feet:
        bases = kFeetBases;
        baseCount = static_cast<int>(std::size(kFeetBases));
        break;
    case ItemCategory::Weapon:
        bases = kWeaponBases;
        baseCount = static_cast<int>(std::size(kWeaponBases));
        break;
    case ItemCategory::OffHand:
        bases = kOffHandBases;
        baseCount = static_cast<int>(std::size(kOffHandBases));
        break;
    case ItemCategory::Amulet:
        bases = kAmuletBases;
        baseCount = static_cast<int>(std::size(kAmuletBases));
        break;
    case ItemCategory::Ring:
        bases = kRingBases;
        baseCount = static_cast<int>(std::size(kRingBases));
        break;
    case ItemCategory::Cloak:
        bases = kCloakBases;
        baseCount = static_cast<int>(std::size(kCloakBases));
        break;
    case ItemCategory::Charm:
        bases = kCharmBases;
        baseCount = static_cast<int>(std::size(kCharmBases));
        break;
    case ItemCategory::Relic:
        bases = kRelicBases;
        baseCount = static_cast<int>(std::size(kRelicBases));
        break;
    default:
        break;
    }

    const int baseIndex = pickIndex(rng_, baseCount);

    std::ostringstream name;
    name << prefix << ' ' << bases[baseIndex];
    if (rarity == ItemRarity::Legendary) {
        name << " of Legends";
    } else {
        name << ' ' << suffix;
    }
    return name.str();
}

ItemMetadata ItemGenerator::generate(const ItemGenerationContext& context) {
    const int tierValue = static_cast<int>(context.tier) + 1;
    const int itemLevel = std::max(1, context.zoneDepth + tierValue - 1);

    ItemMetadata item{};
    item.rarity = context.rarity;
    item.itemLevel = itemLevel;
    item.category = rollCategory();
    item.iconLetter = iconForCategory(item.category);

    const std::string prefix = kPrefixes[pickIndex(rng_, static_cast<int>(std::size(kPrefixes)))];
    const std::string suffix = kSuffixes[pickIndex(rng_, static_cast<int>(std::size(kSuffixes)))];
    item.name = buildName(item.category, context.rarity, prefix, suffix);

    item.bonuses = rollBonuses(item.category, context.rarity, itemLevel);
    item.value = (5 + itemLevel * 3) * (1 + static_cast<int>(context.rarity) * 4);

    const int categoryCode = static_cast<int>(item.category);
    item.itemId = 4000U + static_cast<std::uint32_t>(categoryCode * 1000U) +
                  static_cast<std::uint32_t>(itemLevel * 10U) +
                  static_cast<std::uint32_t>(context.rarity);

    return item;
}

void ItemGenerator::rerollBonuses(ItemMetadata& item) {
    applyItemDefinition(item);
    item.bonuses = rollBonuses(item.category, item.rarity, std::max(1, item.itemLevel));
    item.value = std::max(item.value, (5 + item.itemLevel * 3) * (1 + static_cast<int>(item.rarity) * 4));
}

} // namespace systems
