#pragma once

#include <array>
#include <cstdint>

namespace systems {

enum class SkillId : std::uint8_t {
    None,
    PowerStrike,
    Whirlwind,
    Heal,
    Dash,
    Cleave,
    Firebolt,
    Shout,
    Slam,
};

enum class SkillAnimation : std::uint8_t {
    None,
    Attack,
    Attack2,
    Cast,
};

enum class SkillTargeting : std::uint8_t {
    Self,
    CurrentTarget,
    AreaAroundPlayer,
    Direction,
};

struct SkillDefinition {
    SkillId id{SkillId::None};
    const char* name{"None"};
    char glyph{'-'};
    int manaCost{0};
    float cooldownSeconds{0.0F};
    float damageMultiplier{1.0F};
    float areaRadius{0.0F};
    int healPercent{0};
    float dashDistance{0.0F};
    SkillTargeting targeting{SkillTargeting::Self};
    float colorR{1.0F};
    float colorG{1.0F};
    float colorB{1.0F};
    SkillAnimation animation{SkillAnimation::None};
};

[[nodiscard]] const SkillDefinition& skillDefinition(SkillId id) noexcept;

struct SkillCastResult {
    bool success{false};
    SkillId skill{SkillId::None};
    int manaSpent{0};
    const char* failureReason{nullptr};
};

/// Mana pool + hotkey slots with cooldown tracking. Pure logic, deterministic, save-friendly.
class SkillBar {
public:
    static constexpr int kSlotCount = 8;

    SkillBar();

    void setSlot(int slotIndex, SkillId skill) noexcept;
    [[nodiscard]] SkillId slot(int slotIndex) const noexcept;

    void setMaxMana(int maxMana) noexcept;
    void restoreMana() noexcept;
    void setMana(int mana) noexcept;
    void setManaRegenPerSecond(float regen) noexcept;

    [[nodiscard]] int mana() const noexcept { return static_cast<int>(mana_); }
    [[nodiscard]] int maxMana() const noexcept { return maxMana_; }
    [[nodiscard]] float manaRatio() const noexcept;

    void update(float deltaSeconds) noexcept;

    /// Attempts to cast the skill in `slotIndex`; consumes mana and starts the cooldown on success.
    [[nodiscard]] SkillCastResult tryCast(int slotIndex, bool hasTarget) noexcept;

    [[nodiscard]] float cooldownRemaining(int slotIndex) const noexcept;
    [[nodiscard]] float cooldownRatio(int slotIndex) const noexcept;
    [[nodiscard]] bool isReady(int slotIndex) const noexcept;

    void resetCooldowns() noexcept;

private:
    std::array<SkillId, kSlotCount> slots_{};
    std::array<float, kSlotCount> cooldowns_{};
    float mana_{0.0F};
    int maxMana_{50};
    float manaRegenPerSecond_{2.5F};
};

/// Level/class scaled mana pool: base 40 + 8 per level (+50% for casters via `casterBonus`).
[[nodiscard]] int computeMaxMana(int level, bool casterBonus) noexcept;

} // namespace systems
