#include "render/ParticleRenderer.hpp"

#include "EngineAssert.hpp"
#include "engine/FrameProbe.hpp"

#include "engine/GlBindings.hpp"

#include <glm/geometric.hpp>

#include <algorithm>

namespace render {

namespace {

constexpr std::size_t kFloatsPerVertex = 9; // pos(3) uv(2) color(4)
constexpr std::size_t kVerticesPerParticle = 6;

void appendParticleQuad(
    std::vector<float>& out,
    const Particle& particle,
    const glm::vec3& cameraRight,
    const glm::vec3& cameraUp) {
    const float life = particle.lifeRatio();
    const float size = particle.size * (1.0F + (particle.sizeEndScale - 1.0F) * life);
    const float half = size * 0.5F;
    const float fade = 1.0F - life * life;
    const glm::vec4 color{particle.color.r, particle.color.g, particle.color.b, particle.color.a * fade};

    const glm::vec3 right = cameraRight * half;
    const glm::vec3 up = cameraUp * half;
    const glm::vec3 center = particle.position;

    const glm::vec3 bottomLeft = center - right - up;
    const glm::vec3 bottomRight = center + right - up;
    const glm::vec3 topRight = center + right + up;
    const glm::vec3 topLeft = center - right + up;

    const auto pushVertex = [&out, &color](const glm::vec3& position, const float u, const float v) {
        out.push_back(position.x);
        out.push_back(position.y);
        out.push_back(position.z);
        out.push_back(u);
        out.push_back(v);
        out.push_back(color.r);
        out.push_back(color.g);
        out.push_back(color.b);
        out.push_back(color.a);
    };

    pushVertex(bottomLeft, 0.0F, 0.0F);
    pushVertex(bottomRight, 1.0F, 0.0F);
    pushVertex(topRight, 1.0F, 1.0F);
    pushVertex(topRight, 1.0F, 1.0F);
    pushVertex(topLeft, 0.0F, 1.0F);
    pushVertex(bottomLeft, 0.0F, 0.0F);
}

} // namespace

ParticleRenderer::ParticleRenderer(
    const std::string& vertexShaderPath,
    const std::string& fragmentShaderPath)
    : shader_(vertexShaderPath, fragmentShaderPath) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    ENGINE_GL_CHECK();
}

ParticleRenderer::~ParticleRenderer() {
    if (vbo_ != 0U) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0U) {
        glDeleteVertexArrays(1, &vao_);
    }
}

void ParticleRenderer::drawBatch(
    const std::vector<float>& vertexData,
    const glm::mat4& view,
    const glm::mat4& projection,
    const ParticleBlend blend) const {
    if (vertexData.empty()) {
        return;
    }

    shader_.use();
    shader_.setMat4("u_View", view);
    shader_.setMat4("u_Projection", projection);

    if (blend == ParticleBlend::Additive) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertexData.size() * sizeof(float)),
        vertexData.data(),
        GL_DYNAMIC_DRAW);
    const GLsizei stride = static_cast<GLsizei>(kFloatsPerVertex * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(5 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexData.size() / kFloatsPerVertex));
    engine::FrameProbe::instance().addDraw();
    glDisableVertexAttribArray(2);
    glBindVertexArray(0);
}

void ParticleRenderer::draw(
    const ParticleSystem& system,
    const glm::mat4& view,
    const glm::mat4& projection) const {
    const std::vector<Particle>& particles = system.particles();
    lastDrawnCount_ = particles.size();
    if (particles.empty()) {
        return;
    }

    alphaVertices_.clear();
    additiveVertices_.clear();
    alphaVertices_.reserve(particles.size() * kVerticesPerParticle * kFloatsPerVertex);
    additiveVertices_.reserve(particles.size() * kVerticesPerParticle * kFloatsPerVertex);

    const glm::vec3 cameraRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
    const glm::vec3 cameraUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

    for (const Particle& particle : particles) {
        std::vector<float>& target =
            particle.blend == ParticleBlend::Additive ? additiveVertices_ : alphaVertices_;
        appendParticleQuad(target, particle, cameraRight, cameraUp);
    }

    const GLboolean depthMaskWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    drawBatch(alphaVertices_, view, projection, ParticleBlend::Alpha);
    drawBatch(additiveVertices_, view, projection, ParticleBlend::Additive);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    if (depthMaskWasEnabled == GL_FALSE) {
        glDisable(GL_DEPTH_TEST);
    }
}

} // namespace render
