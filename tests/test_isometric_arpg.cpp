#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "gameplay/IsometricCamera.hpp"
#include "render/AnimationStateMachine.hpp"
#include "render/AtlasMetadata.hpp"
#include "systems/Equipment.hpp"
#include "systems/Inventory.hpp"
#include "systems/InventoryDrag.hpp"
#include "systems/ItemTypes.hpp"
#include "systems/SkillBar.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

systems::ItemMetadata weaponNamed(const char* name, const std::uint32_t id) {
    systems::ItemMetadata item{};
    item.itemId = id;
    item.name = name;
    item.category = systems::ItemCategory::Weapon;
    return item;
}

} // namespace

TEST_CASE("Orthographic isometric pick hits the follow target at screen center", "[iso][camera]") {
    gameplay::IsometricCamera camera;
    camera.setViewportSize(1280, 720);
    const gameplay::Vec3 target{12.0F, 0.0F, -6.0F};
    const gameplay::CameraMatrices matrices = camera.matricesForTarget(target);

    glm::vec3 ground{};
    REQUIRE(gameplay::screenPointToGround(640.0F, 360.0F, 1280, 720, matrices, ground));
    CHECK(ground.x == Catch::Approx(target.x).margin(0.05F));
    CHECK(ground.y == Catch::Approx(0.0F).margin(0.001F));
    CHECK(ground.z == Catch::Approx(target.z).margin(0.05F));

    const gameplay::PointerRay ray = gameplay::pointerRayFromScreen(640.0F, 360.0F, 1280, 720, matrices);
    REQUIRE(ray.valid);
    CHECK(std::abs(glm::dot(glm::normalize(ray.direction), glm::normalize(-gameplay::kIsometricEyeOffset))) >
          0.99F);
}

TEST_CASE("Visible ground bounds cover the view and not the whole zone", "[iso][camera]") {
    gameplay::IsometricCamera camera;
    camera.setViewportSize(1280, 720);
    const glm::vec3 focus{12.0F, 0.0F, -6.0F};
    const gameplay::CameraMatrices matrices =
        camera.matricesForTarget(gameplay::Vec3{focus.x, focus.y, focus.z});

    const gameplay::GroundAabb view = gameplay::visibleGroundAabb(matrices, 1280, 720, focus, 8.0F);

    CHECK(view.contains(focus.x, focus.z));
    CHECK_FALSE(view.contains(focus.x + 200.0F, focus.z + 200.0F));
    const float spanX = view.maxX - view.minX;
    const float spanZ = view.maxZ - view.minZ;
    CHECK(spanX > 20.0F);
    CHECK(spanZ > 20.0F);
    CHECK(spanX < 160.0F);
    CHECK(spanZ < 160.0F);
}

TEST_CASE("Isometric sort key draws farther ground points first", "[iso][sort]") {
    gameplay::IsometricCamera camera;
    camera.setViewportSize(1280, 720);
    const gameplay::CameraMatrices matrices = camera.matricesForTarget(gameplay::Vec3{0.0F, 0.0F, 0.0F});

    const float closer = gameplay::isometricSortKey(matrices.view, glm::vec3(8.0F, 4.0F, 8.0F));
    const float farther = gameplay::isometricSortKey(matrices.view, glm::vec3(-8.0F, 4.0F, -8.0F));
    CHECK(farther < closer);

    struct Command {
        float sortKey;
        std::uint32_t sortId;
    };
    Command commands[] = {{closer, 2U}, {farther, 9U}, {farther, 1U}};
    std::sort(std::begin(commands), std::end(commands), [](const Command& a, const Command& b) {
        if (a.sortKey != b.sortKey) {
            return a.sortKey < b.sortKey;
        }
        return a.sortId < b.sortId;
    });
    CHECK(commands[0].sortId == 1U);
    CHECK(commands[1].sortId == 9U);
    CHECK(commands[2].sortId == 2U);
}

TEST_CASE("Generated warrior atlas JSON maps 8-direction clips", "[iso][atlas]") {
    render::DirectionalAtlas atlas;
    const std::string path = std::string(ENGINE_ASSETS_DIR) + "/textures/generated/warrior_atlas.json";
    REQUIRE(atlas.loadFromFile(path));
    CHECK(atlas.frameWidth == 64);
    CHECK(atlas.frameHeight == 64);
    CHECK(atlas.columns == 6);

    const render::DirectionalClipInfo* idle = atlas.find("idle");
    const render::DirectionalClipInfo* attack2 = atlas.find("attack2");
    const render::DirectionalClipInfo* cast = atlas.find("cast");
    REQUIRE(idle != nullptr);
    REQUIRE(attack2 != nullptr);
    REQUIRE(cast != nullptr);
    CHECK(idle->baseRow == 0);
    CHECK(idle->frames == 4);
    CHECK(idle->loop);
    CHECK(attack2->baseRow == 24);
    CHECK_FALSE(attack2->loop);
    CHECK(cast->baseRow == idle->baseRow + 8 * 4);

    render::NamedAtlas ui;
    REQUIRE(ui.loadFromFile(std::string(ENGINE_ASSETS_DIR) + "/textures/generated/ui_atlas.json"));
    CHECK(ui.find("globe_ring") != nullptr);
    CHECK(ui.find("hotbar_frame") != nullptr);
    CHECK(ui.find("inventory_panel") != nullptr);

    render::NamedAtlas items;
    REQUIRE(items.loadFromFile(std::string(ENGINE_ASSETS_DIR) + "/textures/generated/items_atlas.json"));
    CHECK(items.find("weapon") != nullptr);
    CHECK(items.find("potion") != nullptr);
    CHECK(items.find("gem") != nullptr);
}

TEST_CASE("Skill bar exposes eight warrior slots", "[iso][skills]") {
    systems::SkillBar bar;
    CHECK(systems::SkillBar::kSlotCount == 8);
    CHECK(bar.slot(0) == systems::SkillId::PowerStrike);
    CHECK(bar.slot(4) == systems::SkillId::Cleave);
    CHECK(bar.slot(5) == systems::SkillId::Firebolt);
    CHECK(bar.slot(6) == systems::SkillId::Shout);
    CHECK(bar.slot(7) == systems::SkillId::Slam);
    CHECK(systems::skillDefinition(systems::SkillId::Cleave).animation == systems::SkillAnimation::Attack2);
    CHECK(systems::skillDefinition(systems::SkillId::Firebolt).animation == systems::SkillAnimation::Cast);

    bar.setMaxMana(200);
    bar.restoreMana();
    const systems::SkillCastResult cast = bar.tryCast(4, false);
    CHECK(cast.success);
    CHECK_FALSE(bar.isReady(4));
    CHECK_FALSE(bar.tryCast(8, true).success);
}

TEST_CASE("Inventory drag moves, swaps, equips and unequips", "[iso][drag]") {
    systems::Inventory bag(4, 2);
    systems::Equipment paperDoll;
    REQUIRE(bag.addItem(weaponNamed("Blade", 11U)).success);
    REQUIRE(bag.addItem(weaponNamed("Axe", 12U)).success);

    systems::InventoryDrag drag;
    drag.begin(systems::DragOrigin::Inventory, 0);
    const systems::DragAction moved = drag.release(systems::DragOrigin::Inventory, 3, false);
    CHECK(moved.kind == systems::DragActionKind::Move);
    REQUIRE(bag.exchangeSlots(moved.from, moved.to));
    CHECK(bag.slotAt(3).item->name == "Blade");
    CHECK_FALSE(bag.isSlotOccupied(0));

    drag.begin(systems::DragOrigin::Inventory, 3);
    const systems::DragAction swapped = drag.release(systems::DragOrigin::Inventory, 1, true);
    CHECK(swapped.kind == systems::DragActionKind::Swap);
    REQUIRE(bag.exchangeSlots(swapped.from, swapped.to));
    CHECK(bag.slotAt(1).item->name == "Blade");
    CHECK(bag.slotAt(3).item->name == "Axe");

    drag.begin(systems::DragOrigin::Inventory, 1);
    const systems::DragAction equipped = drag.release(systems::DragOrigin::Inventory, 1, true);
    CHECK(equipped.kind == systems::DragActionKind::EquipFromInventory);
    REQUIRE(paperDoll.equipFromInventory(bag, equipped.from).success);
    CHECK(paperDoll.isSlotOccupied(systems::EquipmentSlotKind::Weapon));

    drag.begin(systems::DragOrigin::Equipment, static_cast<int>(systems::EquipmentSlotKind::Weapon));
    const systems::DragAction dropped =
        drag.release(systems::DragOrigin::Inventory, 0, bag.isSlotOccupied(0));
    CHECK(dropped.kind == systems::DragActionKind::UnequipToInventory);
    REQUIRE(paperDoll.unequipToIndex(bag, systems::EquipmentSlotKind::Weapon, dropped.to).success);
    CHECK(bag.slotAt(0).item->name == "Blade");
    CHECK_FALSE(paperDoll.isSlotOccupied(systems::EquipmentSlotKind::Weapon));

    drag.begin(systems::DragOrigin::Inventory, 0);
    CHECK(drag.release(systems::DragOrigin::None, -1, false).kind == systems::DragActionKind::None);
    drag.cancel();
    CHECK_FALSE(drag.active());
}

TEST_CASE("Headless soak of facing, skills and drags stays finite", "[iso][soak]") {
    render::AnimationStateMachine animation;
    systems::SkillBar skills;
    skills.setMaxMana(500);
    skills.restoreMana();
    systems::Inventory bag(6, 4);
    systems::InventoryDrag drag;
    REQUIRE(bag.addItem(weaponNamed("Blade", 1U)).success);

    for (int step = 0; step < 4000; ++step) {
        const float angle = static_cast<float>(step) * 0.17F;
        animation.setFacingFromDelta(glm::vec2(std::cos(angle), std::sin(angle)));
        animation.update(1.0F / 60.0F, step % 24 < 12);
        if (step % 37 == 0) {
            animation.triggerAttack(0.2F);
        }
        if (step % 53 == 0) {
            animation.triggerAttack2(0.18F);
        }
        if (step % 61 == 0) {
            animation.triggerCast(0.22F);
        }
        if (step % 90 == 0) {
            animation.triggerHit(0.12F);
        }
        skills.update(1.0F / 60.0F);
        if (step % 15 == 0) {
            static_cast<void>(skills.tryCast(step % systems::SkillBar::kSlotCount, true));
        }

        if (step % 20 == 0) {
            drag.begin(systems::DragOrigin::Inventory, 0);
            const int destination = step % bag.capacity();
            const systems::DragAction action =
                drag.release(systems::DragOrigin::Inventory, destination, bag.isSlotOccupied(destination));
            if (action.kind == systems::DragActionKind::Move || action.kind == systems::DragActionKind::Swap) {
                static_cast<void>(bag.exchangeSlots(action.from, action.to));
            }
            drag.cancel();
        }

        CHECK(std::isfinite(animation.stateTime()));
        CHECK(skills.mana() >= 0);
        CHECK(skills.mana() <= skills.maxMana());
    }

    CHECK(animation.facing8() == animation.facing8());
    CHECK(bag.usedSlots() == 1);
}
