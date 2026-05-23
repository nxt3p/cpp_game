#include <catch2/catch_test_macros.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "game/GameApplication.hpp"
#include "render/TextRenderer.hpp"
#include "render/UiRenderer.hpp"
#include "Window.hpp"

#include "test_gl_helpers.hpp"

TEST_CASE("GameApplication renders main menu without OpenGL errors", "[boot][opengl]") {
    if (!test_gl::hasDisplayServer()) {
        SKIP("No DISPLAY or WAYLAND_DISPLAY; skipping OpenGL boot test.");
    }
    if (!test_gl::canCreateOpenGLContext()) {
        SKIP("GLFW could not create an OpenGL context; skipping boot test.");
    }

    engine::Window window(1280, 720, "BootTest");
    game::GameApplication application(window, test_gl::assetsRoot());

    application.tickOneFrame(1.0F / 60.0F);
    application.tickOneFrame(1.0F / 60.0F);

    glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
    window.pollEvents();

    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Ui and text renderers interleave without invalid uniform uploads", "[boot][opengl]") {
    if (!test_gl::hasDisplayServer()) {
        SKIP("No DISPLAY or WAYLAND_DISPLAY; skipping OpenGL boot test.");
    }
    if (!test_gl::canCreateOpenGLContext()) {
        SKIP("GLFW could not create an OpenGL context; skipping boot test.");
    }

    engine::Window window(640, 480, "UiTextBootTest");
    const std::string assets = test_gl::assetsRoot();

    render::UiRenderer uiRenderer(assets + "/shaders/ui.vert", assets + "/shaders/ui.frag");
    render::TextRenderer textRenderer(assets + "/shaders/text.vert", assets + "/shaders/text.frag");
    uiRenderer.resize(window.width(), window.height());
    textRenderer.resize(window.width(), window.height());

    uiRenderer.beginFrame();
    const float panel[4] = {0.1F, 0.12F, 0.18F, 1.0F};
    uiRenderer.drawFilledRect(20.0F, 20.0F, 200.0F, 80.0F, panel);

    const float textColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    textRenderer.drawText(40.0F, 40.0F, "Boot", 2.0F, textColor);

    const float button[4] = {0.2F, 0.55F, 0.35F, 1.0F};
    uiRenderer.drawFilledRect(20.0F, 120.0F, 180.0F, 48.0F, button);
    uiRenderer.endFrame();

    CHECK(glGetError() == GL_NO_ERROR);
}
