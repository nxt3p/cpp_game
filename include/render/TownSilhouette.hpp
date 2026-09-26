#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace render {

/// Douglas–Peucker tolerance in texels. Large enough to drop stair-step
/// jaggies, small enough that a roof peak stays a peak.
inline constexpr float kTownContourSimplify = 2.0F;
inline constexpr int kTownOpaqueAlpha = 32;

struct TownSilhouette {
    int width{0};
    int height{0};
    /// Closed outer contour of the largest opaque component, image space, y down.
    /// Pairs of x,y sitting on the alpha edge (pixel centers). The first point is
    /// not repeated. Empty when the sprite has no opaque pixels. This is the
    /// silhouette, not the texture bounds and not the opaque AABB.
    std::vector<float> contour{};
    /// Source alpha, one byte per pixel, image top at row 0.
    std::vector<std::uint8_t> alpha{};
};

namespace town_silhouette_detail {

inline constexpr int kDirX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr int kDirY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

[[nodiscard]] inline float pointSegmentDistance2(
    const float px,
    const float py,
    const float ax,
    const float ay,
    const float bx,
    const float by) noexcept {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float ab2 = abx * abx + aby * aby;
    float t = 0.0F;
    if (ab2 > 1.0e-8F) {
        t = (apx * abx + apy * aby) / ab2;
    }
    t = std::clamp(t, 0.0F, 1.0F);
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    return dx * dx + dy * dy;
}

inline void simplifyClosed(std::vector<float>& points, const float epsilon) {
    const std::size_t count = points.size() / 2U;
    if (count < 8U || epsilon <= 0.0F) {
        return;
    }
    const float epsilon2 = epsilon * epsilon;
    std::vector<char> keep(count, 0);
    keep[0] = 1;
    std::size_t farthest = 1U;
    float farthestDistance = -1.0F;
    const float originX = points[0];
    const float originY = points[1];
    for (std::size_t index = 1; index < count; ++index) {
        const float dx = points[index * 2U] - originX;
        const float dy = points[index * 2U + 1U] - originY;
        const float distance = dx * dx + dy * dy;
        if (distance > farthestDistance) {
            farthestDistance = distance;
            farthest = index;
        }
    }
    keep[farthest] = 1;

    const auto at = [&](const std::size_t index) {
        const std::size_t wrapped = index == count ? 0U : index;
        return std::pair<float, float>{points[wrapped * 2U], points[wrapped * 2U + 1U]};
    };

    struct Range {
        std::size_t begin{0};
        std::size_t end{0};
    };
    std::vector<Range> stack;
    stack.push_back(Range{0U, farthest});
    stack.push_back(Range{farthest, count});
    while (!stack.empty()) {
        const Range range = stack.back();
        stack.pop_back();
        if (range.end <= range.begin + 1U) {
            continue;
        }
        const auto [ax, ay] = at(range.begin);
        const auto [bx, by] = at(range.end);
        float maxDistance = 0.0F;
        std::size_t maxIndex = range.begin;
        for (std::size_t index = range.begin + 1U; index < range.end; ++index) {
            const float distance = pointSegmentDistance2(points[index * 2U], points[index * 2U + 1U], ax, ay, bx, by);
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = index;
            }
        }
        if (maxDistance <= epsilon2) {
            continue;
        }
        keep[maxIndex] = 1;
        stack.push_back(Range{range.begin, maxIndex});
        stack.push_back(Range{maxIndex, range.end});
    }

    std::vector<float> simplified;
    simplified.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (keep[index] == 0) {
            continue;
        }
        simplified.push_back(points[index * 2U]);
        simplified.push_back(points[index * 2U + 1U]);
    }
    if (simplified.size() >= 6U) {
        points.swap(simplified);
    }
}

} // namespace town_silhouette_detail

/// Outer Moore contour of the largest 4-connected opaque component.
/// `simplifyEpsilon` drops pixel-stair noise; it does not replace the shape with its AABB.
[[nodiscard]] inline TownSilhouette buildTownSilhouette(
    const std::uint8_t* pixels,
    const int width,
    const int height,
    const float simplifyEpsilon = kTownContourSimplify,
    const bool downsample = true) {
    TownSilhouette result;
    result.width = std::max(0, width);
    result.height = std::max(0, height);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return result;
    }

    const int count = width * height;
    result.alpha.assign(static_cast<std::size_t>(count), 0);
    std::vector<char> opaque(static_cast<std::size_t>(count), 0);
    for (int index = 0; index < count; ++index) {
        const auto alpha = pixels[static_cast<std::size_t>(index) * 4U + 3U];
        result.alpha[static_cast<std::size_t>(index)] = alpha;
        if (alpha >= kTownOpaqueAlpha) {
            opaque[static_cast<std::size_t>(index)] = 1;
        }
    }

    // Trace a max-pooled mask when the plate is large. Full-resolution alpha stays
    // for hit tests. Step 2 on a 1024px edge keeps the roof peak within a texel of
    // the true tip, which the silhouette tests require.
    if (downsample && (width > 512 || height > 512)) {
        const int step = std::max((width + 511) / 512, (height + 511) / 512);
        const int smallWidth = (width + step - 1) / step;
        const int smallHeight = (height + step - 1) / step;
        std::vector<std::uint8_t> small(
            static_cast<std::size_t>(smallWidth) * static_cast<std::size_t>(smallHeight) * 4U, 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto alpha =
                    pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U + 3U];
                if (alpha < kTownOpaqueAlpha) {
                    continue;
                }
                const int sx = x / step;
                const int sy = y / step;
                std::uint8_t& pooled = small[(static_cast<std::size_t>(sy) * static_cast<std::size_t>(smallWidth) +
                                               static_cast<std::size_t>(sx)) *
                                              4U +
                                          3U];
                if (alpha > pooled) {
                    pooled = alpha;
                }
            }
        }
        const float coarseEpsilon = std::max(1.25F, simplifyEpsilon / static_cast<float>(step));
        TownSilhouette coarse = buildTownSilhouette(small.data(), smallWidth, smallHeight, coarseEpsilon, false);
        for (float& coord : coarse.contour) {
            coord *= static_cast<float>(step);
        }
        coarse.alpha = std::move(result.alpha);
        coarse.width = width;
        coarse.height = height;
        return coarse;
    }

    std::vector<int> labels(static_cast<std::size_t>(count), -1);
    std::vector<int> stack;
    stack.reserve(1024U);
    int bestLabel = -1;
    int bestCount = 0;
    int nextLabel = 0;
    constexpr int kFourX[4] = {1, 0, -1, 0};
    constexpr int kFourY[4] = {0, 1, 0, -1};
    for (int index = 0; index < count; ++index) {
        if (opaque[static_cast<std::size_t>(index)] == 0 || labels[static_cast<std::size_t>(index)] >= 0) {
            continue;
        }
        int found = 0;
        labels[static_cast<std::size_t>(index)] = nextLabel;
        stack.clear();
        stack.push_back(index);
        while (!stack.empty()) {
            const int current = stack.back();
            stack.pop_back();
            ++found;
            const int x = current % width;
            const int y = current / width;
            for (int dir = 0; dir < 4; ++dir) {
                const int nx = x + kFourX[dir];
                const int ny = y + kFourY[dir];
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                    continue;
                }
                const int neighbor = ny * width + nx;
                if (opaque[static_cast<std::size_t>(neighbor)] == 0 || labels[static_cast<std::size_t>(neighbor)] >= 0) {
                    continue;
                }
                labels[static_cast<std::size_t>(neighbor)] = nextLabel;
                stack.push_back(neighbor);
            }
        }
        if (found > bestCount) {
            bestCount = found;
            bestLabel = nextLabel;
        }
        ++nextLabel;
    }
    if (bestLabel < 0) {
        return result;
    }

    int startX = 0;
    int startY = 0;
    bool foundStart = false;
    for (int y = 0; y < height && !foundStart; ++y) {
        for (int x = 0; x < width; ++x) {
            if (labels[static_cast<std::size_t>(y * width + x)] != bestLabel) {
                continue;
            }
            startX = x;
            startY = y;
            foundStart = true;
            break;
        }
    }
    if (!foundStart) {
        return result;
    }

    const auto inComponent = [&](const int x, const int y) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return false;
        }
        return labels[static_cast<std::size_t>(y * width + x)] == bestLabel;
    };

    std::vector<float> contour;
    contour.reserve(static_cast<std::size_t>(std::min(count, 4096)));
    contour.push_back(static_cast<float>(startX) + 0.5F);
    contour.push_back(static_cast<float>(startY) + 0.5F);

    int cx = startX;
    int cy = startY;
    int backDir = 4;
    int firstDepart = -1;
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(static_cast<std::size_t>(std::min(count, 8192)));
    const int stepLimit = std::max(8, count);
    for (int step = 0; step < stepLimit; ++step) {
        const std::uint64_t state = (static_cast<std::uint64_t>(cx) << 21) |
            (static_cast<std::uint64_t>(cy) << 3) | static_cast<std::uint64_t>(backDir & 7);
        if (!seen.insert(state).second) {
            break;
        }

        int foundDir = -1;
        int nx = cx;
        int ny = cy;
        for (int turn = 1; turn <= 8; ++turn) {
            const int dir = (backDir + turn) & 7;
            const int tx = cx + town_silhouette_detail::kDirX[dir];
            const int ty = cy + town_silhouette_detail::kDirY[dir];
            if (!inComponent(tx, ty)) {
                continue;
            }
            foundDir = dir;
            nx = tx;
            ny = ty;
            break;
        }
        if (foundDir < 0) {
            break;
        }
        if (cx == startX && cy == startY && foundDir == firstDepart) {
            break;
        }
        if (firstDepart < 0) {
            firstDepart = foundDir;
        }
        cx = nx;
        cy = ny;
        backDir = (foundDir + 4) & 7;
        if (cx == startX && cy == startY) {
            continue;
        }
        contour.push_back(static_cast<float>(cx) + 0.5F);
        contour.push_back(static_cast<float>(cy) + 0.5F);
    }

    if (contour.size() < 6U) {
        return result;
    }

    double area = 0.0;
    const std::size_t pointCount = contour.size() / 2U;
    for (std::size_t index = 0; index < pointCount; ++index) {
        const std::size_t next = (index + 1U) % pointCount;
        area += static_cast<double>(contour[index * 2U]) * static_cast<double>(contour[next * 2U + 1U]) -
            static_cast<double>(contour[next * 2U]) * static_cast<double>(contour[index * 2U + 1U]);
    }
    if (area < 0.0) {
        std::vector<float> reversed;
        reversed.reserve(contour.size());
        for (std::size_t index = pointCount; index-- > 0;) {
            reversed.push_back(contour[index * 2U]);
            reversed.push_back(contour[index * 2U + 1U]);
        }
        contour.swap(reversed);
    }

    town_silhouette_detail::simplifyClosed(contour, simplifyEpsilon);
    result.contour = std::move(contour);
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
