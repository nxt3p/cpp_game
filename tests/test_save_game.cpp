#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/SaveGame.hpp"
#include "systems/Equipment.hpp"
#include "systems/ItemStats.hpp"
#include "systems/LootEngine.hpp"
#include "systems/RunProgression.hpp"

#include <filesystem>

using game::CharacterClass;
using game::SaveCharacterData;
using game::SaveEquipmentSlot;
using game::SaveGameIO;
using game::SaveGameResult;
using game::SaveGameSnapshot;
using game::SaveInventorySlot;
using game::SaveMobHealthEntry;
using game::SaveProgressionData;

TEST_CASE("SaveGame round-trip preserves character inventory and equipment", "[save]") {
    SaveGameSnapshot original{};
    original.characterClass = CharacterClass::WARRIOR;
    original.character = SaveCharacterData{
        5,
        240,
        300,
        120,
        3,
        1.24F,
        18,
        12,
        16,
        2,
        95,
        420};
    original.progression = SaveProgressionData{3, 0xDEADBEEFU, 2, 7, 42, 55, 0x12345678U};
    original.world.activeZone = gameplay::WorldZone::PLAINS;
    original.world.playerPosition = gameplay::Vec3{4.0F, 0.0F, 88.0F};
    original.world.attacksEnabled = true;
    original.world.plainsSeed = 0xABCD1234U;
    original.world.plainsDepth = 3;
    original.world.scenery.push_back(
        gameplay::WorldEntitySnapshot{500U, gameplay::EntityKind::ENEMY_MOB, {2.0F, 0.0F, 90.0F}, true, 0});
    original.world.mobHealth.push_back(SaveMobHealthEntry{500U, 22, 40});

    systems::ItemMetadata blade{101U, "Traveler Blade", systems::ItemRarity::Common, 12};
    systems::applyItemDefinition(blade);
    original.inventory.push_back(SaveInventorySlot{0, blade});

    systems::ItemMetadata charm{201U, "Scout Charm", systems::ItemRarity::Rare, 30};
    systems::applyItemDefinition(charm);
    original.equipment.push_back(
        SaveEquipmentSlot{systems::EquipmentSlotKind::Charm, charm});

    const std::string json = SaveGameIO::serializeSnapshot(original);
    SaveGameSnapshot loaded{};
    const SaveGameResult result = SaveGameIO::deserializeSnapshot(json, loaded);
    REQUIRE(result.success);

    CHECK(loaded.characterClass == CharacterClass::WARRIOR);
    CHECK(loaded.character.level == 5);
    CHECK(loaded.character.carriedSouls == 120);
    CHECK(loaded.character.statUpgradesPurchased == 3);
    CHECK(loaded.character.soulGainMultiplier == Catch::Approx(1.24F).margin(1e-4F));
    CHECK(loaded.character.gold == 420);
    CHECK(loaded.progression.depth == 3);
    CHECK(loaded.progression.lifetimeMobKills == 42);
    CHECK(loaded.progression.lootCoinPool == 55);
    CHECK(loaded.world.activeZone == gameplay::WorldZone::PLAINS);
    CHECK(loaded.world.scenery.size() == 1);
    CHECK(loaded.world.scenery.front().id == 500U);
    CHECK(loaded.world.mobHealth.front().currentHp == 22);
    REQUIRE(loaded.inventory.size() == 1);
    REQUIRE(loaded.inventory.front().item.has_value());
    CHECK(loaded.inventory.front().item->itemId == 101U);
    REQUIRE(loaded.equipment.size() == 1);
    REQUIRE(loaded.equipment.front().item.has_value());
    CHECK(loaded.equipment.front().slot == systems::EquipmentSlotKind::Charm);
}

TEST_CASE("SaveGameIO writes and reads save file from disk", "[save]") {
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "cppgame_save_test.json";

    SaveGameSnapshot original{};
    original.characterClass = CharacterClass::MAGE;
    original.character.level = 2;
    original.character.gold = 150;
    original.progression.depth = 2;

    const SaveGameResult saveResult = SaveGameIO::saveToFile(original, tempPath);
    REQUIRE(saveResult.success);

    SaveGameSnapshot loaded{};
    const SaveGameResult loadResult = SaveGameIO::loadFromFile(loaded, tempPath);
    REQUIRE(loadResult.success);
    CHECK(loaded.characterClass == CharacterClass::MAGE);
    CHECK(loaded.character.level == 2);
    CHECK(loaded.character.gold == 150);
    CHECK(loaded.progression.depth == 2);

    std::error_code errorCode;
    std::filesystem::remove(tempPath, errorCode);
}

TEST_CASE("Inventory and equipment apply saved slot snapshots", "[save]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;

    std::vector<systems::InventorySlot> slots(static_cast<std::size_t>(inventory.capacity()));
    systems::ItemMetadata tonic{102U, "Health Tonic", systems::ItemRarity::Common, 5};
    systems::applyItemDefinition(tonic);
    slots[2].item = tonic;
    inventory.applySavedSlots(slots);

    CHECK(inventory.isSlotOccupied(2));
    CHECK(inventory.slotAt(2).item->category == systems::ItemCategory::Consumable);

    systems::ItemMetadata vest{302U, "Plate Vest", systems::ItemRarity::Rare, 120};
    equipment.setSlot(systems::EquipmentSlotKind::Chest, vest);
    CHECK(equipment.isSlotOccupied(systems::EquipmentSlotKind::Chest));
}

TEST_CASE("RunProgression and LootEngine restore saved progression values", "[save]") {
    systems::RunProgression progression;
    progression.applyState(4, 0xFEEDFACEU, 3, 9, 120);
    CHECK(progression.depth() == 4);
    CHECK(progression.runSeed() == 0xFEEDFACEU);
    CHECK(progression.totalBossKills() == 3);
    CHECK(progression.mobsKilledThisDepth() == 9);
    CHECK(progression.lifetimeMobKills() == 120);

    systems::LootEngine lootEngine;
    lootEngine.setSeed(0xABCDEF01U);
    lootEngine.setCoinPool(88);
    CHECK(lootEngine.coinPool() == 88);
    CHECK(lootEngine.rngSeed() == 0xABCDEF01U);
}
