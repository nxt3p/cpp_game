#pragma once

#include "Shader.hpp"
#include "render/Texture.hpp"

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include <string>

namespace render {

class SpriteRenderer {
public:
    explicit SpriteRenderer(
        const std::string& shaderVertexPath,
        const std::string& shaderFragmentPath);
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    void beginBillboardPass(
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& playerLightPosition,
        float lightRadius,
        float ambientDark,
        float ambientBright) const;

    void submitBillboard(const Texture& texture, const std::array<float, 30>& vertices) const;

    void flushBillboardBatch() const;

    void endBillboardPass() const;

    void drawBillboard(
        const Texture& texture,
        const glm::vec3& worldPosition,
        float worldHeight,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& playerLightPosition,
        float lightRadius,
        float ambientDark,
        float ambientBright,
        const glm::vec4& tint = glm::vec4(1.0F)) const;

    void drawBillboardUV(
        const Texture& texture,
        const glm::vec3& worldPosition,
        float worldHeight,
        float u0,
        float v0,
        float u1,
        float v1,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& playerLightPosition,
        float lightRadius,
        float ambientDark,
        float ambientBright,
        const glm::vec4& tint = glm::vec4(1.0F)) const;

    /// Silhouette edge outline that follows sprite alpha (draw after the base sprite).
    void drawBillboardOutline(
        const Texture& texture,
        const glm::vec3& worldPosition,
        float worldHeight,
        float u0,
        float v0,
        float u1,
        float v1,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec4& outlineColor,
        float outlineTexels = 2.0F) const;

private:
    engine::Shader shader_;
    engine::Shader outlineShader_;
    unsigned int vao_{0};
    unsigned int vbo_{0};

    mutable bool batchPassActive_{false};
    mutable glm::mat4 batchView_{1.0F};
    mutable glm::mat4 batchProjection_{1.0F};
    mutable glm::vec3 batchPlayerLight_{0.0F};
    mutable float batchLightRadius_{0.0F};
    mutable float batchAmbientDark_{0.0F};
    mutable float batchAmbientBright_{0.0F};
    mutable const Texture* batchTexture_{nullptr};
    mutable std::vector<float> batchVertexData_;
};

} // namespace render
