#include "render/SpriteRenderer.hpp"

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"

#include <array>
#include <cmath>
#include <string>

namespace render {

namespace {

[[nodiscard]] std::string outlineFragmentPath(const std::string& spriteFragmentPath) {
    const std::string marker = "sprite.frag";
    const std::size_t position = spriteFragmentPath.rfind(marker);
    if (position == std::string::npos) {
        return spriteFragmentPath;
    }
    return spriteFragmentPath.substr(0, position) + "sprite_outline.frag";
}

} // namespace

SpriteRenderer::SpriteRenderer(
    const std::string& shaderVertexPath,
    const std::string& shaderFragmentPath)
    : shader_(shaderVertexPath, shaderFragmentPath),
      outlineShader_(shaderVertexPath, outlineFragmentPath(shaderFragmentPath)) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    ENGINE_GL_CHECK();
}

SpriteRenderer::~SpriteRenderer() {
    if (vbo_ != 0U) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0U) {
        glDeleteVertexArrays(1, &vao_);
    }
}

void SpriteRenderer::flushBillboardBatch() const {
    if (batchVertexData_.empty() || batchTexture_ == nullptr) {
        batchVertexData_.clear();
        batchTexture_ = nullptr;
        return;
    }

    shader_.use();
    shader_.setMat4("u_View", batchView_);
    shader_.setMat4("u_Projection", batchProjection_);
    shader_.setVec3("u_PlayerPos", batchPlayerLight_);
    shader_.setFloat("u_LightRadius", batchLightRadius_);
    shader_.setFloat("u_AmbientDark", batchAmbientDark_);
    shader_.setFloat("u_AmbientBright", batchAmbientBright_);
    shader_.setVec4("u_Tint", glm::vec4(1.0F));
    shader_.setInt("u_Texture", 0);
    batchTexture_->bind(0);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(batchVertexData_.size() * sizeof(float)),
        batchVertexData_.data(),
        GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batchVertexData_.size() / 5U));
    glBindVertexArray(0);
    ENGINE_GL_CHECK();

    batchVertexData_.clear();
    batchTexture_ = nullptr;
}

void SpriteRenderer::beginBillboardPass(
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& playerLightPosition,
    const float lightRadius,
    const float ambientDark,
    const float ambientBright) const {
    batchPassActive_ = true;
    batchView_ = view;
    batchProjection_ = projection;
    batchPlayerLight_ = playerLightPosition;
    batchLightRadius_ = lightRadius;
    batchAmbientDark_ = ambientDark;
    batchAmbientBright_ = ambientBright;
    batchTexture_ = nullptr;
    batchVertexData_.clear();
    batchVertexData_.reserve(30U * 96U);
}

void SpriteRenderer::submitBillboard(const Texture& texture, const std::array<float, 30>& vertices) const {
    if (!batchPassActive_ || !texture.isValid()) {
        return;
    }

    if (batchTexture_ != nullptr && batchTexture_ != &texture) {
        flushBillboardBatch();
    }

    batchTexture_ = &texture;
    batchVertexData_.insert(batchVertexData_.end(), vertices.begin(), vertices.end());
}

void SpriteRenderer::endBillboardPass() const {
    flushBillboardBatch();
    batchPassActive_ = false;
}

namespace {

void drawBillboardVertices(
    const engine::Shader& shader,
    unsigned int vao,
    unsigned int vbo,
    const std::array<float, 30>& vertices,
    const Texture& texture,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& playerLightPosition,
    const float lightRadius,
    const float ambientDark,
    const float ambientBright,
    const glm::vec4& tint) {
    shader.use();
    shader.setMat4("u_View", view);
    shader.setMat4("u_Projection", projection);
    shader.setVec3("u_PlayerPos", playerLightPosition);
    shader.setFloat("u_LightRadius", lightRadius);
    shader.setFloat("u_AmbientDark", ambientDark);
    shader.setFloat("u_AmbientBright", ambientBright);
    shader.setVec4("u_Tint", tint);
    shader.setInt("u_Texture", 0);

    texture.bind(0);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ENGINE_GL_CHECK();
}

[[nodiscard]] std::array<float, 30> buildBillboardVertices(
    const glm::vec3& worldPosition,
    const float worldHeight,
    const float worldWidth,
    const float u0,
    const float v0,
    const float u1,
    const float v1,
    const glm::mat4& view) {
    const float halfHeight = worldHeight * 0.5F;
    const float halfWidth = worldWidth * 0.5F;

    const glm::vec3 cameraRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
    const glm::vec3 cameraUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
    const glm::vec3 anchor = worldPosition + glm::vec3(0.0F, halfHeight, 0.0F);

    const glm::vec3 topLeft = anchor - cameraRight * halfWidth + cameraUp * halfHeight;
    const glm::vec3 topRight = anchor + cameraRight * halfWidth + cameraUp * halfHeight;
    const glm::vec3 bottomLeft = anchor - cameraRight * halfWidth - cameraUp * halfHeight;
    const glm::vec3 bottomRight = anchor + cameraRight * halfWidth - cameraUp * halfHeight;

    return {
        bottomLeft.x, bottomLeft.y, bottomLeft.z, u0, v0,
        bottomRight.x, bottomRight.y, bottomRight.z, u1, v0,
        topRight.x, topRight.y, topRight.z, u1, v1,
        topRight.x, topRight.y, topRight.z, u1, v1,
        topLeft.x, topLeft.y, topLeft.z, u0, v1,
        bottomLeft.x, bottomLeft.y, bottomLeft.z, u0, v0,
    };
}

[[nodiscard]] float computeBillboardWorldWidth(
    const Texture& texture,
    const float worldHeight,
    const float u0,
    const float v0,
    const float u1,
    const float v1) noexcept {
    const float uMin = std::min(u0, u1);
    const float uMax = std::max(u0, u1);
    const float vMin = std::min(v0, v1);
    const float vMax = std::max(v0, v1);
    const float uvWidth = std::max(uMax - uMin, 1.0F / static_cast<float>(std::max(texture.width(), 1)));
    const float uvHeight = std::max(vMax - vMin, 1.0F / static_cast<float>(std::max(texture.height(), 1)));
    const float textureAspect =
        texture.height() > 0
            ? (static_cast<float>(texture.width()) / static_cast<float>(texture.height())) * (uvWidth / uvHeight)
            : 1.0F;
    return std::max(worldHeight * textureAspect, worldHeight * 0.35F);
}

} // namespace

void SpriteRenderer::drawBillboard(
    const Texture& texture,
    const glm::vec3& worldPosition,
    const float worldHeight,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& playerLightPosition,
    const float lightRadius,
    const float ambientDark,
    const float ambientBright,
    const glm::vec4& tint) const {
    drawBillboardUV(
        texture,
        worldPosition,
        worldHeight,
        0.0F,
        0.0F,
        1.0F,
        1.0F,
        view,
        projection,
        playerLightPosition,
        lightRadius,
        ambientDark,
        ambientBright,
        tint);
}

void SpriteRenderer::drawBillboardOutline(
    const Texture& texture,
    const glm::vec3& worldPosition,
    const float worldHeight,
    const float u0,
    const float v0,
    const float u1,
    const float v1,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec4& outlineColor,
    const float outlineTexels) const {
    if (!texture.isValid() || worldHeight <= 0.0F) {
        return;
    }

    const float worldWidth = computeBillboardWorldWidth(texture, worldHeight, u0, v0, u1, v1);

    const std::array<float, 30> vertices =
        buildBillboardVertices(worldPosition, worldHeight, worldWidth, u0, v0, u1, v1, view);

    const float uMin = std::min(u0, u1);
    const float uMax = std::max(u0, u1);
    const float vMin = std::min(v0, v1);
    const float vMax = std::max(v0, v1);
    const float uvWidth = std::max(uMax - uMin, 1.0F / static_cast<float>(std::max(texture.width(), 1)));
    const float uvHeight = std::max(vMax - vMin, 1.0F / static_cast<float>(std::max(texture.height(), 1)));
    const float safeTexels = std::max(outlineTexels, 1.0F);
    const glm::vec2 texelStep{
        safeTexels * uvWidth / static_cast<float>(std::max(texture.width(), 1)),
        safeTexels * uvHeight / static_cast<float>(std::max(texture.height(), 1))};

    outlineShader_.use();
    outlineShader_.setMat4("u_View", view);
    outlineShader_.setMat4("u_Projection", projection);
    outlineShader_.setVec4("u_OutlineColor", outlineColor);
    outlineShader_.setVec2("u_TexelStep", texelStep);
    outlineShader_.setInt("u_Texture", 0);
    texture.bind(0);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ENGINE_GL_CHECK();
}

void SpriteRenderer::drawBillboardUV(
    const Texture& texture,
    const glm::vec3& worldPosition,
    const float worldHeight,
    const float u0,
    const float v0,
    const float u1,
    const float v1,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& playerLightPosition,
    const float lightRadius,
    const float ambientDark,
    const float ambientBright,
    const glm::vec4& tint) const {
    if (!texture.isValid() || worldHeight <= 0.0F) {
        return;
    }

    const float worldWidth = computeBillboardWorldWidth(texture, worldHeight, u0, v0, u1, v1);

    const std::array<float, 30> vertices =
        buildBillboardVertices(worldPosition, worldHeight, worldWidth, u0, v0, u1, v1, view);

    if (batchPassActive_) {
        submitBillboard(texture, vertices);
        return;
    }

    drawBillboardVertices(
        shader_,
        vao_,
        vbo_,
        vertices,
        texture,
        view,
        projection,
        playerLightPosition,
        lightRadius,
        ambientDark,
        ambientBright,
        tint);
}

} // namespace render
