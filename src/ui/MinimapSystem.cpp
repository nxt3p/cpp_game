#include "ui/MinimapSystem.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

void MinimapSystem::setViewport(const Rect2D& viewport) noexcept {
    viewport_ = viewport;
}

void MinimapSystem::setViewRadius(const float worldRadius) noexcept {
    viewRadius_ = std::max(worldRadius, 1.0F);
}

MinimapLayer MinimapSystem::buildLayer(
    const float playerX,
    const float playerZ,
    const std::vector<MinimapBlip>& blips) const {
    MinimapLayer layer{};
    layer.viewport = viewport_;

    const float pixelRadius =
        std::max(0.0F, std::min(viewport_.width, viewport_.height) * 0.5F - 1.0F);
    layer.center.x = viewport_.x + viewport_.width * 0.5F;
    layer.center.y = viewport_.y + viewport_.height * 0.5F;
    layer.radiusPixels = pixelRadius;

    layer.player.pixel = layer.center;
    layer.player.inBounds = true;

    const float radius = std::max(viewRadius_, 1.0F);
    const float radiusSq = radius * radius;
    layer.entities.reserve(blips.size());
    for (const MinimapBlip& blip : blips) {
        const float dx = blip.x - playerX;
        const float dz = blip.z - playerZ;
        if (dx * dx + dz * dz > radiusSq) {
            continue;
        }

        MinimapMarker marker{};
        marker.kind = blip.kind;
        marker.inBounds = true;
        marker.pixel.x = layer.center.x + (dx / radius) * pixelRadius;
        marker.pixel.y = layer.center.y + (dz / radius) * pixelRadius;
        layer.entities.push_back(marker);
    }

    return layer;
}

} // namespace ui
