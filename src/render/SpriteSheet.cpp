#include "render/SpriteSheet.hpp"

#include "stb_image.h"

#include <algorithm>
#include <cmath>

namespace render {

namespace {

constexpr int kDefaultFrameSize = 64;
constexpr int kMinCellAlphaSum = 1800;

[[nodiscard]] bool cellHasContent(
    const unsigned char* pixels,
    const int textureWidth,
    const int textureHeight,
    const int frameWidth,
    const int frameHeight,
    const int row,
    const int column) {
    const int startX = column * frameWidth;
    const int startY = row * frameHeight;
    if (startX + frameWidth > textureWidth || startY + frameHeight > textureHeight) {
        return false;
    }

    int alphaSum = 0;
    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            const int pixelIndex = ((startY + y) * textureWidth + (startX + x)) * 4;
            alphaSum += pixels[pixelIndex + 3];
        }
    }
    return alphaSum >= kMinCellAlphaSum;
}

} // namespace

bool SpriteSheet::loadFromFile(const std::string& path, const int frameWidth, const int frameHeight) {
    if (!texture_.loadFromFile(path, true)) {
        return false;
    }

    frameWidth_ = frameWidth > 0 ? frameWidth : kDefaultFrameSize;
    frameHeight_ = frameHeight > 0 ? frameHeight : kDefaultFrameSize;
    columnCount_ = std::max(texture_.width() / frameWidth_, 1);
    rowCount_ = std::max(texture_.height() / frameHeight_, 1);
    rebuildOccupancyFromFile(path);
    return true;
}

void SpriteSheet::rebuildOccupancyFromFile(const std::string& path) {
    occupiedFramesPerRow_.assign(static_cast<std::size_t>(rowCount_), 0);

    int textureWidth = 0;
    int textureHeight = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* pixels = stbi_load(path.c_str(), &textureWidth, &textureHeight, &channels, 4);
    stbi_set_flip_vertically_on_load(0);
    if (pixels == nullptr) {
        for (int row = 0; row < rowCount_; ++row) {
            occupiedFramesPerRow_[static_cast<std::size_t>(row)] = columnCount_;
        }
        return;
    }

    for (int row = 0; row < rowCount_; ++row) {
        int consecutive = 0;
        for (int column = 0; column < columnCount_; ++column) {
            if (!cellHasContent(
                    pixels, textureWidth, textureHeight, frameWidth_, frameHeight_, row, column)) {
                break;
            }
            consecutive = column + 1;
        }
        occupiedFramesPerRow_[static_cast<std::size_t>(row)] = consecutive;
    }

    stbi_image_free(pixels);
}

int SpriteSheet::occupiedFrameCount(const int row) const noexcept {
    if (row < 0 || row >= rowCount_) {
        return 0;
    }
    const int scanned =
        occupiedFramesPerRow_.empty() ? 0 : occupiedFramesPerRow_[static_cast<std::size_t>(row)];
    return scanned > 0 ? scanned : columnCount_;
}

SpriteFrameUV SpriteSheet::frameUV(const int row, const int column, const bool flipHorizontal) const noexcept {
    const float columns = static_cast<float>(columnCount_);
    const float rows = static_cast<float>(rowCount_);

    float u0 = static_cast<float>(column) / columns;
    float u1 = static_cast<float>(column + 1) / columns;
    if (flipHorizontal) {
        std::swap(u0, u1);
    }

    const float v1 = 1.0F - static_cast<float>(row) / rows;
    const float v0 = 1.0F - static_cast<float>(row + 1) / rows;
    return {u0, v0, u1, v1};
}

SpriteSheetClip clampClipToSheet(
    SpriteSheetClip clip,
    const int rowCount,
    const int columnCount,
    const int occupiedFramesInRow) noexcept {
    const int safeRows = std::max(rowCount, 1);
    const int safeColumns = std::max(columnCount, 1);
    const int safeOccupied = std::max(occupiedFramesInRow, 1);

    clip.row = std::clamp(clip.row, 0, safeRows - 1);
    clip.frameCount = std::clamp(clip.frameCount, 1, safeColumns);
    clip.frameCount = std::clamp(clip.frameCount, 1, safeOccupied);
    return clip;
}

SpriteSheetClip resolveSheetClip(
    const SpriteClip animation,
    const SpriteFacing facing,
    const int rowCount,
    const int columnCount) noexcept {
    SpriteSheetClip clip =
        rowCount > 4 ? defaultClassClip(animation, facing) : defaultMobClip(animation, facing);
    if (clip.row >= rowCount) {
        clip = defaultMobClip(animation, facing);
    }
    return clampClipToSheet(clip, rowCount, columnCount, columnCount);
}

SpriteSheetClip SpriteSheet::clip(const SpriteClip animation, const SpriteFacing facing) const noexcept {
    SpriteSheetClip resolved =
        rowCount_ > 4 ? defaultClassClip(animation, facing) : defaultMobClip(animation, facing);
    if (resolved.row >= rowCount_) {
        resolved = defaultMobClip(animation, facing);
    }
    return clampClipToSheet(resolved, rowCount_, columnCount_, occupiedFrameCount(resolved.row));
}

SpriteFrameSample SpriteSheet::sample(
    const SpriteClip animation,
    const SpriteFacing facing,
    const float elapsedSeconds) const noexcept {
    SpriteFrameSample sample{};
    if (!texture_.isValid()) {
        return sample;
    }

    const SpriteSheetClip sheetClip = clip(animation, facing);
    int frameColumn = frameIndexAtTime(sheetClip, elapsedSeconds);
    const int occupied = occupiedFrameCount(sheetClip.row);
    if (occupied > 0) {
        frameColumn = std::clamp(frameColumn, 0, occupied - 1);
    }

    sample.texture = &texture_;
    sample.uv = sanitizeSpriteFrameUV(
        frameUV(sheetClip.row, frameColumn, sheetClip.flipHorizontal), columnCount_, rowCount_);
    return sample;
}

SpriteFrameSample SpriteSheet::sampleMob(
    const SpriteClip animation,
    const SpriteFacing facing,
    const float elapsedSeconds) const noexcept {
    return sample(animation, facing, elapsedSeconds);
}

SpriteSheetClip defaultClassClip(const SpriteClip animation, const SpriteFacing facing) noexcept {
    SpriteSheetClip clip{};

    switch (animation) {
    case SpriteClip::Idle:
        clip.framesPerSecond = 6.0F;
        clip.frameCount = 4;
        switch (facing) {
        case SpriteFacing::Down:
            clip.row = 0;
            break;
        case SpriteFacing::Up:
            clip.row = 1;
            break;
        case SpriteFacing::Left:
            clip.row = 2;
            clip.flipHorizontal = true;
            break;
        case SpriteFacing::Right:
            clip.row = 2;
            break;
        }
        break;
    case SpriteClip::Walk:
        clip.framesPerSecond = 10.0F;
        clip.frameCount = 6;
        switch (facing) {
        case SpriteFacing::Down:
            clip.row = 11;
            break;
        case SpriteFacing::Up:
            clip.row = 12;
            break;
        case SpriteFacing::Left:
            clip.row = 13;
            clip.flipHorizontal = true;
            break;
        case SpriteFacing::Right:
            clip.row = 14;
            break;
        }
        break;
    case SpriteClip::Attack:
        clip.framesPerSecond = 12.0F;
        clip.loop = false;
        switch (facing) {
        case SpriteFacing::Down:
            clip.row = 8;
            clip.frameCount = 11;
            break;
        case SpriteFacing::Up:
            clip.row = 9;
            clip.frameCount = 12;
            break;
        case SpriteFacing::Left:
            clip.row = 10;
            clip.frameCount = 8;
            clip.flipHorizontal = true;
            break;
        case SpriteFacing::Right:
            clip.row = 10;
            clip.frameCount = 8;
            break;
        }
        break;
    case SpriteClip::Hit:
        clip.framesPerSecond = 12.0F;
        clip.frameCount = 4;
        clip.loop = false;
        clip.row = 5;
        break;
    case SpriteClip::Death:
        clip.framesPerSecond = 10.0F;
        clip.frameCount = 10;
        clip.loop = false;
        clip.row = 7;
        break;
    case SpriteClip::Portrait:
        clip.framesPerSecond = 1.0F;
        clip.frameCount = 1;
        clip.loop = true;
        clip.row = 0;
        break;
    }

    return clip;
}

SpriteSheetClip defaultMobClip(const SpriteClip animation, const SpriteFacing facing) noexcept {
    SpriteSheetClip clip{};
    clip.framesPerSecond = 8.0F;
    clip.frameCount = 4;

    switch (facing) {
    case SpriteFacing::Down:
        clip.row = 0;
        break;
    case SpriteFacing::Up:
        clip.row = 1;
        break;
    case SpriteFacing::Left:
        clip.row = 2;
        break;
    case SpriteFacing::Right:
        clip.row = 3;
        break;
    }

    switch (animation) {
    case SpriteClip::Walk:
        clip.framesPerSecond = 10.0F;
        clip.frameCount = 4;
        break;
    case SpriteClip::Attack:
        clip.framesPerSecond = 12.0F;
        clip.frameCount = 4;
        clip.loop = false;
        break;
    case SpriteClip::Hit:
    case SpriteClip::Death:
        clip.framesPerSecond = 10.0F;
        clip.frameCount = 2;
        clip.loop = false;
        break;
    case SpriteClip::Portrait:
        clip.frameCount = 1;
        clip.framesPerSecond = 1.0F;
        break;
    case SpriteClip::Idle:
    default:
        break;
    }

    if (facing == SpriteFacing::Left) {
        clip.flipHorizontal = true;
    }

    return clip;
}

int frameIndexAtTime(const SpriteSheetClip& clip, const float elapsedSeconds) noexcept {
    if (clip.frameCount <= 1) {
        return 0;
    }

    const float frameDuration = 1.0F / std::max(clip.framesPerSecond, 0.1F);
    int frame = static_cast<int>(elapsedSeconds / frameDuration);
    if (clip.loop) {
        frame %= clip.frameCount;
    } else {
        frame = std::min(frame, clip.frameCount - 1);
    }
    return std::clamp(frame, 0, clip.frameCount - 1);
}

} // namespace render
