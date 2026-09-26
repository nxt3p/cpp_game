#pragma once

#include <cstdint>

namespace ui {

/// Where the shell is running. Mobile and browser raise hit-target minimums.
enum class UiPlatformKind : std::uint8_t {
    Desktop,
    Mobile,
    Browser,
};

[[nodiscard]] UiPlatformKind detectUiPlatform() noexcept;

/// Maps layout coordinates authored at 1280x720 to the current framebuffer size.
struct UiScale {
    int width{1280};
    int height{720};
    float scaleX{1.0F};
    float scaleY{1.0F};
    float uniform{1.0F};
    UiPlatformKind platform{UiPlatformKind::Desktop};
    /// Extra multiplier on inventory slot clamps. Desktop stays at 1.
    float touchBoost{1.0F};

    UiScale() = default;
    UiScale(
        int framebufferWidth,
        int framebufferHeight,
        UiPlatformKind platform = detectUiPlatform());

    /// Absolute pixel floor for a touch control on this platform.
    [[nodiscard]] float minTouchTarget() const noexcept;

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
