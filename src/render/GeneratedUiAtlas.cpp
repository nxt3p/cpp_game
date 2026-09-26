#include "render/GeneratedUiAtlas.hpp"

namespace render {

namespace {

[[nodiscard]] std::string join(const std::string& directory, const char* file) {
    if (directory.empty()) {
        return file;
    }
    if (directory.back() == '/') {
        return directory + file;
    }
    return directory + "/" + file;
}

} // namespace

bool GeneratedUiAtlas::load(const std::string& directory) {
    return load(directory, "ui_atlas.json", "ui_atlas.png");
}

bool GeneratedUiAtlas::load(const std::string& directory, const char* jsonFile, const char* imageFile) {
    atlas_ = {};
    if (jsonFile == nullptr || imageFile == nullptr) {
        return false;
    }
    if (!atlas_.loadFromFile(join(directory, jsonFile))) {
        return false;
    }
    return texture_.loadFromFile(join(directory, imageFile), true);
}

UiFrameUv GeneratedUiAtlas::uvFor(const std::string& frameName) const noexcept {
    UiFrameUv uv{};
    const NamedAtlasFrame* frame = atlas_.find(frameName);
    if (frame == nullptr || !texture_.isValid() || frame->width <= 0 || frame->height <= 0) {
        return uv;
    }

    const float width = static_cast<float>(texture_.width());
    const float height = static_cast<float>(texture_.height());
    if (width <= 0.0F || height <= 0.0F) {
        return uv;
    }

    uv.u0 = static_cast<float>(frame->x) / width;
    uv.u1 = static_cast<float>(frame->x + frame->width) / width;
    // Image top is V = 1 after the vertical flip in Texture::loadFromFile.
    // UiRenderer treats v1 as the top of the screen quad and v0 as the bottom.
    uv.v1 = 1.0F - static_cast<float>(frame->y) / height;
    uv.v0 = 1.0F - static_cast<float>(frame->y + frame->height) / height;
    uv.valid = true;
    return uv;
}

} // namespace render
