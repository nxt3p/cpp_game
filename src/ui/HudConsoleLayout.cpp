#include "ui/HudConsoleLayout.hpp"

#include <algorithm>

namespace ui {

namespace {

constexpr float kRefBarWidth = 188.0F;
constexpr float kRefBadge = 40.0F;
constexpr float kRefClusterInset = 10.0F;
constexpr float kRefBadgeGap = 8.0F;
constexpr float kRefSlotSize = 42.0F;
constexpr float kRefSlotGap = 4.0F;
constexpr float kRefGroupGap = 14.0F;
constexpr float kRefXpBarHeight = 10.0F;
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

    const float inset = scale.dim(kRefClusterInset);
    const float badge = scale.dim(kRefBadge);
    const float labelH = scale.dim(14.0F);
    const float barH = scale.dim(16.0F);
    const float barGap = scale.dim(4.0F);
    const float labelGap = scale.dim(1.0F);
    const float badgeGap = scale.dim(kRefBadgeGap);
    float barW = scale.dim(kRefBarWidth);
    const float menuReserve =
        static_cast<float>(HudConsoleLayout::kMenuIconCount) * scale.dim(kRefMenuIcon) +
        static_cast<float>(HudConsoleLayout::kMenuIconCount - 1) * scale.dim(kRefMenuGap) +
        scale.dim(24.0F);
    const float maxBarRight = std::max(inset + badge + badgeGap + scale.dim(72.0F), screenW - menuReserve - scale.dim(120.0F));
    barW = std::min(barW, std::max(scale.dim(72.0F), maxBarRight - (inset + badge + badgeGap)));

    const float stackTop = layout.panel.y + scale.dim(20.0F);
    const float barX = inset + badge + badgeGap;
    layout.healthLabel = {barX, stackTop, barW, labelH};
    layout.healthBar = {barX, stackTop + labelH + labelGap, barW, barH};
    const float manaTop = layout.healthBar.y + barH + barGap;
    layout.manaLabel = {barX, manaTop, barW, labelH};
    layout.manaBar = {barX, manaTop + labelH + labelGap, barW, barH};
    const float barStackTop = layout.healthBar.y;
    const float barStackH = (layout.manaBar.y + barH) - barStackTop;
    layout.levelBadge = {inset, barStackTop + (barStackH - badge) * 0.5F, badge, badge};
    layout.levelLabel = layout.levelBadge;

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

    const float slotsLeft = layout.healthBar.x + layout.healthBar.width + scale.dim(16.0F);
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
        layout.healthBar.x,
        layout.manaBar.y + layout.manaBar.height + scale.dim(3.0F),
        layout.healthBar.width,
        labelH};

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
