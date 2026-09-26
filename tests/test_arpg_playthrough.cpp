#include <catch2/catch_test_macros.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "game/GameApplication.hpp"
#include "gameplay/GameTypes.hpp"
#include "ui/UiHitTest.hpp"
#include "Window.hpp"

#include "test_gl_helpers.hpp"

namespace {

constexpr float kFrameDelta = 1.0F / 60.0F;

void tickFrames(game::GameApplication& application, const int frameCount) {
    for (int frame = 0; frame < frameCount; ++frame) {
        application.tickOneFrame(kFrameDelta);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
}

void clickRect(game::GameApplication& application, const ui::Rect& rect) {
    application.simulateMouseClick(rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F);
    application.tickOneFrame(kFrameDelta);
    REQUIRE(glGetError() == GL_NO_ERROR);
}

void bootIntoPlains(game::GameApplication& application, const int width, const int height) {
    tickFrames(application, 2);
    REQUIRE(application.currentScreen() == game::AppScreen::MAIN_MENU);

    const float centerX = static_cast<float>(width) * 0.5F;
    clickRect(application, {centerX - 140.0F, static_cast<float>(height) * 0.42F, 280.0F, 56.0F});
    REQUIRE(application.currentScreen() == game::AppScreen::CHARACTER_SELECT);

    const float totalWidth = 220.0F * 3.0F + 36.0F * 2.0F;
    const float startX = centerX - totalWidth * 0.5F;
    clickRect(application, {startX, static_cast<float>(height) * 0.35F, 220.0F, 280.0F});
    REQUIRE(application.currentScreen() == game::AppScreen::IN_GAME);

    tickFrames(application, 10);
    application.setPlayerWorldPositionForTest(0.0F, 39.5F);
    application.tickOneFrame(kFrameDelta);
    REQUIRE(application.activeWorldZone() == gameplay::WorldZone::PLAINS);
    tickFrames(application, 20);
}

} // namespace

TEST_CASE("ARPG loop: skills, hit feedback and slot-machine loot render without GL errors", "[arpg][opengl]") {
    if (!test_gl::hasDisplayServer()) {
        SKIP("No DISPLAY or WAYLAND_DISPLAY; skipping ARPG playthrough test.");
    }
    if (!test_gl::canCreateOpenGLContext()) {
        SKIP("GLFW could not create an OpenGL context; skipping ARPG playthrough test.");
    }

    engine::Window window(1280, 720, "ArpgPlaythroughTest");
    game::GameApplication application(window, test_gl::assetsRoot());
    bootIntoPlains(application, window.width(), window.height());

    SECTION("Quick-cast hotkeys spend mana and spawn particles") {
        const int manaBefore = application.playerMana();
        REQUIRE(manaBefore == application.playerMaxMana());

        application.simulateKeyPress(GLFW_KEY_3); // Heal: castable without a target
        application.tickOneFrame(kFrameDelta);
        REQUIRE(glGetError() == GL_NO_ERROR);
        CHECK(application.playerMana() < manaBefore);
        CHECK(application.activeParticleCount() > 0);

        application.simulateKeyPress(GLFW_KEY_4); // Dash
        application.tickOneFrame(kFrameDelta);
        REQUIRE(glGetError() == GL_NO_ERROR);
        CHECK(application.cameraTrauma() > 0.0F);

        tickFrames(application, 60);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }

    SECTION("Melee combat produces shake, sparks and eventually kills") {
        REQUIRE(application.engageNearestMobForTest());
        bool sawTrauma = false;
        bool sawParticles = false;
        const int soulsBefore = application.characterExperience();
        for (int frame = 0; frame < 900; ++frame) {
            if (frame % 40 == 0) {
                static_cast<void>(application.engageNearestMobForTest());
            }
            if (frame % 90 == 45) {
                application.simulateKeyPress(GLFW_KEY_1); // Power Strike
            }
            if (frame % 120 == 60) {
                application.simulateKeyPress(GLFW_KEY_2); // Whirlwind
            }
            application.tickOneFrame(kFrameDelta);
            REQUIRE(glGetError() == GL_NO_ERROR);
            sawTrauma = sawTrauma || application.cameraTrauma() > 0.0F;
            sawParticles = sawParticles || application.activeParticleCount() > 0;
            if (application.characterExperience() > soulsBefore) {
                break;
            }
        }
        CHECK(sawTrauma);
        CHECK(sawParticles);
        CHECK(application.characterExperience() > soulsBefore);
        CHECK(application.lootSpinCount() > 0);
    }

    SECTION("Opening 1,000 virtual chests keeps the frame loop stable") {
        const int spinsBefore = application.lootSpinCount();
        const int jackpots = application.openVirtualChestsForTest(1000);
        CHECK(application.lootSpinCount() - spinsBefore == 1000);
        CHECK(jackpots > 5);
        CHECK(jackpots < 60);
        CHECK(application.lootJackpotCount() >= jackpots);

        // Render through the resulting particle storm and floating text.
        for (int frame = 0; frame < 180; ++frame) {
            application.tickOneFrame(kFrameDelta);
            REQUIRE(glGetError() == GL_NO_ERROR);
        }
        CHECK(application.playerInventoryUsedSlots() > 0);
    }

    SECTION("Pause and settings panel render with the difficulty row") {
        application.simulateKeyPress(GLFW_KEY_ESCAPE);
        application.tickOneFrame(kFrameDelta);
        REQUIRE(application.isGamePaused());
        tickFrames(application, 5);
        application.simulateKeyPress(GLFW_KEY_ESCAPE);
        application.tickOneFrame(kFrameDelta);
        REQUIRE_FALSE(application.isGamePaused());
        CHECK(application.difficultyTierIndex() == 0);
    }

    glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
    window.pollEvents();
    CHECK(glGetError() == GL_NO_ERROR);
}
