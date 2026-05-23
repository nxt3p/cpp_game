#include "game/GameSettings.hpp"

namespace game {

namespace {

constexpr int kResolutionPresets[][2] = {
    {1280, 720},
    {1600, 900},
    {1920, 1080},
};

constexpr int kResolutionPresetCount = 3;

} // namespace

const char* minimapAnchorLabel(const MinimapAnchor anchor) noexcept {
    switch (anchor) {
    case MinimapAnchor::TopRight:
        return "Top Right";
    case MinimapAnchor::TopLeft:
        return "Top Left";
    case MinimapAnchor::BottomRight:
        return "Bottom Right";
    case MinimapAnchor::BottomLeft:
        return "Bottom Left";
    }
    return "Top Right";
}

const char* graphicsQualityLabel(const int qualityIndex) noexcept {
    switch (qualityIndex) {
    case 0:
        return "Low";
    case 2:
        return "High";
    case 1:
    default:
        return "Medium";
    }
}

void cycleResolution(GameSettings& settings) noexcept {
    int presetIndex = 0;
    for (int index = 0; index < kResolutionPresetCount; ++index) {
        if (settings.resolutionWidth == kResolutionPresets[index][0] &&
            settings.resolutionHeight == kResolutionPresets[index][1]) {
            presetIndex = index;
            break;
        }
    }

    const int nextIndex = (presetIndex + 1) % kResolutionPresetCount;
    settings.resolutionWidth = kResolutionPresets[nextIndex][0];
    settings.resolutionHeight = kResolutionPresets[nextIndex][1];
}

void cycleMinimapAnchor(GameSettings& settings) noexcept {
    const int next = (static_cast<int>(settings.minimapAnchor) + 1) % 4;
    settings.minimapAnchor = static_cast<MinimapAnchor>(next);
}

void cycleGraphicsQuality(GameSettings& settings) noexcept {
    settings.graphicsQuality = (settings.graphicsQuality + 1) % 3;
}

} // namespace game
