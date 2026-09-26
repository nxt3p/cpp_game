#pragma once

#include <algorithm>
#include <cmath>

namespace ui {

/// Half-width of a disc at a vertical offset from its center. Zero outside the radius.
[[nodiscard]] inline float discHalfWidth(const float radius, const float deltaY) noexcept {
    const float remaining = radius * radius - deltaY * deltaY;
    if (remaining <= 0.0F) {
        return 0.0F;
    }
    return std::sqrt(remaining);
}

/// Top of a bottom-up liquid fill. The sine bob is clamped so the surface stays inside the disc.
[[nodiscard]] inline float liquidSurfaceY(
    const float centerY,
    const float radius,
    const float fillRatio,
    const float wavePhase,
    const float waveAmplitude) noexcept {
    const float clamped = std::clamp(fillRatio, 0.0F, 1.0F);
    const float bob = std::sin(wavePhase) * waveAmplitude;
    const float surface = centerY + radius * (1.0F - 2.0F * clamped) - bob;
    return std::clamp(surface, centerY - radius, centerY + radius);
}

} // namespace ui
