#pragma once

#include "render/AtlasMetadata.hpp"
#include "render/Texture.hpp"

#include <string>

namespace render {

struct UiFrameUv {
    float u0{0.0F};
    float v0{0.0F};
    float u1{1.0F};
    float v1{1.0F};
    bool valid{false};
};

/// UI atlas produced by scripts/generate_assets.py. UV v0 is the bottom of the frame for UiRenderer.
class GeneratedUiAtlas {
public:
    [[nodiscard]] bool load(const std::string& directory);
    [[nodiscard]] bool load(
        const std::string& directory,
        const char* jsonFile,
        const char* imageFile);
    [[nodiscard]] bool isLoaded() const noexcept { return texture_.isValid() && !atlas_.frames.empty(); }
    [[nodiscard]] const Texture& texture() const noexcept { return texture_; }
    [[nodiscard]] UiFrameUv uvFor(const std::string& frameName) const noexcept;

private:
    Texture texture_{};
    NamedAtlas atlas_{};
};

} // namespace render
