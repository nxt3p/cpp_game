#pragma once

#include "ui/UiHitTest.hpp"
#include "ui/UiScale.hpp"

#include <algorithm>
#include <array>

namespace ui {

/// Bottom chrome: stacked health and mana bars on the left, skill quickbar and
/// potion belt in the middle, menu icons on the right.
struct HudConsoleLayout {
    static constexpr int kSkillSlotCount = 8;
    static constexpr int kBeltSlotCount = 4;
    static constexpr int kMenuIconCount = 5;

    Rect panel{};
    Rect xpBar{};
    Rect healthBar{};
    Rect manaBar{};
    Rect healthLabel{};
    Rect manaLabel{};
    Rect levelLabel{};
    Rect levelBadge{};
    Rect messageStrip{};
    Rect soulsLabel{};
    std::array<Rect, kSkillSlotCount> skillSlots{};
    std::array<Rect, kBeltSlotCount> beltSlots{};
    std::array<Rect, kMenuIconCount> menuIcons{};
    float slotSize{0.0F};
    float labelScale{1.5F};
    float hotkeyScale{1.3F};
};

/// Reference height of the console at 1280x720 (scaled by UiScale::dim).
constexpr float kReferenceHudConsoleHeight = 112.0F;

/// Punched hole in the generated globe ring, as a fraction of the globe radius.
/// Kept for the liquid-fill math even though the HUD draws bars.
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
