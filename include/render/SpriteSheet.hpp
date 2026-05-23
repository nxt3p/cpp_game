#pragma once

#include "render/Texture.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace game {
enum class CharacterClass : std::uint8_t;
}

namespace render {

enum class SpriteFacing : std::uint8_t {
    Down,
    Up,
    Left,
    Right,
};

enum class SpriteClip : std::uint8_t {
    Idle,
    Walk,
    Attack,
    Hit,
    Death,
    Portrait,
};

struct SpriteFrameUV {
    float u0{0.0F};
    float v0{0.0F};
    float u1{1.0F};
    float v1{1.0F};
};

/// World billboards use OpenGL-style V; screen UI quads use top-down Y (swap V).
[[nodiscard]] inline SpriteFrameUV screenSpaceUV(const SpriteFrameUV& worldUv) noexcept {
    return {worldUv.u0, worldUv.v1, worldUv.u1, worldUv.v0};
}

/// Ensures u0<u1 and v0<v1 with a minimum span so billboards never collapse to a line.
[[nodiscard]] inline SpriteFrameUV sanitizeSpriteFrameUV(
    SpriteFrameUV uv,
    int columnCount = 1,
    int rowCount = 1) noexcept {
    if (uv.u0 > uv.u1) {
        std::swap(uv.u0, uv.u1);
    }
    if (uv.v0 > uv.v1) {
        std::swap(uv.v0, uv.v1);
    }

    const float minUSpan = columnCount > 0 ? (1.0F / static_cast<float>(columnCount)) * 0.5F : 0.01F;
    const float minVSpan = rowCount > 0 ? (1.0F / static_cast<float>(rowCount)) * 0.5F : 0.01F;
    if (uv.u1 - uv.u0 < minUSpan) {
        uv.u0 = 0.0F;
        uv.u1 = std::min(1.0F, minUSpan);
    }
    if (uv.v1 - uv.v0 < minVSpan) {
        uv.v0 = 0.0F;
        uv.v1 = std::min(1.0F, minVSpan);
    }
    return uv;
}

struct SpriteSheetClip {
    int row{0};
    int frameCount{1};
    float framesPerSecond{8.0F};
    bool loop{true};
    bool flipHorizontal{false};
};

struct SpriteFrameSample {
    const Texture* texture{nullptr};
    SpriteFrameUV uv{};
};

/// Sprite sheet with uniform frame cells (defaults 64x64 for class sheets).
class SpriteSheet {
public:
    [[nodiscard]] bool loadFromFile(const std::string& path, int frameWidth = 64, int frameHeight = 64);

    [[nodiscard]] bool isValid() const noexcept { return texture_.isValid(); }
    [[nodiscard]] const Texture& texture() const noexcept { return texture_; }

    [[nodiscard]] int frameWidth() const noexcept { return frameWidth_; }
    [[nodiscard]] int frameHeight() const noexcept { return frameHeight_; }
    [[nodiscard]] int columnCount() const noexcept { return columnCount_; }
    [[nodiscard]] int rowCount() const noexcept { return rowCount_; }

    [[nodiscard]] SpriteFrameUV frameUV(int row, int column, bool flipHorizontal = false) const noexcept;

    [[nodiscard]] SpriteSheetClip clip(SpriteClip animation, SpriteFacing facing) const noexcept;

    [[nodiscard]] SpriteFrameSample sample(
        SpriteClip animation,
        SpriteFacing facing,
        float elapsedSeconds) const noexcept;

    [[nodiscard]] SpriteFrameSample sampleMob(
        SpriteClip animation,
        SpriteFacing facing,
        float elapsedSeconds) const noexcept;

    /// Consecutive occupied frames from column 0 on a row (from alpha scan at load).
    [[nodiscard]] int occupiedFrameCount(int row) const noexcept;

private:
    void rebuildOccupancyFromFile(const std::string& path);

    Texture texture_{};
    int frameWidth_{64};
    int frameHeight_{64};
    int columnCount_{12};
    int rowCount_{22};
    std::vector<int> occupiedFramesPerRow_{};
};

[[nodiscard]] SpriteSheetClip defaultClassClip(SpriteClip animation, SpriteFacing facing) noexcept;

[[nodiscard]] SpriteSheetClip defaultMobClip(SpriteClip animation, SpriteFacing facing) noexcept;

/// Picks LPC or compact layout based on sheet dimensions, then clamps to valid rows/columns.
[[nodiscard]] SpriteSheetClip resolveSheetClip(
    SpriteClip animation,
    SpriteFacing facing,
    int rowCount,
    int columnCount) noexcept;

[[nodiscard]] SpriteSheetClip clampClipToSheet(
    SpriteSheetClip clip,
    int rowCount,
    int columnCount) noexcept;

[[nodiscard]] int frameIndexAtTime(const SpriteSheetClip& clip, float elapsedSeconds) noexcept;

} // namespace render
