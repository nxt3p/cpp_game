#include "systems/SkillBar.hpp"

#include <algorithm>

namespace systems {

namespace {

constexpr SkillDefinition kSkillTable[] = {
    {SkillId::None, "None", '-', 0, 0.0F, 1.0F, 0.0F, 0, 0.0F, SkillTargeting::Self, 0.5F, 0.5F, 0.5F, SkillAnimation::None},
    {SkillId::PowerStrike, "Power Strike", 'P', 12, 3.0F, 2.6F, 0.0F, 0, 0.0F, SkillTargeting::CurrentTarget, 1.0F, 0.55F, 0.2F, SkillAnimation::Attack},
    {SkillId::Whirlwind, "Whirlwind", 'W', 22, 6.0F, 1.4F, 4.5F, 0, 0.0F, SkillTargeting::AreaAroundPlayer, 0.55F, 0.85F, 1.0F, SkillAnimation::Attack2},
    {SkillId::Heal, "Heal", 'H', 18, 8.0F, 0.0F, 0.0F, 35, 0.0F, SkillTargeting::Self, 0.35F, 1.0F, 0.45F, SkillAnimation::Cast},
    {SkillId::Dash, "Dash", 'D', 8, 2.0F, 0.0F, 0.0F, 0, 6.0F, SkillTargeting::Direction, 0.8F, 0.75F, 1.0F, SkillAnimation::None},
    {SkillId::Cleave, "Cleave", 'C', 14, 4.0F, 1.8F, 2.8F, 0, 0.0F, SkillTargeting::AreaAroundPlayer, 1.0F, 0.45F, 0.2F, SkillAnimation::Attack2},
    {SkillId::Firebolt, "Firebolt", 'F', 16, 3.5F, 2.1F, 0.0F, 0, 0.0F, SkillTargeting::CurrentTarget, 1.0F, 0.55F, 0.15F, SkillAnimation::Cast},
    {SkillId::Shout, "Shout", 'S', 10, 8.0F, 0.0F, 0.0F, 12, 0.0F, SkillTargeting::Self, 0.95F, 0.75F, 0.3F, SkillAnimation::Cast},
    {SkillId::Slam, "Slam", 'L', 20, 5.0F, 2.0F, 3.2F, 0, 0.0F, SkillTargeting::AreaAroundPlayer, 0.85F, 0.55F, 0.2F, SkillAnimation::Attack2},
};

[[nodiscard]] bool validSlot(const int slotIndex) noexcept {
    return slotIndex >= 0 && slotIndex < SkillBar::kSlotCount;
}

} // namespace

const SkillDefinition& skillDefinition(const SkillId id) noexcept {
    for (const SkillDefinition& definition : kSkillTable) {
        if (definition.id == id) {
            return definition;
        }
    }
    return kSkillTable[0];
}

int computeMaxMana(const int level, const bool casterBonus) noexcept {
    const int base = 40 + std::max(1, level) * 8;
    return casterBonus ? base + base / 2 : base;
}

SkillBar::SkillBar() {
    slots_ = {
        SkillId::PowerStrike,
        SkillId::Whirlwind,
        SkillId::Heal,
        SkillId::Dash,
        SkillId::Cleave,
        SkillId::Firebolt,
        SkillId::Shout,
        SkillId::Slam};
    cooldowns_.fill(0.0F);
    mana_ = static_cast<float>(maxMana_);
}

void SkillBar::setSlot(const int slotIndex, const SkillId skill) noexcept {
    if (validSlot(slotIndex)) {
        slots_[static_cast<std::size_t>(slotIndex)] = skill;
    }
}

SkillId SkillBar::slot(const int slotIndex) const noexcept {
    return validSlot(slotIndex) ? slots_[static_cast<std::size_t>(slotIndex)] : SkillId::None;
}

void SkillBar::setMaxMana(const int maxMana) noexcept {
    maxMana_ = std::max(1, maxMana);
    mana_ = std::min(mana_, static_cast<float>(maxMana_));
}

void SkillBar::restoreMana() noexcept {
    mana_ = static_cast<float>(maxMana_);
}

void SkillBar::setMana(const int mana) noexcept {
    mana_ = static_cast<float>(std::clamp(mana, 0, maxMana_));
}

void SkillBar::setManaRegenPerSecond(const float regen) noexcept {
    manaRegenPerSecond_ = std::max(0.0F, regen);
}

float SkillBar::manaRatio() const noexcept {
    return maxMana_ > 0 ? std::clamp(mana_ / static_cast<float>(maxMana_), 0.0F, 1.0F) : 0.0F;
}

void SkillBar::update(const float deltaSeconds) noexcept {
    if (deltaSeconds <= 0.0F) {
        return;
    }
    mana_ = std::min(static_cast<float>(maxMana_), mana_ + manaRegenPerSecond_ * deltaSeconds);
    for (float& cooldown : cooldowns_) {
        cooldown = std::max(0.0F, cooldown - deltaSeconds);
    }
}

SkillCastResult SkillBar::tryCast(const int slotIndex, const bool hasTarget) noexcept {
    SkillCastResult result{};
    if (!validSlot(slotIndex)) {
        result.failureReason = "Invalid skill slot";
        return result;
    }

    const SkillId skill = slots_[static_cast<std::size_t>(slotIndex)];
    result.skill = skill;
    const SkillDefinition& definition = skillDefinition(skill);
    if (skill == SkillId::None) {
        result.failureReason = "Empty slot";
        return result;
    }
    if (cooldowns_[static_cast<std::size_t>(slotIndex)] > 0.0F) {
        result.failureReason = "On cooldown";
        return result;
    }
    if (static_cast<int>(mana_) < definition.manaCost) {
        result.failureReason = "Not enough mana";
        return result;
    }
    if (definition.targeting == SkillTargeting::CurrentTarget && !hasTarget) {
        result.failureReason = "No target";
        return result;
    }

    mana_ -= static_cast<float>(definition.manaCost);
    cooldowns_[static_cast<std::size_t>(slotIndex)] = definition.cooldownSeconds;
    result.success = true;
    result.manaSpent = definition.manaCost;
    return result;
}

float SkillBar::cooldownRemaining(const int slotIndex) const noexcept {
    return validSlot(slotIndex) ? cooldowns_[static_cast<std::size_t>(slotIndex)] : 0.0F;
}

float SkillBar::cooldownRatio(const int slotIndex) const noexcept {
    if (!validSlot(slotIndex)) {
        return 0.0F;
    }
    const SkillDefinition& definition = skillDefinition(slots_[static_cast<std::size_t>(slotIndex)]);
    if (definition.cooldownSeconds <= 0.0F) {
        return 0.0F;
    }
    return std::clamp(cooldowns_[static_cast<std::size_t>(slotIndex)] / definition.cooldownSeconds, 0.0F, 1.0F);
}

bool SkillBar::isReady(const int slotIndex) const noexcept {
    return validSlot(slotIndex) && cooldowns_[static_cast<std::size_t>(slotIndex)] <= 0.0F;
}

void SkillBar::resetCooldowns() noexcept {
    cooldowns_.fill(0.0F);
}

} // namespace systems
