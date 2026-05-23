#include <catch2/catch_test_macros.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "game/GameApplication.hpp"
#include "gameplay/GameTypes.hpp"
#include "gameplay/ZoneManager.hpp"
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

void enterPlainsForBenchmark(game::GameApplication& application, engine::Window& window) {
    tickFrames(application, 2);
    clickRect(application, mainMenuPlayButton(window.width(), window.height()));
    clickRect(application, characterSelectWarriorBox(window.width(), window.height()));
    REQUIRE(application.activeWorldZone() == gameplay::WorldZone::TOWN);

    application.setPlayerWorldPositionForTest(0.0F, 39.5F);
    application.tickOneFrame(kFrameDelta);
    REQUIRE(application.activeWorldZone() == gameplay::WorldZone::PLAINS);
}

} // namespace

TEST_CASE("Plains zone entry rebuilds scenery once", "[performance][zone]") {
    gameplay::ZoneManager zoneManager;
    const std::uint32_t initialRevision = zoneManager.sceneryRevision();

    const gameplay::ZoneTransitionResult transition =
        zoneManager.updatePlayerPosition(gameplay::Vec3{0.0F, 0.0F, 39.5F});
    REQUIRE(transition.transitioned);
    REQUIRE(zoneManager.activeZone() == gameplay::WorldZone::PLAINS);
    REQUIRE(zoneManager.sceneryRevision() == initialRevision + 1U);
}

TEST_CASE("Plains frame time stays within budget", "[performance][opengl]") {
    if (!test_gl::hasDisplayServer()) {
        SKIP("No DISPLAY or WAYLAND_DISPLAY; skipping performance test.");
    }
    if (!test_gl::canCreateOpenGLContext()) {
        SKIP("GLFW could not create an OpenGL context; skipping performance test.");
    }

    engine::Window window(1280, 720, "PerformanceTest");
    game::GameApplication application(window, test_gl::assetsRoot());

    enterPlainsForBenchmark(application, window);
    tickFrames(application, 30);

    const double medianFrameMs = application.runPlainsFrameBenchmarkForTest(60, 120);
    INFO("Median plains frame time: " << medianFrameMs << " ms");

    REQUIRE(medianFrameMs > 0.0);
    CHECK(application.lastBenchmarkMedianFrameMs() == medianFrameMs);

#if defined(NDEBUG)
    REQUIRE(medianFrameMs < 22.0);
#else
    REQUIRE(medianFrameMs < 55.0);
#endif
}
