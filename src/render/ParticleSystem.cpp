#include "render/ParticleSystem.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace render {

ParticleSystem::ParticleSystem(const std::size_t capacity, const std::uint32_t seed)
    : capacity_(std::max<std::size_t>(capacity, 16)), rng_(seed) {
    particles_.reserve(capacity_);
}

void ParticleSystem::clear() noexcept {
    particles_.clear();
    dustAccumulator_ = 0.0F;
}

float ParticleSystem::randomRange(const float minValue, const float maxValue) {
    if (maxValue <= minValue) {
        return minValue;
    }
    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(rng_);
}

glm::vec3 ParticleSystem::randomUnitVector() {
    const float z = randomRange(-1.0F, 1.0F);
    const float theta = randomRange(0.0F, 6.28318530718F);
    const float radial = std::sqrt(std::max(0.0F, 1.0F - z * z));
    return {radial * std::cos(theta), z, radial * std::sin(theta)};
}

void ParticleSystem::push(Particle particle) {
    if (particles_.size() >= capacity_) {
        // Recycle the oldest particle so bursts never silently vanish under load.
        auto oldest = std::max_element(
            particles_.begin(), particles_.end(), [](const Particle& a, const Particle& b) {
                return a.lifeRatio() < b.lifeRatio();
            });
        if (oldest != particles_.end()) {
            *oldest = particle;
        }
        ++droppedSpawnCount_;
        return;
    }
    particles_.push_back(particle);
}

void ParticleSystem::update(const float deltaSeconds) {
    if (deltaSeconds <= 0.0F) {
        return;
    }

    for (Particle& particle : particles_) {
        particle.ageSeconds += deltaSeconds;
        particle.velocity.y += particle.gravity * deltaSeconds;
        if (particle.drag > 0.0F) {
            const float damping = std::max(0.0F, 1.0F - particle.drag * deltaSeconds);
            particle.velocity *= damping;
        }
        particle.position += particle.velocity * deltaSeconds;
        if (particle.position.y < 0.02F && particle.gravity < 0.0F) {
            particle.position.y = 0.02F;
            particle.velocity.y = 0.0F;
        }
    }

    particles_.erase(
        std::remove_if(
            particles_.begin(),
            particles_.end(),
            [](const Particle& particle) { return !particle.alive(); }),
        particles_.end());
}

void ParticleSystem::emitBurst(const ParticleBurstParams& params) {
    for (int index = 0; index < params.count; ++index) {
        Particle particle{};
        glm::vec3 direction = randomUnitVector();
        direction.y = std::abs(direction.y) * (1.0F - params.coneUpBias) + params.coneUpBias;
        direction = glm::normalize(direction);

        particle.position = params.origin;
        particle.velocity = direction * randomRange(params.speedMin, params.speedMax);
        particle.size = randomRange(params.sizeMin, params.sizeMax);
        particle.lifetimeSeconds = randomRange(params.lifetimeMin, params.lifetimeMax);
        particle.gravity = params.gravity;
        particle.drag = params.drag;
        particle.sizeEndScale = params.sizeEndScale;
        particle.blend = params.blend;
        const float mix = randomRange(0.0F, 1.0F);
        particle.color = params.colorA * (1.0F - mix) + params.colorB * mix;
        push(particle);
    }
}

void ParticleSystem::spawnHitSparks(const glm::vec3& worldPosition, const bool critical) {
    ParticleBurstParams params{};
    params.origin = worldPosition + glm::vec3(0.0F, 1.1F, 0.0F);
    params.count = critical ? 26 : 10;
    params.speedMin = critical ? 3.5F : 2.0F;
    params.speedMax = critical ? 7.0F : 4.0F;
    params.sizeMin = critical ? 0.16F : 0.1F;
    params.sizeMax = critical ? 0.34F : 0.2F;
    params.lifetimeMin = 0.25F;
    params.lifetimeMax = critical ? 0.7F : 0.45F;
    params.gravity = -9.0F;
    params.drag = 2.0F;
    params.coneUpBias = 0.35F;
    params.colorA = critical ? glm::vec4(1.0F, 0.95F, 0.6F, 1.0F) : glm::vec4(1.0F, 0.75F, 0.35F, 1.0F);
    params.colorB = critical ? glm::vec4(1.0F, 0.45F, 0.1F, 1.0F) : glm::vec4(0.95F, 0.35F, 0.15F, 1.0F);
    params.blend = ParticleBlend::Additive;
    emitBurst(params);
}

void ParticleSystem::spawnDeathBurst(const glm::vec3& worldPosition, const glm::vec4& color) {
    ParticleBurstParams params{};
    params.origin = worldPosition + glm::vec3(0.0F, 0.9F, 0.0F);
    params.count = 22;
    params.speedMin = 0.8F;
    params.speedMax = 2.6F;
    params.sizeMin = 0.18F;
    params.sizeMax = 0.42F;
    params.lifetimeMin = 0.6F;
    params.lifetimeMax = 1.3F;
    params.gravity = -3.0F;
    params.drag = 1.2F;
    params.coneUpBias = 0.2F;
    params.sizeEndScale = 1.6F;
    params.colorA = color;
    params.colorB = glm::vec4(color.r * 0.4F, color.g * 0.4F, color.b * 0.4F, color.a);
    params.blend = ParticleBlend::Alpha;
    emitBurst(params);
}

void ParticleSystem::spawnLootPillar(
    const glm::vec3& worldPosition,
    const glm::vec4& color,
    const float intensity) {
    const int count = static_cast<int>(std::clamp(intensity, 0.5F, 3.0F) * 18.0F);
    for (int index = 0; index < count; ++index) {
        Particle particle{};
        const float angle = randomRange(0.0F, 6.28318530718F);
        const float radius = randomRange(0.05F, 0.35F * intensity);
        particle.position = worldPosition +
                            glm::vec3(std::cos(angle) * radius * 0.35F, randomRange(0.0F, 0.15F), std::sin(angle) * radius * 0.35F);
        particle.velocity = glm::vec3(
            std::cos(angle) * randomRange(0.02F, 0.18F),
            randomRange(3.2F, 6.4F) * std::max(intensity, 0.6F),
            std::sin(angle) * randomRange(0.02F, 0.18F));
        particle.size = randomRange(0.08F, 0.2F) * std::max(intensity, 0.7F);
        particle.lifetimeSeconds = randomRange(1.4F, 2.8F);
        particle.gravity = 0.0F;
        particle.drag = 0.4F;
        particle.sizeEndScale = 0.1F;
        particle.color = color;
        particle.blend = ParticleBlend::Additive;
        push(particle);
    }
}

void ParticleSystem::spawnSpellFlash(const glm::vec3& worldPosition, const glm::vec4& color) {
    ParticleBurstParams params{};
    params.origin = worldPosition + glm::vec3(0.0F, 1.2F, 0.0F);
    params.count = 18;
    params.speedMin = 2.0F;
    params.speedMax = 5.0F;
    params.sizeMin = 0.14F;
    params.sizeMax = 0.3F;
    params.lifetimeMin = 0.3F;
    params.lifetimeMax = 0.6F;
    params.gravity = 0.0F;
    params.drag = 4.0F;
    params.coneUpBias = 0.0F;
    params.sizeEndScale = 0.3F;
    params.colorA = color;
    params.colorB = glm::vec4(1.0F, 1.0F, 1.0F, color.a);
    params.blend = ParticleBlend::Additive;
    emitBurst(params);
}

void ParticleSystem::updateAmbientDust(
    const glm::vec3& center,
    const float radius,
    const float deltaSeconds,
    const float densityPerSecond) {
    if (deltaSeconds <= 0.0F || densityPerSecond <= 0.0F) {
        return;
    }

    dustAccumulator_ += deltaSeconds * densityPerSecond;
    int toSpawn = static_cast<int>(dustAccumulator_);
    dustAccumulator_ -= static_cast<float>(toSpawn);
    toSpawn = std::min(toSpawn, 24);

    for (int index = 0; index < toSpawn; ++index) {
        Particle particle{};
        const float angle = randomRange(0.0F, 6.28318530718F);
        const float distance = randomRange(1.0F, radius);
        particle.position = center + glm::vec3(
                                         std::cos(angle) * distance,
                                         randomRange(0.2F, 3.2F),
                                         std::sin(angle) * distance);
        particle.velocity = glm::vec3(randomRange(-0.25F, 0.25F), randomRange(0.05F, 0.3F), randomRange(-0.25F, 0.25F));
        particle.size = randomRange(0.05F, 0.11F);
        particle.lifetimeSeconds = randomRange(2.5F, 5.0F);
        particle.gravity = 0.0F;
        particle.drag = 0.0F;
        particle.sizeEndScale = 0.6F;
        particle.color = glm::vec4(0.85F, 0.8F, 0.65F, 0.45F);
        particle.blend = ParticleBlend::Additive;
        push(particle);
    }
}

} // namespace render
