#pragma once

#include "ui/UiTypes.hpp"

#include <vector>

namespace ui {

struct TerrainBounds {
    float minX{0.0F};
    float maxX{0.0F};
    float minZ{0.0F};
    float maxZ{0.0F};
};

struct MinimapMarker {
    Vec2 normalized{0.0F, 0.0F};
    Vec2 pixel{0.0F, 0.0F};
    bool inBounds{false};
};

struct MinimapLayer {
    Rect2D viewport{};
    MinimapMarker player{};
    std::vector<MinimapMarker> entities;
};

class MinimapSystem {
public:
    void setViewport(const Rect2D& viewport) noexcept;
    void setTerrainBounds(const TerrainBounds& bounds) noexcept;

    [[nodiscard]] MinimapLayer buildLayer(
        float playerX,
        float playerZ,
        const std::vector<Vec2>& entityPositions) const;

private:
    [[nodiscard]] Vec2 worldToNormalized(float worldX, float worldZ) const noexcept;
    [[nodiscard]] Vec2 normalizedToPixel(const Vec2& normalized) const noexcept;

    Rect2D viewport_{0.0F, 0.0F, 220.0F, 220.0F};
    TerrainBounds terrain_{-120.0F, 120.0F, -40.0F, 200.0F};
};

} // namespace ui
