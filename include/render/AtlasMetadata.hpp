#pragma once

#include <string>
#include <vector>

namespace render {

/// One animation row-block on an 8-direction sheet. Row = baseRow + facing index.
struct DirectionalClipInfo {
    std::string name;
    int baseRow{0};
    int frames{1};
    float fps{8.0F};
    bool loop{true};
};

/// JSON written by scripts/generate_assets.py for hero and monster sheets.
struct DirectionalAtlas {
    int frameWidth{64};
    int frameHeight{64};
    int columns{1};
    std::vector<DirectionalClipInfo> clips;

    [[nodiscard]] bool loadFromFile(const std::string& path);
    [[nodiscard]] bool loadFromString(const std::string& json);
    [[nodiscard]] const DirectionalClipInfo* find(const std::string& clipName) const noexcept;
};

struct NamedAtlasFrame {
    std::string name;
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

/// Named rectangles inside a UI or item atlas.
struct NamedAtlas {
    int imageWidth{0};
    int imageHeight{0};
    std::vector<NamedAtlasFrame> frames;

    [[nodiscard]] bool loadFromFile(const std::string& path);
    [[nodiscard]] bool loadFromString(const std::string& json);
    [[nodiscard]] const NamedAtlasFrame* find(const std::string& frameName) const noexcept;
};

} // namespace render
