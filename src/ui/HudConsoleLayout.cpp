#include "ui/HudConsoleLayout.hpp"

#include <algorithm>

namespace ui {

namespace {

constexpr float kRefHealthDiameter = 92.0F;
constexpr float kRefManaDiameter = 76.0F;
constexpr float kRefGlobeInset = 12.0F;
constexpr float kRefSlotSize = 42.0F;
constexpr float kRefSlotGap = 4.0F;
constexpr float kRefGroupGap = 14.0F;
constexpr float kRefXpBarHeight = 4.0F;
constexpr float kRefMessageHeight = 26.0F;
constexpr float kRefMessageWidth = 740.0F;
constexpr float kRefMenuIcon = 34.0F;
constexpr float kRefMenuGap = 6.0F;

} // namespace

HudConsoleLayout computeHudConsoleLayout(const UiScale& scale) noexcept {
    HudConsoleLayout layout{};

    const float screenW = static_cast<float>(scale.width);
    const float screenH = static_cast<float>(scale.height);
    const float consoleH = scale.dim(kReferenceHudConsoleHeight);

    layout.panel = {0.0F, screenH - consoleH, screenW, consoleH};

    const float globeD = scale.dim(kRefHealthDiameter);
    const float manaD = scale.dim(kRefManaDiameter);
    const float globeInset = scale.dim(kRefGlobeInset);
    const float globeY = layout.panel.y + (consoleH - globeD) * 0.55F;
    layout.globeRadius = globeD * 0.5F;
    layout.healthGlobe = {globeInset, globeY, globeD, globeD};
    layout.manaGlobe = {
        globeInset + globeD * 0.58F,
        globeY + (globeD - manaD) * 0.55F,
        manaD,
        manaD};

    layout.levelBadge = {
        layout.healthGlobe.x + globeD * 0.08F,
        layout.healthGlobe.y + globeD - scale.dim(18.0F),
        globeD * 0.95F,
        scale.dim(18.0F)};
    layout.levelLabel = layout.levelBadge;

    layout.healthLabel = {
        layout.healthGlobe.x,
        layout.healthGlobe.y + globeD * 0.36F,
        globeD,
        scale.dim(16.0F)};
    layout.manaLabel = {
        layout.manaGlobe.x,
        layout.manaGlobe.y + manaD * 0.34F,
        manaD,
        scale.dim(14.0F)};

    const float menuSize = scale.dim(kRefMenuIcon);
    const float menuGap = scale.dim(kRefMenuGap);
    const float menuMargin = scale.dim(10.0F);
    const float menuRowW =
        static_cast<float>(HudConsoleLayout::kMenuIconCount) * menuSize +
        static_cast<float>(HudConsoleLayout::kMenuIconCount - 1) * menuGap;
    const float menuX = std::max(0.0F, screenW - menuMargin - menuRowW);
    const float menuY = layout.panel.y + (consoleH - menuSize) * 0.62F;
    for (int index = 0; index < HudConsoleLayout::kMenuIconCount; ++index) {
        layout.menuIcons[static_cast<std::size_t>(index)] = {
            menuX + static_cast<float>(index) * (menuSize + menuGap),
            menuY,
            menuSize,
            menuSize};
    }

    const float slotsLeft = layout.manaGlobe.x + layout.manaGlobe.width + scale.dim(14.0F);
    const float slotsRight = menuX - scale.dim(12.0F);
    const float available = std::max(scale.dim(160.0F), slotsRight - slotsLeft);

    float slot = scale.dim(kRefSlotSize);
    float gap = scale.dim(kRefSlotGap);
    float group = scale.dim(kRefGroupGap);
    const auto rowWidth = [](const float slotSize, const float slotGap, const float groupGap) noexcept {
        return 12.0F * slotSize + 10.0F * slotGap + groupGap;
    };
    while (rowWidth(slot, gap, group) > available && slot > scale.dim(22.0F)) {
        slot *= 0.92F;
        gap *= 0.92F;
        group *= 0.92F;
    }

    layout.slotSize = slot;
    const float slotsY = layout.panel.y + (consoleH - slot) * 0.56F;
    float cursorX = slotsLeft;
    for (int index = 0; index < HudConsoleLayout::kSkillSlotCount; ++index) {
        layout.skillSlots[static_cast<std::size_t>(index)] = {cursorX, slotsY, slot, slot};
        cursorX += slot + gap;
    }
    cursorX += group - gap;
    for (int index = 0; index < HudConsoleLayout::kBeltSlotCount; ++index) {
        layout.beltSlots[static_cast<std::size_t>(index)] = {cursorX, slotsY, slot, slot};
        cursorX += slot + gap;
    }

    const float xpWidth = std::max(scale.dim(80.0F), slotsRight - slotsLeft);
    layout.xpBar = {slotsLeft, layout.panel.y + scale.dim(6.0F), xpWidth, scale.dim(kRefXpBarHeight)};

    layout.soulsLabel = {
        slotsLeft,
        layout.panel.y + consoleH - scale.dim(16.0F),
        std::min(scale.dim(280.0F), xpWidth),
        scale.dim(14.0F)};

    const float messageW = std::min(scale.dim(kRefMessageWidth), screenW * 0.7F);
    layout.messageStrip = {
        (screenW - messageW) * 0.5F,
        scale.dim(10.0F),
        messageW,
        scale.dim(kRefMessageHeight)};

    layout.labelScale = scale.dim(1.45F);
    layout.hotkeyScale = scale.dim(1.15F);
    return layout;
}

} // namespace ui
