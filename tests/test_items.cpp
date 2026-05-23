#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "systems/Inventory.hpp"
#include "systems/ItemStats.hpp"
#include "ui/UiTypes.hpp"

TEST_CASE("Item definitions apply RPG stat bonuses when equipped", "[items]") {
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

    const systems::EffectiveCharacterStats effective =
        systems::computeEffectiveStats(base, equipment);

    CHECK(effective.strength == 14);
    CHECK(effective.damage >= 14);
    CHECK(effective.attacksPerSecond == Catch::Approx(1.5F).margin(1e-3F));
}

TEST_CASE("Item tooltip lists stat lines", "[items]") {
    systems::ItemMetadata charm{201U, "Scout Charm", systems::ItemRarity::Rare, 30};
    systems::applyItemDefinition(charm);

    const std::vector<std::string> lines = systems::formatItemStatLines(charm);
    REQUIRE_FALSE(lines.empty());

    const std::string tooltip = systems::formatItemTooltip(charm);
    CHECK(tooltip.find("Scout Charm") != std::string::npos);
    CHECK(tooltip.find("Dexterity") != std::string::npos);
    CHECK(tooltip.find("Light Radius") != std::string::npos);
}

TEST_CASE("Gear light radius extends player torch reach when equipped", "[items]") {
    systems::Inventory inventory(6, 4);
    systems::Equipment equipment;
    systems::ItemMetadata charm{201U, "Scout Charm", systems::ItemRarity::Rare, 30};
    REQUIRE(inventory.addItem(charm).success);
    REQUIRE(equipment.equipFromInventory(inventory, 0).success);

    ui::CharacterScreenData base{};
    const systems::EffectiveCharacterStats effective =
        systems::computeEffectiveStats(base, equipment);

    CHECK(effective.lightRadius == Catch::Approx(systems::kBaseLightRadius + 5.0F).margin(1e-3F));
}
