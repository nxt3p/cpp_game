#pragma once

#include "Shader.hpp"
#include "render/ParticleSystem.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace render {

/// Draws a ParticleSystem as camera-facing soft-disc quads in two batched passes
/// (alpha-blended, then additive). Depth writes are disabled while drawing.
class ParticleRenderer {
public:
    ParticleRenderer(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
    ~ParticleRenderer();

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    void draw(
        const ParticleSystem& system,
        const glm::mat4& view,
        const glm::mat4& projection) const;

    [[nodiscard]] std::size_t lastDrawnParticleCount() const noexcept { return lastDrawnCount_; }

private:
    void drawBatch(
        const std::vector<float>& vertexData,
        const glm::mat4& view,
        const glm::mat4& projection,
        ParticleBlend blend) const;

    engine::Shader shader_;
    unsigned int vao_{0};
    unsigned int vbo_{0};
    mutable std::vector<float> alphaVertices_{};
    mutable std::vector<float> additiveVertices_{};
    mutable std::size_t lastDrawnCount_{0};
};

} // namespace render
