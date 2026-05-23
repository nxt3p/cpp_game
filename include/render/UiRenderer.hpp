#pragma once

#include "Shader.hpp"
#include "render/Texture.hpp"

#include <string>

namespace render {

class UiRenderer {
public:
    explicit UiRenderer(const std::string& shaderVertexPath, const std::string& shaderFragmentPath);

    void resize(int width, int height);
    void beginFrame();
    void endFrame();

    void drawFilledRect(float x, float y, float width, float height, const float color[4]) const;
    void drawOutlineRect(
        float x,
        float y,
        float width,
        float height,
        const float color[4],
        float lineWidth = 2.0F) const;

    void drawTexturedRect(
        const Texture& texture,
        float x,
        float y,
        float width,
        float height,
        const float tint[4] = nullptr) const;

    void drawTexturedRectUV(
        const Texture& texture,
        float x,
        float y,
        float width,
        float height,
        float u0,
        float v0,
        float u1,
        float v1,
        const float tint[4] = nullptr) const;

    /// Stretches a 9-slice panel texture (border pixels stay fixed, center tiles).
    void drawNineSlice(
        const Texture& texture,
        float x,
        float y,
        float width,
        float height,
        float borderPixels,
        const float tint[4] = nullptr) const;

private:
    void drawQuad(
        float x,
        float y,
        float width,
        float height,
        float u0,
        float v0,
        float u1,
        float v1,
        bool textured,
        const Texture* texture,
        const float color[4]) const;

    engine::Shader shader_;
    unsigned int vao_{0};
    unsigned int vbo_{0};
    int screenWidth_{0};
    int screenHeight_{0};
};

} // namespace render
