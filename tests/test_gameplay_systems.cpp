#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include "gameplay/GameStateManager.hpp"
#include "gameplay/IsometricCamera.hpp"
#include "gameplay/ZoneManager.hpp"
#include "systems/Inventory.hpp"
#include "systems/LootEngine.hpp"
#include "systems/TradeSystem.hpp"
#include "ui/MinimapSystem.hpp"

TEST_CASE("Isometric camera tracks player with fixed offset", "[gameplay][camera]") {
    gameplay::IsometricCamera camera;
    camera.setViewportSize(1280, 720);

    const gameplay::Vec3 player{10.0F, 0.0F, 20.0F};
    const glm::vec3 eye = camera.eyePositionForTarget(player);
    const gameplay::CameraMatrices matrices = camera.matricesForTarget(player);

    CHECK(eye.x == Catch::Approx(player.x + 12.0F).margin(1e-4F));
    CHECK(eye.y == Catch::Approx(player.y + 18.0F).margin(1e-4F));
    CHECK(eye.z == Catch::Approx(player.z + 12.0F).margin(1e-4F));
    CHECK(glm::determinant(matrices.view) != Catch::Approx(0.0F).margin(1e-4F));
    CHECK(glm::determinant(matrices.projection) != Catch::Approx(0.0F).margin(1e-4F));
}

TEST_CASE("Zone transition shifts player from town to plains", "[gameplay][zone]") {
    gameplay::ZoneManager zones;
    gameplay::GameStateManager states;

    CHECK(zones.activeZone() == gameplay::WorldZone::TOWN);
    CHECK_FALSE(zones.player().attacksEnabled());

    const gameplay::ZoneTransitionResult transition =
        zones.updatePlayerPosition(gameplay::Vec3{0.0F, 0.0F, 40.0F});

    CHECK(transition.transitioned);
    CHECK(zones.activeZone() == gameplay::WorldZone::PLAINS);
    CHECK(zones.player().attacksEnabled());
    CHECK(zones.scenery().size() >= 8);

    states.transitionTo(gameplay::GameState::PLAINS);
    CHECK(states.currentState() == gameplay::GameState::PLAINS);
}

TEST_CASE("LootEngine accrues action coins with configured weights", "[systems][loot]") {
    systems::LootEngine loot(12345U);

    loot.registerAction(systems::ActionType::ROCK_CLICK);
    loot.registerAction(systems::ActionType::CHEST_OPEN);
    loot.registerAction(systems::ActionType::MOB_KILL);
    loot.registerAction(systems::ActionType::BOSS_KILL);

    CHECK(loot.coinPool() == 116);
}

TEST_CASE("LootEngine drains pool on rare drops", "[systems][loot]") {
    systems::LootEngine loot(424242U);

    for (int i = 0; i < 30; ++i) {
        loot.registerAction(systems::ActionType::BOSS_KILL);
    }

    const systems::LootDropResult drop = loot.triggerDropCheck(systems::EntityTier::Boss);
    REQUIRE(drop.dropped);

    if (drop.resolvedRarity == systems::ItemRarity::Rare ||
        drop.resolvedRarity == systems::ItemRarity::Legendary) {
        CHECK(drop.poolDrained);
        CHECK(loot.coinPool() == 0);
    } else {
        CHECK(loot.coinPool() < drop.coinPoolBeforeRoll);
    }
}

TEST_CASE("Inventory enforces capacity and slot validation", "[systems][inventory]") {
    systems::Inventory inventory(4, 2);
    CHECK(inventory.capacity() == 8);

    systems::ItemMetadata item{1U, "Iron Ore", systems::ItemRarity::Common, 3};
    for (int i = 0; i < inventory.capacity(); ++i) {
        const systems::InventoryAddResult result = inventory.addItem(item);
        REQUIRE(result.success);
    }

    const systems::InventoryAddResult overflow = inventory.addItem(item);
    CHECK_FALSE(overflow.success);
    CHECK(inventory.usedSlots() == inventory.capacity());
    CHECK(inventory.discardAt(0));
    CHECK(inventory.canPlaceAt(0));
}

TEST_CASE("TradeSystem validates gold before swaps", "[systems][trade]") {
    systems::Inventory playerInventory(4, 2);
    systems::Inventory vendorInventory(4, 2);
    systems::TradeSystem trade(playerInventory, vendorInventory);

    trade.bindVendor("Blacksmith", 500);
    trade.setPlayerGold(20);

    systems::ItemMetadata playerItem{10U, "Rusty Sword", systems::ItemRarity::Common, 8};
    systems::ItemMetadata vendorItem{20U, "Steel Blade", systems::ItemRarity::Rare, 40};
    REQUIRE(playerInventory.addItemAt(playerItem, 0).success);
    REQUIRE(vendorInventory.addItemAt(vendorItem, 0).success);

    const systems::TradeResult denied =
        trade.executeSwap(systems::TradeOffer{0, 0, 100});
    CHECK_FALSE(denied.success);

    trade.setPlayerGold(200);
    const systems::TradeResult success = trade.executeSwap(systems::TradeOffer{0, 0, 50});
    CHECK(success.success);
    CHECK(trade.playerGold() == 150);
    CHECK(playerInventory.slotAt(0).item->name == "Steel Blade");
    CHECK(vendorInventory.slotAt(0).item->name == "Rusty Sword");
}

TEST_CASE("Minimap maps world coordinates into viewport pixels", "[ui][minimap]") {
    ui::MinimapSystem minimap;
    minimap.setViewport(ui::Rect2D{20.0F, 20.0F, 200.0F, 200.0F});
    minimap.setTerrainBounds(ui::TerrainBounds{-100.0F, 100.0F, -100.0F, 100.0F});

    const ui::MinimapLayer layer = minimap.buildLayer(0.0F, 0.0F, {{50.0F, -50.0F}});

    CHECK(layer.player.normalized.x == Catch::Approx(0.5F).margin(1e-4F));
    CHECK(layer.player.normalized.y == Catch::Approx(0.5F).margin(1e-4F));
    CHECK(layer.player.pixel.x == Catch::Approx(120.0F).margin(1e-3F));
    CHECK(layer.player.pixel.y == Catch::Approx(120.0F).margin(1e-3F));
    REQUIRE(layer.entities.size() == 1);
    CHECK(layer.entities.front().inBounds);
}
