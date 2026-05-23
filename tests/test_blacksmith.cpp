#include "systems/Blacksmith.hpp"
#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemStats.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

systems::ItemMetadata makeWeapon(const int upgradeLevel = 0) {
    systems::ItemMetadata item{};
    item.itemId = 4001U;
    item.name = "Test Blade";
    item.category = systems::ItemCategory::Weapon;
    item.rarity = systems::ItemRarity::Common;
    item.value = 40;
    item.bonuses.damage = 4;
    item.upgradeLevel = upgradeLevel;
    systems::applyItemDefinition(item);
    return item;
}

systems::ItemMetadata makeChestArmor() {
    systems::ItemMetadata item{};
    item.itemId = 4002U;
    item.name = "Test Vest";
    item.category = systems::ItemCategory::Chest;
    item.rarity = systems::ItemRarity::Common;
    item.value = 35;
    item.bonuses.vitality = 2;
    systems::applyItemDefinition(item);
    return item;
}

} // namespace

TEST_CASE("Blacksmith services unlock from mob and boss kills", "[blacksmith]") {
    const systems::BlacksmithUnlockState early{5, 0, 1};
    CHECK_FALSE(systems::isBlacksmithServiceUnlocked(systems::BlacksmithServiceKind::TemperWeapon, early));
    CHECK(systems::isBlacksmithServiceUnlocked(
        systems::BlacksmithServiceKind::TemperWeapon,
        {8, 0, 1}));

    CHECK_FALSE(systems::isBlacksmithServiceUnlocked(
        systems::BlacksmithServiceKind::ReforgeBackpack,
        {100, 0, 3}));
    CHECK(systems::isBlacksmithServiceUnlocked(
        systems::BlacksmithServiceKind::ReforgeBackpack,
        {0, 1, 1}));

    CHECK_FALSE(systems::isBlacksmithServiceUnlocked(
        systems::BlacksmithServiceKind::SoulInfusion,
        {40, 0, 1}));
    CHECK(systems::isBlacksmithServiceUnlocked(
        systems::BlacksmithServiceKind::SoulInfusion,
        {40, 0, 2}));
}

TEST_CASE("Blacksmith temper and masterwork modify equipped gear", "[blacksmith]") {
    systems::Equipment equipment{};
    equipment.setSlot(systems::EquipmentSlotKind::Weapon, makeWeapon());

    int gold = 100;
    const systems::BlacksmithResult tempered = systems::temperEquippedWeapon(equipment, gold);
    REQUIRE(tempered.success);
    CHECK(equipment.itemAt(systems::EquipmentSlotKind::Weapon)->bonuses.damage >= 6);
    CHECK(gold < 100);

    equipment.modifySlot(systems::EquipmentSlotKind::Weapon, [](systems::ItemMetadata& item) {
        item.rarity = systems::ItemRarity::Rare;
    });
    gold = 500;
    const systems::BlacksmithResult masterwork = systems::masterworkEquippedWeapon(equipment, gold);
    REQUIRE(masterwork.success);
    CHECK(equipment.itemAt(systems::EquipmentSlotKind::Weapon)->rarity == systems::ItemRarity::Legendary);
}

TEST_CASE("Blacksmith reforge rerolls procedural item stats", "[blacksmith]") {
    systems::Inventory inventory(4, 2);
    systems::ItemMetadata item = makeChestArmor();
    item.bonuses.strength = 9;
    REQUIRE(inventory.addItem(item).success);

    int gold = 200;
    systems::ItemMetadata working = *inventory.slotAt(0).item;
    const systems::BlacksmithResult result = systems::reforgeBackpackItem(working, 0x1234U, gold);
    REQUIRE(result.success);
    CHECK(gold == 125);
    CHECK(working.bonuses.strength != 9);
}

TEST_CASE("Blacksmith sell price is below item value", "[blacksmith]") {
    const systems::ItemMetadata item = makeWeapon();
    CHECK(systems::blacksmithSellPrice(item) < item.value);
    CHECK(systems::blacksmithBuyPrice(item) == item.value);
}
