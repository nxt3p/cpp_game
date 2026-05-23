#include <catch2/catch_test_macros.hpp>

#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemStats.hpp"
#include "ui/UiTypes.hpp"

TEST_CASE("Equipped items apply stat bonuses but bag items do not", "[items][equipment]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;

    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    systems::ItemMetadata charm{201U, "Scout Charm", systems::ItemRarity::Rare, 30};
    REQUIRE(inventory.addItem(blade).success);
    REQUIRE(inventory.addItem(charm).success);

    ui::CharacterScreenData base{};
    base.level = 1;
    base.strength = 10;
    base.dexterity = 10;
    base.vitality = 10;

    const systems::EffectiveCharacterStats bagOnly =
        systems::computeEffectiveStats(base, equipment);
    CHECK(bagOnly.strength == 10);
    CHECK(bagOnly.damage == 10);

    REQUIRE(equipment.equipFromInventory(inventory, 0).success);
    const systems::EffectiveCharacterStats withBlade =
        systems::computeEffectiveStats(base, equipment);
    CHECK(withBlade.strength == 14);
    CHECK(withBlade.damage >= 14);

    REQUIRE(equipment.equipFromInventory(inventory, 1).success);
    const systems::EffectiveCharacterStats withBladeAndCharm =
        systems::computeEffectiveStats(base, equipment);
    CHECK(withBladeAndCharm.dexterity == 13);
}

TEST_CASE("Equipment swaps return previous item to inventory", "[items][equipment]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;

    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    systems::ItemMetadata sword{301U, "Forged Sword", systems::ItemRarity::Rare, 80};
    REQUIRE(inventory.addItem(blade).success);
    REQUIRE(inventory.addItem(sword).success);

    REQUIRE(equipment.equipFromInventory(inventory, 0).success);
    REQUIRE(equipment.isSlotOccupied(systems::EquipmentSlotKind::Weapon));

    REQUIRE(equipment.equipFromInventory(inventory, 1).success);
    CHECK(equipment.itemAt(systems::EquipmentSlotKind::Weapon)->itemId == 301U);
    REQUIRE(inventory.isSlotOccupied(1));
    CHECK(inventory.slotAt(1).item->itemId == 101U);
}

TEST_CASE("Ring items fill left then right slot", "[items][equipment]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;

    systems::ItemMetadata ringA{501U, "Iron Band", systems::ItemRarity::Common, 8};
    systems::ItemMetadata ringB{502U, "Silver Loop", systems::ItemRarity::Rare, 20};
    ringA.category = systems::ItemCategory::Ring;
    ringA.iconLetter = 'R';
    ringB.category = systems::ItemCategory::Ring;
    ringB.iconLetter = 'R';

    REQUIRE(inventory.addItem(ringA).success);
    REQUIRE(inventory.addItem(ringB).success);

    REQUIRE(equipment.equipFromInventory(inventory, 0).success);
    CHECK(equipment.isSlotOccupied(systems::EquipmentSlotKind::RingLeft));
    CHECK_FALSE(equipment.isSlotOccupied(systems::EquipmentSlotKind::RingRight));

    REQUIRE(equipment.equipFromInventory(inventory, 1).success);
    CHECK(equipment.isSlotOccupied(systems::EquipmentSlotKind::RingLeft));
    CHECK(equipment.isSlotOccupied(systems::EquipmentSlotKind::RingRight));
}

TEST_CASE("Unequip returns item to first free inventory slot", "[items][equipment]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;

    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    REQUIRE(inventory.addItem(blade).success);
    REQUIRE(equipment.equipFromInventory(inventory, 0).success);
    CHECK_FALSE(inventory.isSlotOccupied(0));

    REQUIRE(equipment.unequipToInventory(inventory, systems::EquipmentSlotKind::Weapon).success);
    CHECK_FALSE(equipment.isSlotOccupied(systems::EquipmentSlotKind::Weapon));
    CHECK(inventory.usedSlots() == 1);
}

TEST_CASE("Consumables cannot be equipped", "[items][equipment]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;

    systems::ItemMetadata tonic{102U, "Health Tonic", systems::ItemRarity::Common, 5};
    REQUIRE(inventory.addItem(tonic).success);

    const systems::EquipmentActionResult result = equipment.equipFromInventory(inventory, 0);
    CHECK_FALSE(result.success);
}

TEST_CASE("All fifteen equipment slot labels are defined", "[items][equipment]") {
    for (int index = 0; index < static_cast<int>(systems::EquipmentSlotKind::Count); ++index) {
        const auto slot = static_cast<systems::EquipmentSlotKind>(index);
        CHECK(systems::Equipment::slotLabel(slot)[0] != '\0');
        CHECK(systems::Equipment::slotAbbreviation(slot) != '?');
    }
}
