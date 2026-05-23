#pragma once

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <string>

namespace test_gl {

[[nodiscard]] inline bool hasDisplayServer() noexcept {
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    return (display != nullptr && display[0] != '\0') ||
           (wayland != nullptr && wayland[0] != '\0');
}

[[nodiscard]] inline bool canCreateOpenGLContext() {
    if (glfwInit() == GLFW_FALSE) {
        return false;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* probe = glfwCreateWindow(64, 64, "gl_probe", nullptr, nullptr);
    if (probe == nullptr) {
        glfwTerminate();
        return false;
    }

    glfwDestroyWindow(probe);
    glfwTerminate();
    return true;
}

[[nodiscard]] inline std::string assetsRoot() {
#ifdef ENGINE_ASSETS_DIR
    return ENGINE_ASSETS_DIR;
#else
    return "assets";
#endif
}

} // namespace test_gl
