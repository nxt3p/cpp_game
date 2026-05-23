#pragma once

struct GLFWwindow;

namespace engine {

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] GLFWwindow* handle() const noexcept { return window_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool shouldClose() const;
    void pollEvents() const;
    void swapBuffers() const;
    void setWindowSize(int width, int height);

    using FramebufferResizeCallback = void (*)(int width, int height, void* userData);
    void setFramebufferResizeCallback(FramebufferResizeCallback callback, void* userData);

private:
    static void onFramebufferResize(GLFWwindow* window, int width, int height);
    void initializeGlfw();
    void createContext();
    void initializeGlew();

    GLFWwindow* window_{nullptr};
    int width_{0};
    int height_{0};
    const char* title_{nullptr};
    FramebufferResizeCallback resizeCallback_{nullptr};
    void* resizeUserData_{nullptr};
};

} // namespace engine
