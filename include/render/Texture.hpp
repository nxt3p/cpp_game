#pragma once

#include <string>

namespace render {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] bool loadFromFile(const std::string& path, bool pixelArtFiltering = true);
    [[nodiscard]] bool isValid() const noexcept { return textureId_ != 0U; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] unsigned int id() const noexcept { return textureId_; }

    void bind(unsigned int textureUnit = 0) const;

private:
    unsigned int textureId_{0U};
    int width_{0};
    int height_{0};
};

} // namespace render
