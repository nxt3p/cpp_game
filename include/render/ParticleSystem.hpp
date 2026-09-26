#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace render {

/// Blend mode a particle is drawn with. Additive particles glow; alpha particles occlude.
enum class ParticleBlend : std::uint8_t {
    Alpha,
    Additive,
};

struct Particle {
    glm::vec3 position{0.0F};
    glm::vec3 velocity{0.0F};
    glm::vec4 color{1.0F};
    float size{0.2F};
    float ageSeconds{0.0F};
    float lifetimeSeconds{1.0F};
    float gravity{0.0F};
    float drag{0.0F};
    float sizeEndScale{1.0F};
    ParticleBlend blend{ParticleBlend::Alpha};

    [[nodiscard]] bool alive() const noexcept { return ageSeconds < lifetimeSeconds; }
    [[nodiscard]] float lifeRatio() const noexcept {
        return lifetimeSeconds > 0.0F ? ageSeconds / lifetimeSeconds : 1.0F;
    }
};

struct ParticleBurstParams {
    glm::vec3 origin{0.0F};
    int count{12};
    float speedMin{1.0F};
    float speedMax{3.0F};
    float sizeMin{0.12F};
    float sizeMax{0.28F};
    float lifetimeMin{0.35F};
    float lifetimeMax{0.75F};
    float gravity{-6.0F};
    float drag{1.5F};
    float coneUpBias{0.6F};
    float sizeEndScale{0.2F};
    glm::vec4 colorA{1.0F, 0.85F, 0.4F, 1.0F};
    glm::vec4 colorB{1.0F, 0.4F, 0.15F, 1.0F};
    ParticleBlend blend{ParticleBlend::Additive};
};

/// GL-free particle pool. All spawning helpers are deterministic given the seeded RNG.
class ParticleSystem {
public:
    explicit ParticleSystem(std::size_t capacity = 2048, std::uint32_t seed = 0x9A7C1E5FU);

    void update(float deltaSeconds);
    void clear() noexcept;

    void emitBurst(const ParticleBurstParams& params);

    /// Melee/ranged hit sparks that fan upward from the impact point.
    void spawnHitSparks(const glm::vec3& worldPosition, bool critical);

    /// Short-lived blood/soul mist when a mob dies.
    void spawnDeathBurst(const glm::vec3& worldPosition, const glm::vec4& color);

    /// Diablo-style light pillar rising from a dropped item.
    void spawnLootPillar(const glm::vec3& worldPosition, const glm::vec4& color, float intensity);

    /// Spell cast flash at the caster.
    void spawnSpellFlash(const glm::vec3& worldPosition, const glm::vec4& color);

    /// Slowly drifting motes around the player; call every frame with `deltaSeconds`.
    void updateAmbientDust(
        const glm::vec3& center,
        float radius,
        float deltaSeconds,
        float densityPerSecond);

    [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particles_; }
    [[nodiscard]] std::size_t aliveCount() const noexcept { return particles_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t droppedSpawnCount() const noexcept { return droppedSpawnCount_; }

private:
    void push(Particle particle);
    [[nodiscard]] float randomRange(float minValue, float maxValue);
    [[nodiscard]] glm::vec3 randomUnitVector();

    std::size_t capacity_{2048};
    std::vector<Particle> particles_{};
    std::mt19937 rng_;
    float dustAccumulator_{0.0F};
    std::size_t droppedSpawnCount_{0};
};

} // namespace render
