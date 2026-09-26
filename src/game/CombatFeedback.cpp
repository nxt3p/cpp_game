#include "game/CombatFeedback.hpp"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float kTraumaDecayPerSecond = 1.9F;
constexpr float kShakeFrequency = 27.0F;

} // namespace

void CombatFeedback::reset() noexcept {
    trauma_ = 0.0F;
    shakeClock_ = 0.0F;
    hitStopRemaining_ = 0.0F;
    hitStopScale_ = 1.0F;
    flashRemaining_ = 0.0F;
    flashPeakAlpha_ = 0.0F;
}

void CombatFeedback::update(const float deltaSeconds) noexcept {
    if (deltaSeconds <= 0.0F) {
        return;
    }
    trauma_ = std::max(0.0F, trauma_ - kTraumaDecayPerSecond * deltaSeconds);
    shakeClock_ += deltaSeconds;
    hitStopRemaining_ = std::max(0.0F, hitStopRemaining_ - deltaSeconds);
    flashRemaining_ = std::max(0.0F, flashRemaining_ - deltaSeconds);
}

void CombatFeedback::addTrauma(const float amount) noexcept {
    trauma_ = std::clamp(trauma_ + std::max(0.0F, amount), 0.0F, 1.0F);
}

void CombatFeedback::addHitStop(const float durationSeconds, const float timeScale) noexcept {
    if (durationSeconds <= 0.0F) {
        return;
    }
    hitStopRemaining_ = std::max(hitStopRemaining_, durationSeconds);
    hitStopScale_ = std::clamp(timeScale, 0.0F, 1.0F);
}

void CombatFeedback::addFlash(const glm::vec3& color, const float peakAlpha, const float durationSeconds) noexcept {
    if (durationSeconds <= 0.0F || peakAlpha <= 0.0F) {
        return;
    }
    flashRgb_ = color;
    flashPeakAlpha_ = std::clamp(peakAlpha, 0.0F, 1.0F);
    flashDuration_ = durationSeconds;
    flashRemaining_ = durationSeconds;
}

glm::vec3 CombatFeedback::shakeOffset() const noexcept {
    if (trauma_ <= 0.0F) {
        return glm::vec3(0.0F);
    }
    const float magnitude = trauma_ * trauma_ * maxShakeOffset_;
    // Two incommensurate sine waves approximate noise without a table lookup.
    const float t = shakeClock_ * kShakeFrequency;
    const float x = std::sin(t * 1.0F) * 0.6F + std::sin(t * 2.37F + 1.3F) * 0.4F;
    const float z = std::cos(t * 1.13F + 0.7F) * 0.6F + std::sin(t * 1.91F + 2.1F) * 0.4F;
    return glm::vec3(x * magnitude, 0.0F, z * magnitude);
}

float CombatFeedback::timeScale() const noexcept {
    return hitStopRemaining_ > 0.0F ? hitStopScale_ : 1.0F;
}

glm::vec4 CombatFeedback::flashColor() const noexcept {
    if (flashRemaining_ <= 0.0F || flashDuration_ <= 0.0F) {
        return glm::vec4(flashRgb_, 0.0F);
    }
    const float alpha = flashPeakAlpha_ * (flashRemaining_ / flashDuration_);
    return glm::vec4(flashRgb_, alpha);
}

} // namespace game
