#pragma once

namespace ui {

/// Maps layout coordinates authored at 1280x720 to the current framebuffer size.
struct UiScale {
    int width{1280};
    int height{720};
    float scaleX{1.0F};
    float scaleY{1.0F};
    float uniform{1.0F};

    UiScale() = default;
    UiScale(int framebufferWidth, int framebufferHeight);

    [[nodiscard]] float x(float value) const noexcept { return value * scaleX; }
    [[nodiscard]] float y(float value) const noexcept { return value * scaleY; }
    [[nodiscard]] float dim(float value) const noexcept { return value * uniform; }

    [[nodiscard]] float fractionX(float normalized) const noexcept {
        return normalized * static_cast<float>(width);
    }
    [[nodiscard]] float fractionY(float normalized) const noexcept {
        return normalized * static_cast<float>(height);
    }
};

} // namespace ui
