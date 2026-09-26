#include "ui/HudConsoleLayout.hpp"

#include <algorithm>

namespace ui {

namespace {

constexpr float kRefGlobeDiameter = 104.0F;
constexpr float kRefGlobeInset = 18.0F;
constexpr float kRefSlotSize = 46.0F;
constexpr float kRefSlotGap = 6.0F;
constexpr float kRefXpBarHeight = 6.0F;
constexpr float kRefMessageHeight = 26.0F;
constexpr float kRefMessageWidth = 740.0F;

} // namespace

HudConsoleLayout computeHudConsoleLayout(const UiScale& scale) noexcept {
    HudConsoleLayout layout{};

    const float screenW = static_cast<float>(scale.width);
    const float screenH = static_cast<float>(scale.height);
    const float consoleH = scale.dim(kReferenceHudConsoleHeight);

    layout.panel = {0.0F, screenH - consoleH, screenW, consoleH};
    layout.xpBar = {
        scale.dim(kRefGlobeDiameter + kRefGlobeInset * 2.0F),
        layout.panel.y,
        screenW - scale.dim(kRefGlobeDiameter + kRefGlobeInset * 2.0F) * 2.0F,
        scale.dim(kRefXpBarHeight)};

    const float globeD = scale.dim(kRefGlobeDiameter);
    const float globeInset = scale.dim(kRefGlobeInset);
    const float globeY = layout.panel.y + (consoleH - globeD) * 0.5F + scale.dim(4.0F);
    layout.globeRadius = globeD * 0.5F;
    layout.healthGlobe = {globeInset, globeY, globeD, globeD};
    layout.manaGlobe = {screenW - globeInset - globeD, globeY, globeD, globeD};

    layout.healthLabel = {
        layout.healthGlobe.x,
        layout.healthGlobe.y + globeD * 0.42F,
        globeD,
        scale.dim(18.0F)};
    layout.manaLabel = {
        layout.manaGlobe.x,
        layout.manaGlobe.y + globeD * 0.42F,
        globeD,
        scale.dim(18.0F)};

    layout.slotSize = scale.dim(kRefSlotSize);
    const float slotGap = scale.dim(kRefSlotGap);
    const int totalSlots = HudConsoleLayout::kSkillSlotCount + HudConsoleLayout::kBeltSlotCount;
    const float groupGap = scale.dim(28.0F);
    const float slotsWidth =
        static_cast<float>(totalSlots) * layout.slotSize +
        static_cast<float>(totalSlots - 2) * slotGap + groupGap;
    const float slotsX = (screenW - slotsWidth) * 0.5F;
    const float slotsY = layout.panel.y + (consoleH - layout.slotSize) * 0.5F + scale.dim(6.0F);

    float cursorX = slotsX;
    for (int index = 0; index < HudConsoleLayout::kSkillSlotCount; ++index) {
        layout.skillSlots[static_cast<std::size_t>(index)] = {cursorX, slotsY, layout.slotSize, layout.slotSize};
        cursorX += layout.slotSize + slotGap;
    }
    cursorX += groupGap - slotGap;
    for (int index = 0; index < HudConsoleLayout::kBeltSlotCount; ++index) {
        layout.beltSlots[static_cast<std::size_t>(index)] = {cursorX, slotsY, layout.slotSize, layout.slotSize};
        cursorX += layout.slotSize + slotGap;
    }

    layout.levelLabel = {
        slotsX,
        layout.panel.y + scale.dim(kRefXpBarHeight + 4.0F),
        slotsWidth,
        scale.dim(16.0F)};

    layout.soulsLabel = {
        layout.healthGlobe.x + globeD + scale.dim(6.0F),
        layout.panel.y + consoleH - scale.dim(22.0F),
        std::max(slotsX - (layout.healthGlobe.x + globeD) - scale.dim(12.0F), scale.dim(80.0F)),
        scale.dim(18.0F)};

    const float messageW = std::min(scale.dim(kRefMessageWidth), screenW * 0.7F);
    layout.messageStrip = {
        (screenW - messageW) * 0.5F,
        scale.dim(10.0F),
        messageW,
        scale.dim(kRefMessageHeight)};

    layout.labelScale = scale.dim(1.5F);
    layout.hotkeyScale = scale.dim(1.3F);
    return layout;
}

} // namespace ui
