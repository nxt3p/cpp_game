#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace render {

/// Texels of gold drawn just outside opaque sprite pixels. At town scale this
/// is a few screen pixels, enough to read as a contour rather than a box.
inline constexpr float kTownSilhouetteRadius = 7.0F;
inline constexpr int kTownOpaqueAlpha = 32;

struct TownSilhouette {
    int width{0};
    int height{0};
    /// RGBA, image top at row 0. Gold only on transparent texels near the sprite.
    std::vector<std::uint8_t> outline{};
    /// Source alpha, one byte per pixel, image top at row 0.
    std::vector<std::uint8_t> alpha{};
};

/// Euclidean contour. Opaque texels stay transparent in the outline so the
/// stroke sits on the silhouette edge and ignores empty canvas padding.
[[nodiscard]] inline TownSilhouette buildTownSilhouette(
    const std::uint8_t* pixels,
    const int width,
    const int height,
    const float radius = kTownSilhouetteRadius) {
    TownSilhouette result;
    result.width = std::max(0, width);
    result.height = std::max(0, height);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return result;
    }
    const int count = width * height;
    result.alpha.assign(static_cast<std::size_t>(count), 0);
    std::vector<std::uint8_t> opaque(static_cast<std::size_t>(count), 0);
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            const auto alpha = pixels[static_cast<std::size_t>(index) * 4U + 3U];
            result.alpha[static_cast<std::size_t>(index)] = alpha;
            if (alpha < kTownOpaqueAlpha) {
                continue;
            }
            opaque[static_cast<std::size_t>(index)] = 1;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    result.outline.assign(static_cast<std::size_t>(count) * 4U, 0);
    if (maxX < minX) {
        return result;
    }
    const int reach = std::max(1, static_cast<int>(std::ceil(radius)));
    const float radius2 = radius * radius;
    const float solid = std::min(1.75F, radius * 0.28F);
    const int yBegin = std::max(0, minY - reach);
    const int yEnd = std::min(height - 1, maxY + reach);
    const int xBegin = std::max(0, minX - reach);
    const int xEnd = std::min(width - 1, maxX + reach);
    for (int y = yBegin; y <= yEnd; ++y) {
        for (int x = xBegin; x <= xEnd; ++x) {
            const int index = y * width + x;
            if (opaque[static_cast<std::size_t>(index)] != 0) {
                continue;
            }
            float best = radius2 + 1.0F;
            const int ny0 = std::max(0, y - reach);
            const int ny1 = std::min(height - 1, y + reach);
            const int nx0 = std::max(0, x - reach);
            const int nx1 = std::min(width - 1, x + reach);
            for (int ny = ny0; ny <= ny1; ++ny) {
                const int dy = ny - y;
                const int row = ny * width;
                for (int nx = nx0; nx <= nx1; ++nx) {
                    if (opaque[static_cast<std::size_t>(row + nx)] == 0) {
                        continue;
                    }
                    const int dx = nx - x;
                    const float distance2 = static_cast<float>(dx * dx + dy * dy);
                    if (distance2 < best) {
                        best = distance2;
                    }
                }
            }
            if (best > radius2) {
                continue;
            }
            const float distance = std::sqrt(best);
            float coverage = 1.0F;
            if (distance > solid) {
                coverage = 1.0F - (distance - solid) / std::max(0.01F, radius - solid);
            }
            coverage = std::clamp(coverage, 0.0F, 1.0F);
            coverage = coverage * coverage * (3.0F - 2.0F * coverage);
            const std::size_t out = static_cast<std::size_t>(index) * 4U;
            result.outline[out] = 255;
            result.outline[out + 1U] = 214;
            result.outline[out + 2U] = 82;
            result.outline[out + 3U] = static_cast<std::uint8_t>(coverage * 255.0F);
        }
    }
    return result;
}

/// True when (x, y) lands on an opaque texel of the fitted sprite. Points in
/// the transparent padding of the texture are misses.
[[nodiscard]] inline bool townSpriteOpaqueAt(
    const std::uint8_t* alpha,
    const int width,
    const int height,
    const float u0,
    const float v0,
    const float u1,
    const float v1,
    const float spriteX,
    const float spriteY,
    const float spriteW,
    const float spriteH,
    const float x,
    const float y) noexcept {
    if (alpha == nullptr || width <= 0 || height <= 0 || spriteW <= 0.0F || spriteH <= 0.0F) {
        return false;
    }
    if (x < spriteX || y < spriteY || x >= spriteX + spriteW || y >= spriteY + spriteH) {
        return false;
    }
    const float u = u0 + ((x - spriteX) / spriteW) * (u1 - u0);
    const float v = v0 + ((y - spriteY) / spriteH) * (v1 - v0);
    const int px = std::clamp(static_cast<int>(u * static_cast<float>(width)), 0, width - 1);
    const int py = std::clamp(static_cast<int>(v * static_cast<float>(height)), 0, height - 1);
    return alpha[static_cast<std::size_t>(py) * static_cast<std::size_t>(width) + static_cast<std::size_t>(px)] >=
        kTownOpaqueAlpha;
}

} // namespace render
