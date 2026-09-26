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
    CHECK(tooltip.find("ilvl") != std::string::npos);
    CHECK(tooltip.find("Price") != std::string::npos);

    const systems::ItemTooltipCard card = systems::buildItemTooltipCard(charm, "Equipped");
    CHECK(card.lines.front().text == "Equipped");
    bool sawAttribute = false;
    bool sawEffect = false;
    for (const systems::TooltipLine& line : card.lines) {
        if (line.tone == systems::TooltipTone::Attribute && line.text.find("Dexterity") != std::string::npos) {
            sawAttribute = true;
        }
        if (line.tone == systems::TooltipTone::Effect && line.text.find("Light Radius") != std::string::npos) {
            sawEffect = true;
        }
    }
    CHECK(sawAttribute);
    CHECK(sawEffect);

    systems::ItemMetadata blade{};
    blade.name = "Edge";
    blade.category = systems::ItemCategory::Weapon;
    blade.rarity = systems::ItemRarity::Legendary;
    blade.itemLevel = 8;
    blade.value = 120;
    blade.bonuses.damage = 6;
    blade.bonuses.attackSpeed = 0.2F;
    const int dps = systems::estimateWeaponDps(blade);
    CHECK(dps > 1);
    CHECK(systems::formatItemTooltip(blade).find("DPS") != std::string::npos);
    CHECK(systems::rarityColor(systems::ItemRarity::Unique).green >
          systems::rarityColor(systems::ItemRarity::Legendary).green);
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
