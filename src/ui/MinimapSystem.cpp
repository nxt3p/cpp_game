#include "ui/MinimapSystem.hpp"

#include <algorithm>

namespace ui {

void MinimapSystem::setViewport(const Rect2D& viewport) noexcept {
    viewport_ = viewport;
}

void MinimapSystem::setTerrainBounds(const TerrainBounds& bounds) noexcept {
    terrain_ = bounds;
}

MinimapLayer MinimapSystem::buildLayer(
    float playerX,
    float playerZ,
    const std::vector<Vec2>& entityPositions) const {
    MinimapLayer layer{};
    layer.viewport = viewport_;
    layer.player.normalized = worldToNormalized(playerX, playerZ);
    layer.player.pixel = normalizedToPixel(layer.player.normalized);
    layer.player.inBounds =
        layer.player.normalized.x >= 0.0F && layer.player.normalized.x <= 1.0F &&
        layer.player.normalized.y >= 0.0F && layer.player.normalized.y <= 1.0F;

    layer.entities.reserve(entityPositions.size());
    for (const Vec2& entity : entityPositions) {
        MinimapMarker marker{};
        marker.normalized = worldToNormalized(entity.x, entity.y);
        marker.pixel = normalizedToPixel(marker.normalized);
        marker.inBounds =
            marker.normalized.x >= 0.0F && marker.normalized.x <= 1.0F &&
            marker.normalized.y >= 0.0F && marker.normalized.y <= 1.0F;
        layer.entities.push_back(marker);
    }

    return layer;
}

Vec2 MinimapSystem::worldToNormalized(float worldX, float worldZ) const noexcept {
    const float width = std::max(terrain_.maxX - terrain_.minX, 0.0001F);
    const float depth = std::max(terrain_.maxZ - terrain_.minZ, 0.0001F);

    Vec2 normalized{};
    normalized.x = (worldX - terrain_.minX) / width;
    normalized.y = (worldZ - terrain_.minZ) / depth;
    normalized.x = std::clamp(normalized.x, 0.0F, 1.0F);
    normalized.y = std::clamp(normalized.y, 0.0F, 1.0F);
    return normalized;
}

Vec2 MinimapSystem::normalizedToPixel(const Vec2& normalized) const noexcept {
    const float terrainWidth = std::max(terrain_.maxX - terrain_.minX, 0.0001F);
    const float terrainDepth = std::max(terrain_.maxZ - terrain_.minZ, 0.0001F);
    const float terrainAspect = terrainWidth / terrainDepth;
    const float viewAspect =
        viewport_.height > 0.0001F ? viewport_.width / viewport_.height : 1.0F;

    float drawWidth = viewport_.width;
    float drawHeight = viewport_.height;
    float offsetX = viewport_.x;
    float offsetY = viewport_.y;

    if (terrainAspect > viewAspect) {
        drawHeight = viewport_.width / terrainAspect;
        offsetY += (viewport_.height - drawHeight) * 0.5F;
    } else {
        drawWidth = viewport_.height * terrainAspect;
        offsetX += (viewport_.width - drawWidth) * 0.5F;
    }

    Vec2 pixel{};
    pixel.x = offsetX + normalized.x * drawWidth;
    pixel.y = offsetY + normalized.y * drawHeight;
    return pixel;
}

} // namespace ui
