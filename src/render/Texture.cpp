#include "render/Texture.hpp"

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace render {

Texture::~Texture() {
    if (textureId_ != 0U) {
        glDeleteTextures(1, &textureId_);
        textureId_ = 0U;
    }
}

Texture::Texture(Texture&& other) noexcept
    : textureId_(other.textureId_), width_(other.width_), height_(other.height_) {
    other.textureId_ = 0U;
    other.width_ = 0;
    other.height_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (textureId_ != 0U) {
            glDeleteTextures(1, &textureId_);
        }
        textureId_ = other.textureId_;
        width_ = other.width_;
        height_ = other.height_;
        other.textureId_ = 0U;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

bool Texture::loadFromFile(const std::string& path, const bool pixelArtFiltering) {
    if (textureId_ != 0U) {
        glDeleteTextures(1, &textureId_);
        textureId_ = 0U;
    }

    stbi_set_flip_vertically_on_load(1);

    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width_, &height_, &channels, 4);
    if (pixels == nullptr) {
        width_ = 0;
        height_ = 0;
        return false;
    }

    glGenTextures(1, &textureId_);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    stbi_image_free(pixels);

    const GLenum filter = pixelArtFiltering ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    ENGINE_GL_CHECK();
    return true;
}

void Texture::bind(const unsigned int textureUnit) const {
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, textureId_);
}

} // namespace render
