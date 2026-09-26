#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace game {

/// Screen-space "juice" state: trauma-driven camera shake, hit-stop time scaling and full-screen flashes.
/// Pure logic (no GL) so it can be unit tested and driven from any render loop.
class CombatFeedback {
public:
    void update(float deltaSeconds) noexcept;
    void reset() noexcept;

    /// Adds shake energy in [0,1]; accumulates and clamps. Shake magnitude is trauma squared.
    void addTrauma(float amount) noexcept;

    /// Briefly scales world time (e.g. 0.08s at 0.15x) so heavy hits land with weight.
    void addHitStop(float durationSeconds, float timeScale = 0.15F) noexcept;

    /// Full-screen tinted flash that fades linearly over `durationSeconds`.
    void addFlash(const glm::vec3& color, float peakAlpha, float durationSeconds) noexcept;

    [[nodiscard]] float trauma() const noexcept { return trauma_; }
    [[nodiscard]] glm::vec3 shakeOffset() const noexcept;
    [[nodiscard]] float timeScale() const noexcept;
    [[nodiscard]] bool inHitStop() const noexcept { return hitStopRemaining_ > 0.0F; }
    [[nodiscard]] glm::vec4 flashColor() const noexcept;
    [[nodiscard]] bool hasFlash() const noexcept { return flashRemaining_ > 0.0F; }

    void setMaxShakeOffset(float worldUnits) noexcept { maxShakeOffset_ = worldUnits; }

private:
    float trauma_{0.0F};
    float shakeClock_{0.0F};
    float maxShakeOffset_{0.55F};
    float hitStopRemaining_{0.0F};
    float hitStopScale_{1.0F};
    glm::vec3 flashRgb_{1.0F};
    float flashPeakAlpha_{0.0F};
    float flashDuration_{0.0F};
    float flashRemaining_{0.0F};
};

} // namespace game
