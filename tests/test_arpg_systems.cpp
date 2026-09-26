#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/CombatFeedback.hpp"
#include "render/AnimationStateMachine.hpp"
#include "render/ParticleSystem.hpp"
#include "systems/CharacterCombat.hpp"
#include "systems/ItemStats.hpp"
#include "systems/RunProgression.hpp"
#include "systems/SkillBar.hpp"
#include "ui/HudConsoleLayout.hpp"
#include "ui/UiLayout.hpp"
#include "ui/UiScale.hpp"

#include <glm/glm.hpp>

TEST_CASE("SkillBar spends mana, enforces cooldowns and regenerates", "[arpg][skills]") {
    systems::SkillBar bar;
    bar.setMaxMana(100);
    bar.restoreMana();
    REQUIRE(bar.mana() == 100);

    const systems::SkillCastResult first = bar.tryCast(0, true);
    REQUIRE(first.success);
    CHECK(first.skill == systems::SkillId::PowerStrike);
    CHECK(bar.mana() == 100 - systems::skillDefinition(systems::SkillId::PowerStrike).manaCost);
    CHECK_FALSE(bar.isReady(0));

    const systems::SkillCastResult again = bar.tryCast(0, true);
    CHECK_FALSE(again.success);
    CHECK(std::string(again.failureReason) == "On cooldown");

    const systems::SkillCastResult noTarget = bar.tryCast(0, false);
    CHECK_FALSE(noTarget.success);

    bar.update(10.0F);
    CHECK(bar.isReady(0));
    CHECK(bar.mana() == 100);

    bar.setMana(5);
    const systems::SkillCastResult broke = bar.tryCast(2, false);
    CHECK_FALSE(broke.success);
    CHECK(std::string(broke.failureReason) == "Not enough mana");

    CHECK(systems::computeMaxMana(1, false) == 48);
    CHECK(systems::computeMaxMana(1, true) == 72);
    CHECK(bar.tryCast(9, true).success == false);
}

TEST_CASE("Critical hits scale with dexterity and multiply damage", "[arpg][combat]") {
    CHECK(systems::computeCriticalChance(0) == Catch::Approx(0.05F));
    CHECK(systems::computeCriticalChance(50) == Catch::Approx(0.25F));
    CHECK(systems::computeCriticalChance(500) == Catch::Approx(0.5F));
    CHECK(systems::isCriticalRoll(0.01F, 0.05F));
    CHECK_FALSE(systems::isCriticalRoll(0.5F, 0.05F));
    CHECK(systems::applyCriticalDamage(10) == 18);
    CHECK(systems::applyCriticalDamage(0) == 0);
}

TEST_CASE("CombatFeedback trauma decays and hit-stop scales time", "[arpg][feedback]") {
    game::CombatFeedback feedback;
    CHECK(feedback.shakeOffset() == glm::vec3(0.0F));
    CHECK(feedback.timeScale() == Catch::Approx(1.0F));

    feedback.addTrauma(0.6F);
    feedback.addTrauma(0.6F);
    CHECK(feedback.trauma() == Catch::Approx(1.0F));
    feedback.update(0.016F);
    CHECK(glm::length(feedback.shakeOffset()) > 0.0F);
    CHECK(glm::length(feedback.shakeOffset()) <= 0.55F * 1.01F);

    feedback.addHitStop(0.1F, 0.2F);
    CHECK(feedback.inHitStop());
    CHECK(feedback.timeScale() == Catch::Approx(0.2F));
    feedback.update(0.2F);
    CHECK_FALSE(feedback.inHitStop());
    CHECK(feedback.timeScale() == Catch::Approx(1.0F));

    feedback.addFlash(glm::vec3(1.0F, 0.5F, 0.0F), 0.5F, 1.0F);
    CHECK(feedback.flashColor().a == Catch::Approx(0.5F));
    feedback.update(0.5F);
    CHECK(feedback.flashColor().a == Catch::Approx(0.25F));
    feedback.update(2.0F);
    CHECK(feedback.trauma() == Catch::Approx(0.0F));
    CHECK_FALSE(feedback.hasFlash());
}

TEST_CASE("AnimationStateMachine prioritises death over hit and expires timed states", "[arpg][animation]") {
    render::AnimationStateMachine anim;
    CHECK(anim.state() == render::AnimState::Idle);

    anim.update(0.1F, true);
    CHECK(anim.state() == render::AnimState::Walk);
    CHECK(anim.clip() == render::SpriteClip::Walk);

    anim.triggerHit(0.2F);
    CHECK(anim.state() == render::AnimState::Hit);
    anim.update(0.1F, true);
    CHECK(anim.state() == render::AnimState::Hit);
    anim.update(0.15F, true);
    CHECK(anim.state() == render::AnimState::Walk);

    anim.triggerAttack(0.3F);
    anim.triggerHit(0.2F);
    CHECK(anim.state() == render::AnimState::Attack);

    anim.triggerDeath(0.5F);
    CHECK(anim.isDying());
    anim.triggerHit(0.2F);
    anim.triggerAttack(0.2F);
    CHECK(anim.state() == render::AnimState::Death);
    CHECK(anim.stateProgress() == Catch::Approx(0.0F));
    anim.update(0.25F, false);
    CHECK(anim.stateProgress() == Catch::Approx(0.5F));
    anim.update(0.3F, false);
    CHECK(anim.finished());
    CHECK(anim.clip() == render::SpriteClip::Death);
}

TEST_CASE("Eight-way facing resolves sectors with hysteresis and maps to sheet rows", "[arpg][animation]") {
    using render::SpriteFacing8;
    SpriteFacing8 facing = SpriteFacing8::South;
    facing = render::facing8FromDelta(glm::vec2(0.0F, 1.0F), facing);
    CHECK(facing == SpriteFacing8::South);
    facing = render::facing8FromDelta(glm::vec2(-1.0F, 0.0F), facing);
    CHECK(facing == SpriteFacing8::West);
    facing = render::facing8FromDelta(glm::vec2(0.0F, -1.0F), facing);
    CHECK(facing == SpriteFacing8::North);
    facing = render::facing8FromDelta(glm::vec2(1.0F, 0.0F), facing);
    CHECK(facing == SpriteFacing8::East);
    facing = render::facing8FromDelta(glm::vec2(1.0F, 1.0F), facing);
    CHECK(facing == SpriteFacing8::SouthEast);
    facing = render::facing8FromDelta(glm::vec2(-1.0F, -1.0F), facing);
    CHECK(facing == SpriteFacing8::NorthWest);

    // Tiny deltas and near-boundary jitter keep the current facing.
    CHECK(render::facing8FromDelta(glm::vec2(0.001F, 0.001F), SpriteFacing8::East) == SpriteFacing8::East);
    const glm::vec2 nearBoundary(std::cos(glm::radians(22.0F)), std::sin(glm::radians(22.0F)));
    CHECK(render::facing8FromDelta(nearBoundary, SpriteFacing8::East) == SpriteFacing8::East);

    CHECK(render::facing4From8(SpriteFacing8::SouthWest) == render::SpriteFacing::Left);
    CHECK(render::facing4From8(SpriteFacing8::NorthEast) == render::SpriteFacing::Right);
    CHECK(render::facing4From8(SpriteFacing8::North) == render::SpriteFacing::Up);
    CHECK(render::facing4From8(SpriteFacing8::South) == render::SpriteFacing::Down);

    const glm::vec2 westDir = render::facing8Direction(SpriteFacing8::West);
    CHECK(westDir.x == Catch::Approx(-1.0F).margin(1e-4F));
}

TEST_CASE("ParticleSystem simulates, expires and recycles within capacity", "[arpg][particles]") {
    render::ParticleSystem particles(64, 123U);
    particles.spawnHitSparks(glm::vec3(0.0F), true);
    const std::size_t afterSparks = particles.aliveCount();
    CHECK(afterSparks == 26);

    particles.spawnLootPillar(glm::vec3(1.0F, 0.0F, 1.0F), glm::vec4(1.0F), 3.0F);
    CHECK(particles.aliveCount() <= particles.capacity());
    CHECK(particles.droppedSpawnCount() > 0);

    for (int step = 0; step < 240; ++step) {
        particles.update(1.0F / 60.0F);
    }
    CHECK(particles.aliveCount() == 0);

    particles.updateAmbientDust(glm::vec3(0.0F), 8.0F, 1.0F, 10.0F);
    CHECK(particles.aliveCount() == 10);
    for (const render::Particle& particle : particles.particles()) {
        CHECK(particle.blend == render::ParticleBlend::Additive);
        CHECK(particle.position.y > 0.0F);
    }
    particles.clear();
    CHECK(particles.aliveCount() == 0);
}

TEST_CASE("Hud console layout keeps globes, slots and strip inside the screen", "[arpg][hud][ui]") {
    for (const auto& [width, height] : {std::pair{1280, 720}, std::pair{1920, 1080}, std::pair{3840, 2160}}) {
        const ui::UiScale scale(width, height);
        const ui::HudConsoleLayout console = ui::computeHudConsoleLayout(scale);
        const float screenW = static_cast<float>(width);
        const float screenH = static_cast<float>(height);

        CHECK(console.panel.y + console.panel.height == Catch::Approx(screenH).margin(0.01F));
        CHECK(console.panel.width == Catch::Approx(screenW));
        CHECK(console.healthGlobe.x >= 0.0F);
        CHECK(console.manaGlobe.x > console.healthGlobe.x);
        CHECK(console.manaGlobe.x < console.healthGlobe.x + console.healthGlobe.width);
        CHECK(console.healthGlobe.x + console.healthGlobe.width < console.skillSlots[0].x);
        CHECK(console.skillSlots[0].x >= console.manaGlobe.x + console.manaGlobe.width - 1.0F);
        CHECK(console.menuIcons[0].x > console.beltSlots[3].x + console.beltSlots[3].width - 1.0F);
        CHECK(console.menuIcons[ui::HudConsoleLayout::kMenuIconCount - 1].x +
                  console.menuIcons[ui::HudConsoleLayout::kMenuIconCount - 1].width <=
              screenW + 0.5F);
        CHECK(console.levelBadge.width > 0.0F);
        CHECK(console.levelBadge.y + console.levelBadge.height <= screenH + 0.5F);
        CHECK(console.messageStrip.y + console.messageStrip.height <= console.panel.y);

        for (int slot = 1; slot < ui::HudConsoleLayout::kSkillSlotCount; ++slot) {
            CHECK(console.skillSlots[slot].x >= console.skillSlots[slot - 1].x + console.skillSlots[slot - 1].width);
        }
        CHECK(console.beltSlots[0].x > console.skillSlots[3].x + console.skillSlots[3].width);

        const ui::HudChromeLayout chrome = ui::computeHudChromeLayout(scale);
        CHECK(chrome.statusHud.y == Catch::Approx(console.panel.y));

        const ui::MinimapWidgetLayout minimap = ui::computeMinimapWidgetLayout(
            scale, ui::ScreenAnchor::BottomRight, 12.0F, 12.0F, 220.0F);
        CHECK(minimap.frame.y + minimap.frame.height <= console.panel.y + 0.01F);
    }
}

TEST_CASE("Difficulty tiers multiply depth scaling and unlock through boss kills", "[arpg][difficulty]") {
    systems::RunProgression run(1U);
    const systems::DifficultyModifiers normal = run.modifiers();
    CHECK(run.tier() == systems::DifficultyTier::Normal);
    CHECK(run.isTierUnlocked(systems::DifficultyTier::Normal));
    CHECK_FALSE(run.isTierUnlocked(systems::DifficultyTier::Nightmare));

    run.setTier(systems::DifficultyTier::Hell);
    const systems::DifficultyModifiers hell = run.modifiers();
    CHECK(hell.mobHpMultiplier > normal.mobHpMultiplier * 3.0F);
    CHECK(hell.mobDamageMultiplier > normal.mobDamageMultiplier * 2.0F);
    CHECK(hell.lootTierBonus > normal.lootTierBonus);
    CHECK(hell.itemLevel == normal.itemLevel + 7);
    CHECK(hell.mobSpawnBudget > normal.mobSpawnBudget);

    run.onBossDefeated();
    CHECK(run.isTierUnlocked(systems::DifficultyTier::Nightmare));
    CHECK_FALSE(run.isTierUnlocked(systems::DifficultyTier::Hell));
    run.onBossDefeated();
    run.onBossDefeated();
    CHECK(run.isTierUnlocked(systems::DifficultyTier::Hell));

    CHECK(std::string(systems::difficultyTierLabel(systems::DifficultyTier::Nightmare)) == "Nightmare");
    CHECK(systems::difficultyTierFromIndex(99) == systems::DifficultyTier::Hell);
    CHECK(systems::difficultyTierFromIndex(-4) == systems::DifficultyTier::Normal);
}

TEST_CASE("Item comparison lines report signed deltas versus equipped gear", "[arpg][items]") {
    systems::ItemMetadata equipped{};
    equipped.itemId = 4500U;
    equipped.name = "Old Blade";
    equipped.category = systems::ItemCategory::Weapon;
    equipped.bonuses = {4, 0, 0, 0, 0.1F, 6, 0.0F};

    systems::ItemMetadata candidate = equipped;
    candidate.itemId = 4501U;
    candidate.name = "New Blade";
    candidate.bonuses = {7, 2, 0, 0, 0.1F, 3, 0.0F};

    const systems::ItemStatBonuses delta = systems::compareItemBonuses(candidate, equipped);
    CHECK(delta.strength == 3);
    CHECK(delta.dexterity == 2);
    CHECK(delta.damage == -3);
    CHECK(delta.attackSpeed == Catch::Approx(0.0F).margin(1e-4F));

    const std::vector<std::string> lines = systems::formatItemComparisonLines(candidate, equipped);
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "vs Old Blade:");
    CHECK(lines[1] == "+3 Strength");
    CHECK(lines[2] == "+2 Dexterity");
    CHECK(lines[3] == "-3 Damage");

    CHECK(systems::formatItemComparisonLines(equipped, equipped).empty());

    systems::ItemMetadata socketed = candidate;
    socketed.sockets = 2;
    const std::vector<std::string> statLines = systems::formatItemStatLines(socketed);
    bool foundSockets = false;
    for (const std::string& line : statLines) {
        if (line.find("2 empty sockets") != std::string::npos) {
            foundSockets = true;
        }
    }
    CHECK(foundSockets);
}
