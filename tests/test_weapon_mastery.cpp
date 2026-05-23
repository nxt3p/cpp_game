#include <catch2/catch_test_macros.hpp>

#include "game/SaveGame.hpp"
#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/ItemStats.hpp"
#include "systems/WeaponMastery.hpp"
#include "ui/UiTypes.hpp"

TEST_CASE("Weapon mastery grants XP and levels eligible weapons", "[items][mastery]") {
    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    systems::applyItemDefinition(blade);

    const systems::WeaponMasteryResult firstHit = systems::grantWeaponMasteryXp(blade, 10);
    CHECK(firstHit.xpGained == 10);
    CHECK_FALSE(firstHit.leveledUp);
    CHECK(blade.masteryXp == 10);

    const int threshold = systems::weaponMasteryXpToNextLevel(blade.masteryLevel);
    const systems::WeaponMasteryResult levelUp =
        systems::grantWeaponMasteryXp(blade, threshold - blade.masteryXp + 1);
    CHECK(levelUp.leveledUp);
    CHECK(blade.masteryLevel == 2);

    systems::ItemMetadata charm{201U, "Scout Charm", systems::ItemRarity::Rare, 30};
    systems::applyItemDefinition(charm);
    const systems::WeaponMasteryResult charmResult = systems::grantWeaponMasteryXp(charm, 100);
    CHECK(charmResult.xpGained == 0);
    CHECK(charm.masteryLevel == 1);
}

TEST_CASE("Weapon mastery bonuses scale equipped combat stats", "[items][mastery]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;
    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    REQUIRE(inventory.addItem(blade).success);
    REQUIRE(equipment.equipFromInventory(inventory, 0).success);

    ui::CharacterScreenData base{};
    base.level = 1;
    base.strength = 10;
    base.dexterity = 10;
    base.vitality = 10;

    const systems::EffectiveCharacterStats before =
        systems::computeEffectiveStats(base, equipment);

    REQUIRE(equipment.modifySlot(systems::EquipmentSlotKind::Weapon, [](systems::ItemMetadata& item) {
        item.masteryLevel = 3;
        item.masteryXp = 0;
    }));

    const systems::EffectiveCharacterStats after =
        systems::computeEffectiveStats(base, equipment);
    CHECK(after.damage > before.damage);
}

TEST_CASE("Weapon mastery serializes through save item JSON", "[save][mastery]") {
    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    systems::applyItemDefinition(blade);
    blade.masteryLevel = 4;
    blade.masteryXp = 12;

    game::SaveGameSnapshot snapshot{};
    snapshot.characterClass = game::CharacterClass::WARRIOR;
    snapshot.equipment.push_back(
        game::SaveEquipmentSlot{systems::EquipmentSlotKind::Weapon, blade});

    const std::string json = game::SaveGameIO::serializeSnapshot(snapshot);
    game::SaveGameSnapshot loaded{};
    const game::SaveGameResult result = game::SaveGameIO::deserializeSnapshot(json, loaded);
    REQUIRE(result.success);
    REQUIRE(loaded.equipment.size() == 1);
    REQUIRE(loaded.equipment.front().item.has_value());
    CHECK(loaded.equipment.front().item->masteryLevel == 4);
    CHECK(loaded.equipment.front().item->masteryXp == 12);

    const std::string tooltip = systems::formatItemTooltip(*loaded.equipment.front().item);
    CHECK(tooltip.find("Weapon Mastery") != std::string::npos);
}

TEST_CASE("Weapon mastery XP formulas reward hits and kills", "[items][mastery]") {
    CHECK(systems::weaponMasteryXpForHit(20) == 10);
    CHECK(systems::weaponMasteryXpForHit(1) == 1);
    CHECK(systems::weaponMasteryXpForKill(25) == 12);
    CHECK(systems::weaponMasteryXpForKill(3) == 5);
}
