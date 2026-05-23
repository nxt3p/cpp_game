#include "Window.hpp"

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace engine {

namespace {

void glfwErrorCallback(int error, const char* description) {
    throw std::runtime_error(
        "GLFW error " + std::to_string(error) + ": " + (description ? description : "unknown"));
}

} // namespace

Window::Window(int width, int height, const char* title)
    : width_(width), height_(height), title_(title) {
    initializeGlfw();
    createContext();
    initializeGlew();
}

Window::~Window() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::swapBuffers() const {
    glfwSwapBuffers(window_);
}

void Window::setWindowSize(const int width, const int height) {
    if (window_ == nullptr) {
        return;
    }

    width_ = std::max(width, 640);
    height_ = std::max(height, 480);
    glfwSetWindowSize(window_, width_, height_);

    int framebufferWidth = width_;
    int framebufferHeight = height_;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    width_ = std::max(framebufferWidth, 1);
    height_ = std::max(framebufferHeight, 1);
    glViewport(0, 0, width_, height_);

    if (resizeCallback_ != nullptr) {
        resizeCallback_(width_, height_, resizeUserData_);
    }
}

void Window::initializeGlfw() {
    glfwSetErrorCallback(glfwErrorCallback);

    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
}

void Window::createContext() {
    window_ = glfwCreateWindow(width_, height_, title_, nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
#ifndef __EMSCRIPTEN__
    glfwSwapInterval(1);
#endif
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, onFramebufferResize);

    int framebufferWidth = width_;
    int framebufferHeight = height_;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    width_ = std::max(framebufferWidth, 1);
    height_ = std::max(framebufferHeight, 1);
    glViewport(0, 0, width_, height_);
}

void Window::setFramebufferResizeCallback(FramebufferResizeCallback callback, void* userData) {
    resizeCallback_ = callback;
    resizeUserData_ = userData;
}

void Window::onFramebufferResize(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr) {
        return;
    }

    self->width_ = std::max(width, 1);
    self->height_ = std::max(height, 1);
    glViewport(0, 0, self->width_, self->height_);

    if (self->resizeCallback_ != nullptr) {
        self->resizeCallback_(self->width_, self->height_, self->resizeUserData_);
    }
}

void Window::initializeGlew() {
#ifndef __EMSCRIPTEN__
    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        throw std::runtime_error(
            std::string("Failed to initialize GLEW: ") +
            reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
    }
#endif

    glEnable(GL_DEPTH_TEST);
    ENGINE_GL_CHECK();
}

} // namespace engine
