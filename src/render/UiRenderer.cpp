#include "render/UiRenderer.hpp"

#include "EngineAssert.hpp"
#include "ui/GlobeFill.hpp"

#include "engine/FrameProbe.hpp"
#include "engine/GlBindings.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace render {

namespace {

constexpr float kWhiteTint[4] = {1.0F, 1.0F, 1.0F, 1.0F};

} // namespace

UiRenderer::UiRenderer(const std::string& shaderVertexPath, const std::string& shaderFragmentPath)
    : shader_(shaderVertexPath, shaderFragmentPath) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    constexpr GLsizei stride = static_cast<GLsizei>(sizeof(UiVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(4 * sizeof(float)));
    glBindVertexArray(0);
    ENGINE_GL_CHECK();

    batch_.reserve(2048U);
}

void UiRenderer::resize(int width, int height) {
    screenWidth_ = width;
    screenHeight_ = height;
}

void UiRenderer::beginFrame() {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    batching_ = true;
}

void UiRenderer::endFrame() {
    flushBatch();
    batching_ = false;
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UiRenderer::appendVertex(const float x, const float y, const float u, const float v, const float color[4]) const {
    batch_.push_back(UiVertex{x, y, u, v, color[0], color[1], color[2], color[3]});
}

void UiRenderer::flushBatch() const {
    if (!batchOpen_ || batch_.empty()) {
        batch_.clear();
        batchOpen_ = false;
        return;
    }

    shader_.use();
    const glm::mat4 projection = glm::ortho(
        0.0F, static_cast<float>(std::max(screenWidth_, 1)), static_cast<float>(std::max(screenHeight_, 1)), 0.0F);
    shader_.setMat4("u_Projection", projection);
    shader_.setInt("u_Texture", 0);
    shader_.setInt("u_UseTexture", batchTextured_ ? 1 : 0);
    if (batchTextured_ && batchTexture_ != nullptr && batchTexture_->isValid()) {
        batchTexture_->bind(0);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(batch_.size() * sizeof(UiVertex)),
        batch_.data(),
        GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch_.size()));
    glBindVertexArray(0);
    engine::FrameProbe::instance().addDraw();

    batch_.clear();
    batchOpen_ = false;
}

void UiRenderer::appendQuad(
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
    const Texture* batchTexture = textured ? texture : nullptr;
    if (batchOpen_ && (batchTextured_ != textured || batchTexture_ != batchTexture)) {
        flushBatch();
    }

    batchOpen_ = true;
    batchTextured_ = textured;
    batchTexture_ = batchTexture;

    // Screen Y grows downward; textures are uploaded with stbi vertical flip (image top at V=1).
    appendVertex(x, y, u0, v1, color);
    appendVertex(x + width, y, u1, v1, color);
    appendVertex(x + width, y + height, u1, v0, color);
    appendVertex(x, y, u0, v1, color);
    appendVertex(x + width, y + height, u1, v0, color);
    appendVertex(x, y + height, u0, v0, color);
    engine::FrameProbe::instance().addUiQuad();

    if (!batching_) {
        flushBatch();
    }
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
    appendQuad(x, y, width, height, u0, v0, u1, v1, textured, texture, color);
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

void UiRenderer::drawSolidTriangle(
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const float color[4]) const {
    if (color == nullptr) {
        return;
    }
    if (batchOpen_ && batchTextured_) {
        flushBatch();
    }
    batchOpen_ = true;
    batchTextured_ = false;
    batchTexture_ = nullptr;
    appendVertex(x0, y0, 0.0F, 0.0F, color);
    appendVertex(x1, y1, 0.0F, 0.0F, color);
    appendVertex(x2, y2, 0.0F, 0.0F, color);
    engine::FrameProbe::instance().addUiQuad();
    if (!batching_) {
        flushBatch();
    }
}

void UiRenderer::drawFilledCircle(
    const float centerX,
    const float centerY,
    const float radius,
    const float color[4],
    const int bands) const {
    if (radius <= 0.0F || bands < 3) {
        return;
    }

    if (batchOpen_ && batchTextured_) {
        flushBatch();
    }
    batchOpen_ = true;
    batchTextured_ = false;
    batchTexture_ = nullptr;

    const float step = 6.2831853F / static_cast<float>(bands);
    for (int index = 0; index < bands; ++index) {
        const float angle0 = step * static_cast<float>(index);
        const float angle1 = step * static_cast<float>(index + 1);
        appendVertex(centerX, centerY, 0.0F, 0.0F, color);
        appendVertex(centerX + std::cos(angle0) * radius, centerY + std::sin(angle0) * radius, 0.0F, 0.0F, color);
        appendVertex(centerX + std::cos(angle1) * radius, centerY + std::sin(angle1) * radius, 0.0F, 0.0F, color);
    }
    engine::FrameProbe::instance().addUiQuad();
    if (!batching_) {
        flushBatch();
    }
}

void UiRenderer::drawCircleFill(
    const float centerX,
    const float centerY,
    const float radius,
    const float fillRatio,
    const float color[4],
    const int bands,
    const float wavePhase,
    const float waveAmplitude) const {
    if (radius <= 0.0F || bands <= 0 || fillRatio <= 0.0F) {
        return;
    }

    const float bandHeight = (radius * 2.0F) / static_cast<float>(bands);
    const float surfaceY = ui::liquidSurfaceY(centerY, radius, fillRatio, wavePhase, waveAmplitude);
    const float discBottom = centerY + radius;

    // Each band is clipped to the circle. The wave only moves the surface, so liquid cannot leave the disc.
    for (int band = 0; band < bands; ++band) {
        const float bottomY = discBottom - static_cast<float>(band) * bandHeight;
        const float topY = bottomY - bandHeight;
        const float drawTop = std::max(topY, surfaceY);
        const float drawBottom = std::min(bottomY, discBottom);
        const float drawHeight = drawBottom - drawTop;
        if (drawHeight <= 0.5F) {
            continue;
        }
        const float midY = (drawTop + drawBottom) * 0.5F;
        const float halfWidth = ui::discHalfWidth(radius, midY - centerY);
        if (halfWidth <= 0.5F) {
            continue;
        }
        drawFilledRect(centerX - halfWidth, drawTop, halfWidth * 2.0F, drawHeight, color);
    }
}

void UiRenderer::drawRadialCooldown(
    const float centerX,
    const float centerY,
    const float radius,
    const float ratio,
    const float color[4]) const {
    if (radius <= 0.0F || ratio <= 0.001F) {
        return;
    }

    constexpr int kSegments = 28;
    const float clamped = std::min(ratio, 1.0F);
    const float sweep = clamped * 6.2831853F;
    const int steps = std::max(1, static_cast<int>(std::ceil(clamped * static_cast<float>(kSegments))));
    const float step = sweep / static_cast<float>(steps);
    const float start = -1.5707963F;

    if (batchOpen_ && batchTextured_) {
        flushBatch();
    }
    batchOpen_ = true;
    batchTextured_ = false;
    batchTexture_ = nullptr;

    for (int index = 0; index < steps; ++index) {
        const float angle0 = start + step * static_cast<float>(index);
        const float angle1 = start + step * static_cast<float>(index + 1);
        appendVertex(centerX, centerY, 0.0F, 0.0F, color);
        appendVertex(centerX + std::sin(angle0) * radius, centerY - std::cos(angle0) * radius, 0.0F, 0.0F, color);
        appendVertex(centerX + std::sin(angle1) * radius, centerY - std::cos(angle1) * radius, 0.0F, 0.0F, color);
    }
    engine::FrameProbe::instance().addUiQuad();
    if (!batching_) {
        flushBatch();
    }
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
