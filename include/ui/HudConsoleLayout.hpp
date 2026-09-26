#pragma once

#include "ui/UiHitTest.hpp"
#include "ui/UiScale.hpp"

#include <algorithm>
#include <array>

namespace ui {

/// Diablo-style bottom console: health globe (left), mana globe (right),
/// hotkey skill slots and belt/potion slots in the middle, XP bar along the top edge.
struct HudConsoleLayout {
    static constexpr int kSkillSlotCount = 8;
    static constexpr int kBeltSlotCount = 4;

    Rect panel{};
    Rect xpBar{};
    Rect healthGlobe{};
    Rect manaGlobe{};
    Rect healthLabel{};
    Rect manaLabel{};
    Rect levelLabel{};
    Rect messageStrip{};
    Rect soulsLabel{};
    std::array<Rect, kSkillSlotCount> skillSlots{};
    std::array<Rect, kBeltSlotCount> beltSlots{};
    float slotSize{0.0F};
    float globeRadius{0.0F};
    float labelScale{1.5F};
    float hotkeyScale{1.3F};

    [[nodiscard]] float healthGlobeCenterX() const noexcept { return healthGlobe.x + healthGlobe.width * 0.5F; }
    [[nodiscard]] float healthGlobeCenterY() const noexcept { return healthGlobe.y + healthGlobe.height * 0.5F; }
    [[nodiscard]] float manaGlobeCenterX() const noexcept { return manaGlobe.x + manaGlobe.width * 0.5F; }
    [[nodiscard]] float manaGlobeCenterY() const noexcept { return manaGlobe.y + manaGlobe.height * 0.5F; }
};

/// Reference height of the console at 1280x720 (scaled by UiScale::dim).
constexpr float kReferenceHudConsoleHeight = 112.0F;

/// Punched hole in the generated globe ring, as a fraction of the globe radius.
constexpr float kGlobeRingInnerRadiusFraction = 0.64F;
/// Liquid disc stays inside that hole, including the surface bob.
constexpr float kGlobeLiquidRadiusFraction = 0.58F;
constexpr float kGlobeWaveAmplitudeFraction = 0.02F;
/// Reference-pixel hotkey strip at the bottom of a skill slot (scaled by UiScale::dim).
constexpr float kHudHotkeyBand = 14.0F;

[[nodiscard]] inline Rect hudGlyphRect(const Rect& slot, const float hotkeyBand) noexcept {
    const float band = std::clamp(hotkeyBand, 0.0F, slot.height * 0.4F);
    return {slot.x, slot.y, slot.width, std::max(slot.height - band, 1.0F)};
}

[[nodiscard]] inline Rect hudHotkeyRect(const Rect& slot, const float hotkeyBand) noexcept {
    const float band = std::clamp(hotkeyBand, 0.0F, slot.height * 0.4F);
    return {slot.x, slot.y + slot.height - band, slot.width, band};
}

/// Cooldown pie radius that sits inside the hotbar frame border.
[[nodiscard]] inline float hudCooldownRadius(const Rect& slot) noexcept {
    return std::min(slot.width, slot.height) * 0.42F;
}

[[nodiscard]] HudConsoleLayout computeHudConsoleLayout(const UiScale& scale) noexcept;

} // namespace ui
