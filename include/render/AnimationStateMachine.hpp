#pragma once

#include "render/SpriteSheet.hpp"

#include <glm/vec2.hpp>

#include <cstdint>

namespace render {

/// Eight compass facings (S = toward camera / +Z in world space).
enum class SpriteFacing8 : std::uint8_t {
    South,
    SouthWest,
    West,
    NorthWest,
    North,
    NorthEast,
    East,
    SouthEast,
};

/// Picks the 8-way facing for a movement delta (x right, y = world Z / down-screen).
/// Applies a small hysteresis so facings do not flicker at sector boundaries.
[[nodiscard]] SpriteFacing8 facing8FromDelta(const glm::vec2& delta, SpriteFacing8 current) noexcept;

/// Reduces an 8-way facing to the 4 rows present on the sprite sheets.
/// Diagonals collapse to their horizontal component so strafing reads left/right.
[[nodiscard]] SpriteFacing facing4From8(SpriteFacing8 facing) noexcept;

/// Unit vector for a facing; useful for spawning effects in front of a character.
[[nodiscard]] glm::vec2 facing8Direction(SpriteFacing8 facing) noexcept;

enum class AnimState : std::uint8_t {
    Idle,
    Walk,
    Attack,
    Attack2,
    Cast,
    Hit,
    Death,
    Dead,
};

/// Priority-driven animation state machine. Death > Attack > Hit > Walk/Idle.
/// Timed states expire back to Idle/Walk; Death is terminal and reports `finished()`.
class AnimationStateMachine {
public:
    void update(float deltaSeconds, bool moving) noexcept;

    void triggerAttack(float durationSeconds) noexcept;
    void triggerAttack2(float durationSeconds) noexcept;
    void triggerCast(float durationSeconds) noexcept;
    void triggerHit(float durationSeconds) noexcept;
    void triggerDeath(float durationSeconds) noexcept;
    void reset() noexcept;

    void setFacingFromDelta(const glm::vec2& delta) noexcept;
    void setFacing(SpriteFacing8 facing) noexcept { facing_ = facing; }

    [[nodiscard]] AnimState state() const noexcept { return state_; }
    [[nodiscard]] float stateTime() const noexcept { return stateTime_; }
    [[nodiscard]] float stateProgress() const noexcept;
    [[nodiscard]] bool finished() const noexcept { return state_ == AnimState::Dead; }
    [[nodiscard]] bool isDying() const noexcept { return state_ == AnimState::Death; }
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] SpriteFacing8 facing8() const noexcept { return facing_; }
    [[nodiscard]] SpriteFacing facing4() const noexcept { return facing4From8(facing_); }
    [[nodiscard]] SpriteClip clip() const noexcept;

private:
    void enter(AnimState state, float duration) noexcept;

    AnimState state_{AnimState::Idle};
    float stateTime_{0.0F};
    float stateDuration_{0.0F};
    SpriteFacing8 facing_{SpriteFacing8::South};
};

} // namespace render
