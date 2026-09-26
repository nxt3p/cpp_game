#pragma once

#include "ui/UiTypes.hpp"

#include <cstdint>
#include <vector>

namespace ui {

/// What a local-radar blip represents. Decorative props are not submitted.
enum class MinimapBlipKind : std::uint8_t {
    Hostile,
    Boss,
    Ally,
    Loot,
    Landmark,
};

struct MinimapBlip {
    float x{0.0F};
    float z{0.0F};
    MinimapBlipKind kind{MinimapBlipKind::Hostile};
};

struct MinimapMarker {
    Vec2 pixel{0.0F, 0.0F};
    MinimapBlipKind kind{MinimapBlipKind::Hostile};
    bool inBounds{false};
};

struct MinimapLayer {
    Rect2D viewport{};
    Vec2 center{0.0F, 0.0F};
    float radiusPixels{0.0F};
    MinimapMarker player{};
    std::vector<MinimapMarker> entities;
};

/// Local circular radar. The player is the center; blips outside `viewRadius` are dropped.
class MinimapSystem {
public:
    void setViewport(const Rect2D& viewport) noexcept;
    void setViewRadius(float worldRadius) noexcept;

    [[nodiscard]] float viewRadius() const noexcept { return viewRadius_; }

    [[nodiscard]] MinimapLayer buildLayer(float playerX, float playerZ, const std::vector<MinimapBlip>& blips) const;

private:
    Rect2D viewport_{0.0F, 0.0F, 220.0F, 220.0F};
    float viewRadius_{46.0F};
};

} // namespace ui
