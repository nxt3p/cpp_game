#include "render/AnimationStateMachine.hpp"

#include <algorithm>
#include <cmath>

namespace render {

namespace {

constexpr float kPi = 3.14159265358979F;
constexpr float kSector = kPi / 4.0F;
constexpr float kHysteresis = kSector * 0.18F;
constexpr float kMinDeltaSq = 0.0004F;

[[nodiscard]] float facingAngle(const SpriteFacing8 facing) noexcept {
    // Angle measured from +Y (south / toward camera) rotating toward -X (west).
    return static_cast<float>(facing) * kSector;
}

} // namespace

SpriteFacing8 facing8FromDelta(const glm::vec2& delta, const SpriteFacing8 current) noexcept {
    if (delta.x * delta.x + delta.y * delta.y <= kMinDeltaSq) {
        return current;
    }

    // atan2(-x, y): 0 = south, +pi/2 = west, pi = north, -pi/2 = east.
    float angle = std::atan2(-delta.x, delta.y);
    if (angle < 0.0F) {
        angle += 2.0F * kPi;
    }

    const float currentAngle = facingAngle(current);
    float diff = angle - currentAngle;
    while (diff > kPi) {
        diff -= 2.0F * kPi;
    }
    while (diff < -kPi) {
        diff += 2.0F * kPi;
    }
    if (std::abs(diff) <= kSector * 0.5F + kHysteresis) {
        return current;
    }

    const int index = static_cast<int>(std::floor((angle + kSector * 0.5F) / kSector)) % 8;
    return static_cast<SpriteFacing8>(index);
}

SpriteFacing facing4From8(const SpriteFacing8 facing) noexcept {
    switch (facing) {
    case SpriteFacing8::South:
        return SpriteFacing::Down;
    case SpriteFacing8::North:
        return SpriteFacing::Up;
    case SpriteFacing8::West:
    case SpriteFacing8::SouthWest:
    case SpriteFacing8::NorthWest:
        return SpriteFacing::Left;
    case SpriteFacing8::East:
    case SpriteFacing8::SouthEast:
    case SpriteFacing8::NorthEast:
        return SpriteFacing::Right;
    }
    return SpriteFacing::Down;
}

glm::vec2 facing8Direction(const SpriteFacing8 facing) noexcept {
    const float angle = facingAngle(facing);
    return {-std::sin(angle), std::cos(angle)};
}

void AnimationStateMachine::enter(const AnimState state, const float duration) noexcept {
    state_ = state;
    stateTime_ = 0.0F;
    stateDuration_ = std::max(0.0F, duration);
}

void AnimationStateMachine::reset() noexcept {
    enter(AnimState::Idle, 0.0F);
}

bool AnimationStateMachine::isBusy() const noexcept {
    return state_ == AnimState::Attack || state_ == AnimState::Attack2 || state_ == AnimState::Cast ||
           state_ == AnimState::Hit || state_ == AnimState::Death || state_ == AnimState::Dead;
}

float AnimationStateMachine::stateProgress() const noexcept {
    if (stateDuration_ <= 0.0F) {
        return 1.0F;
    }
    return std::clamp(stateTime_ / stateDuration_, 0.0F, 1.0F);
}

void AnimationStateMachine::update(const float deltaSeconds, const bool moving) noexcept {
    stateTime_ += std::max(0.0F, deltaSeconds);

    switch (state_) {
    case AnimState::Death:
        if (stateTime_ >= stateDuration_) {
            enter(AnimState::Dead, 0.0F);
        }
        return;
    case AnimState::Dead:
        return;
    case AnimState::Attack:
    case AnimState::Attack2:
    case AnimState::Cast:
    case AnimState::Hit:
        if (stateTime_ < stateDuration_) {
            return;
        }
        enter(moving ? AnimState::Walk : AnimState::Idle, 0.0F);
        return;
    case AnimState::Walk:
        if (!moving) {
            enter(AnimState::Idle, 0.0F);
        }
        return;
    case AnimState::Idle:
        if (moving) {
            enter(AnimState::Walk, 0.0F);
        }
        return;
    }
}

void AnimationStateMachine::triggerAttack(const float durationSeconds) noexcept {
    if (state_ == AnimState::Death || state_ == AnimState::Dead) {
        return;
    }
    enter(AnimState::Attack, durationSeconds);
}

void AnimationStateMachine::triggerAttack2(const float durationSeconds) noexcept {
    if (state_ == AnimState::Death || state_ == AnimState::Dead) {
        return;
    }
    enter(AnimState::Attack2, durationSeconds);
}

void AnimationStateMachine::triggerCast(const float durationSeconds) noexcept {
    if (state_ == AnimState::Death || state_ == AnimState::Dead) {
        return;
    }
    enter(AnimState::Cast, durationSeconds);
}

void AnimationStateMachine::triggerHit(const float durationSeconds) noexcept {
    if (state_ == AnimState::Death || state_ == AnimState::Dead || state_ == AnimState::Attack ||
        state_ == AnimState::Attack2 || state_ == AnimState::Cast) {
        return;
    }
    enter(AnimState::Hit, durationSeconds);
}

void AnimationStateMachine::triggerDeath(const float durationSeconds) noexcept {
    if (state_ == AnimState::Dead) {
        return;
    }
    enter(AnimState::Death, durationSeconds);
}

void AnimationStateMachine::setFacingFromDelta(const glm::vec2& delta) noexcept {
    facing_ = facing8FromDelta(delta, facing_);
}

SpriteClip AnimationStateMachine::clip() const noexcept {
    switch (state_) {
    case AnimState::Walk:
        return SpriteClip::Walk;
    case AnimState::Attack:
        return SpriteClip::Attack;
    case AnimState::Attack2:
        return SpriteClip::Attack2;
    case AnimState::Cast:
        return SpriteClip::Cast;
    case AnimState::Hit:
        return SpriteClip::Hit;
    case AnimState::Death:
    case AnimState::Dead:
        return SpriteClip::Death;
    case AnimState::Idle:
    default:
        return SpriteClip::Idle;
    }
}

} // namespace render
