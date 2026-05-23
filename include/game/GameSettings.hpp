#pragma once

#include <cstdint>

namespace game {

enum class MinimapAnchor : std::uint8_t {
    TopRight,
    TopLeft,
    BottomRight,
    BottomLeft
};

struct GameSettings {
    int resolutionWidth{1280};
    int resolutionHeight{720};
    float masterVolume{0.8F};
    float mouseSensitivity{1.0F};
    int graphicsQuality{1};
    float minimapSize{220.0F};
    MinimapAnchor minimapAnchor{MinimapAnchor::TopRight};
    float minimapMarginX{12.0F};
    float minimapMarginY{12.0F};
};

[[nodiscard]] const char* minimapAnchorLabel(MinimapAnchor anchor) noexcept;
[[nodiscard]] const char* graphicsQualityLabel(int qualityIndex) noexcept;
void cycleResolution(GameSettings& settings) noexcept;
void cycleMinimapAnchor(GameSettings& settings) noexcept;
void cycleGraphicsQuality(GameSettings& settings) noexcept;

} // namespace game
