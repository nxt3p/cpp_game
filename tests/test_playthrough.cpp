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

[[nodiscard]] ui::Rect mainMenuPlayButton(const int width, const int height) {
    const float centerX = static_cast<float>(width) * 0.5F;
    const float startY = static_cast<float>(height) * 0.42F;
    return {centerX - 140.0F, startY, 280.0F, 56.0F};
}

[[nodiscard]] ui::Rect characterSelectWarriorBox(const int width, const int height) {
    const float boxW = 220.0F;
    const float boxH = 280.0F;
    const float gap = 36.0F;
    const float totalWidth = boxW * 3.0F + gap * 2.0F;
    const float startX = static_cast<float>(width) * 0.5F - totalWidth * 0.5F;
    const float panelY = static_cast<float>(height) * 0.35F;
    return {startX, panelY, boxW, boxH};
}

[[nodiscard]] float rectCenterX(const ui::Rect& rect) {
    return rect.x + rect.width * 0.5F;
}

[[nodiscard]] float rectCenterY(const ui::Rect& rect) {
    return rect.y + rect.height * 0.5F;
}

void clickRect(game::GameApplication& application, const ui::Rect& rect) {
    application.simulateMouseClick(rectCenterX(rect), rectCenterY(rect));
    application.tickOneFrame(kFrameDelta);
    REQUIRE(glGetError() == GL_NO_ERROR);
}

void hoverWorld(
    game::GameApplication& application,
    const float worldX,
    const float worldY,
    const float worldZ) {
    float screenX = 0.0F;
    float screenY = 0.0F;
    REQUIRE(application.projectWorldToScreen(worldX, worldY, worldZ, screenX, screenY));
    application.simulateMouseMove(screenX, screenY);
    tickFrames(application, 8);
}

} // namespace

TEST_CASE("Full gameplay playthrough exercises core systems", "[playthrough][opengl]") {
    if (!test_gl::hasDisplayServer()) {
        SKIP("No DISPLAY or WAYLAND_DISPLAY; skipping playthrough test.");
    }
    if (!test_gl::canCreateOpenGLContext()) {
        SKIP("GLFW could not create an OpenGL context; skipping playthrough test.");
    }

    engine::Window window(1280, 720, "PlaythroughTest");
    game::GameApplication application(window, test_gl::assetsRoot());

    tickFrames(application, 2);
    REQUIRE(application.currentScreen() == game::AppScreen::MAIN_MENU);

    clickRect(application, mainMenuPlayButton(window.width(), window.height()));
    REQUIRE(application.currentScreen() == game::AppScreen::CHARACTER_SELECT);

    clickRect(application, characterSelectWarriorBox(window.width(), window.height()));
    REQUIRE(application.currentScreen() == game::AppScreen::IN_GAME);
    REQUIRE(application.activeWorldZone() == gameplay::WorldZone::TOWN);

    tickFrames(application, 30);

    SECTION("Interactable hover outline (chest) does not crash") {
        hoverWorld(application, -5.0F, 0.0F, -4.0F);
    }

    application.simulateKeyPress(GLFW_KEY_C);
    application.tickOneFrame(kFrameDelta);
    REQUIRE(glGetError() == GL_NO_ERROR);

    application.simulateKeyPress(GLFW_KEY_I);
    tickFrames(application, 8);

    application.simulateKeyPress(GLFW_KEY_C);
    application.simulateKeyPress(GLFW_KEY_I);
    tickFrames(application, 4);

    float gateScreenX = 0.0F;
    float gateScreenY = 0.0F;
    if (application.projectWorldToScreen(0.0F, 0.0F, 39.0F, gateScreenX, gateScreenY)) {
        application.simulateMouseClick(gateScreenX, gateScreenY);
        tickFrames(application, 120);
    }

    if (application.activeWorldZone() != gameplay::WorldZone::PLAINS) {
        application.setPlayerWorldPositionForTest(0.0F, 39.5F);
        application.tickOneFrame(kFrameDelta);
    }
    REQUIRE(application.activeWorldZone() == gameplay::WorldZone::PLAINS);

    tickFrames(application, 45);

    const int experienceBefore = application.characterExperience();

    REQUIRE(application.engageNearestMobForTest());

    for (int frame = 0; frame < 600; ++frame) {
        if (frame % 40 == 0) {
            static_cast<void>(application.engageNearestMobForTest());
        }
        application.tickOneFrame(kFrameDelta);
        REQUIRE(glGetError() == GL_NO_ERROR);
        if (application.characterExperience() > experienceBefore) {
            break;
        }
    }
    CHECK(application.characterExperience() > experienceBefore);

    application.simulateKeyPress(GLFW_KEY_ESCAPE);
    application.tickOneFrame(kFrameDelta);
    REQUIRE(application.isGamePaused());

    application.simulateKeyPress(GLFW_KEY_ESCAPE);
    application.tickOneFrame(kFrameDelta);
    REQUIRE_FALSE(application.isGamePaused());

    glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
    window.pollEvents();
    CHECK(glGetError() == GL_NO_ERROR);
}
