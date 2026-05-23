#include "render/UiRenderer.hpp"

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstring>

namespace render {

namespace {

constexpr float kWhiteTint[4] = {1.0F, 1.0F, 1.0F, 1.0F};

} // namespace

UiRenderer::UiRenderer(const std::string& shaderVertexPath, const std::string& shaderFragmentPath)
    : shader_(shaderVertexPath, shaderFragmentPath) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    ENGINE_GL_CHECK();
}

void UiRenderer::resize(int width, int height) {
    screenWidth_ = width;
    screenHeight_ = height;
}

void UiRenderer::beginFrame() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    const glm::mat4 projection =
        glm::ortho(0.0F, static_cast<float>(screenWidth_), static_cast<float>(screenHeight_), 0.0F);
    shader_.setMat4("u_Projection", projection);
    shader_.setInt("u_Texture", 0);
    shader_.setInt("u_UseTexture", 0);
}

void UiRenderer::endFrame() {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UiRenderer::drawQuad(
    const float x,
    const float y,
    const float width,
    const float height,
    const float u0,
    const float v0,
    const float u1,
    const float v1,
    const bool textured,
    const Texture* texture,
    const float color[4]) const {
    // Screen Y grows downward; textures are uploaded with stbi vertical flip (image top at V=1).
    const std::array<float, 24> vertices = {
        x, y, u0, v1,
        x + width, y, u1, v1,
        x + width, y + height, u1, v0,
        x, y, u0, v1,
        x + width, y + height, u1, v0,
        x, y + height, u0, v0,
    };

    shader_.use();
    shader_.setVec4("u_Color", glm::vec4(color[0], color[1], color[2], color[3]));
    if (textured && texture != nullptr && texture->isValid()) {
        texture->bind(0);
        shader_.setInt("u_Texture", 0);
        shader_.setInt("u_UseTexture", 1);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
        shader_.setInt("u_UseTexture", 0);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ENGINE_GL_CHECK();
}

void UiRenderer::drawFilledRect(
    const float x,
    const float y,
    const float width,
    const float height,
    const float color[4]) const {
    drawQuad(x, y, width, height, 0.0F, 0.0F, 1.0F, 1.0F, false, nullptr, color);
}

void UiRenderer::drawOutlineRect(
    const float x,
    const float y,
    const float width,
    const float height,
    const float color[4],
    const float lineWidth) const {
    drawFilledRect(x, y, width, lineWidth, color);
    drawFilledRect(x, y + height - lineWidth, width, lineWidth, color);
    drawFilledRect(x, y, lineWidth, height, color);
    drawFilledRect(x + width - lineWidth, y, lineWidth, height, color);
}

void UiRenderer::drawTexturedRect(
    const Texture& texture,
    const float x,
    const float y,
    const float width,
    const float height,
    const float tint[4]) const {
    const float* color = tint != nullptr ? tint : kWhiteTint;
    drawQuad(x, y, width, height, 0.0F, 0.0F, 1.0F, 1.0F, true, &texture, color);
}

void UiRenderer::drawTexturedRectUV(
    const Texture& texture,
    const float x,
    const float y,
    const float width,
    const float height,
    const float u0,
    const float v0,
    const float u1,
    const float v1,
    const float tint[4]) const {
    const float* color = tint != nullptr ? tint : kWhiteTint;
    drawQuad(x, y, width, height, u0, v0, u1, v1, true, &texture, color);
}

void UiRenderer::drawNineSlice(
    const Texture& texture,
    const float x,
    const float y,
    const float width,
    const float height,
    const float borderPixels,
    const float tint[4]) const {
    if (!texture.isValid() || width <= 0.0F || height <= 0.0F) {
        return;
    }

    const float texW = static_cast<float>(texture.width());
    const float texH = static_cast<float>(texture.height());
    const float borderU = borderPixels / texW;
    const float borderV = borderPixels / texH;

    const float leftW = borderPixels;
    const float rightW = borderPixels;
    const float topH = borderPixels;
    const float bottomH = borderPixels;
    const float centerW = std::max(width - leftW - rightW, 0.0F);
    const float centerH = std::max(height - topH - bottomH, 0.0F);

    const float uCenter0 = borderU;
    const float uCenter1 = 1.0F - borderU;
    const float vCenter1 = 1.0F - borderV;

    drawTexturedRectUV(texture, x, y, leftW, topH, 0.0F, 0.0F, borderU, borderV, tint);
    drawTexturedRectUV(texture, x + leftW, y, centerW, topH, uCenter0, 0.0F, uCenter1, borderV, tint);
    drawTexturedRectUV(texture, x + leftW + centerW, y, rightW, topH, uCenter1, 0.0F, 1.0F, borderV, tint);

    drawTexturedRectUV(texture, x, y + topH, leftW, centerH, 0.0F, borderV, borderU, vCenter1, tint);
    drawTexturedRectUV(
        texture, x + leftW, y + topH, centerW, centerH, uCenter0, borderV, uCenter1, vCenter1, tint);
    drawTexturedRectUV(
        texture, x + leftW + centerW, y + topH, rightW, centerH, uCenter1, borderV, 1.0F, vCenter1, tint);

    drawTexturedRectUV(
        texture, x, y + topH + centerH, leftW, bottomH, 0.0F, vCenter1, borderU, 1.0F, tint);
    drawTexturedRectUV(
        texture, x + leftW, y + topH + centerH, centerW, bottomH, uCenter0, vCenter1, uCenter1, 1.0F, tint);
    drawTexturedRectUV(
        texture, x + leftW + centerW, y + topH + centerH, rightW, bottomH, uCenter1, vCenter1, 1.0F, 1.0F, tint);
}

} // namespace render
