#pragma once

#include "Shader.hpp"
#include "render/Texture.hpp"

#include <string>
#include <vector>

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

    /// Filled disc. Bands are batched into one draw with the rest of the solid UI.
    void drawFilledCircle(float centerX, float centerY, float radius, const float color[4], int bands = 28) const;

    /// Liquid fill of a disc from the bottom up to `fillRatio` (0..1); used for HP/mana globes.
    /// `waveAmplitude` ripples the surface in pixels when non-zero.
    void drawCircleFill(
        float centerX,
        float centerY,
        float radius,
        float fillRatio,
        const float color[4],
        int bands = 28,
        float wavePhase = 0.0F,
        float waveAmplitude = 0.0F) const;

    /// Dark clockwise wedge used as a cooldown overlay. `ratio` 0 is empty, 1 covers the disc.
    void drawRadialCooldown(
        float centerX,
        float centerY,
        float radius,
        float ratio,
        const float color[4]) const;

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

    struct UiVertex {
        float x;
        float y;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    void appendQuad(
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
    void appendVertex(float x, float y, float u, float v, const float color[4]) const;
    void flushBatch() const;

    engine::Shader shader_;
    unsigned int vao_{0};
    unsigned int vbo_{0};
    int screenWidth_{0};
    int screenHeight_{0};
    mutable std::vector<UiVertex> batch_;
    mutable const Texture* batchTexture_{nullptr};
    mutable bool batchTextured_{false};
    mutable bool batchOpen_{false};
    mutable bool batching_{false};
};

} // namespace render
