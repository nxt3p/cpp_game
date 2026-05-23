#include "game/CombatSystem.hpp"

#include "systems/RunProgression.hpp"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

bool isAttackableKind(gameplay::EntityKind kind) noexcept {
    return kind == gameplay::EntityKind::ENEMY_MOB || kind == gameplay::EntityKind::ENEMY_BOSS;
}

int scaleStat(const int baseValue, const float multiplier) {
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(baseValue) * multiplier)));
}

} // namespace

MobCombatProfile CombatSystem::profileFor(
    const gameplay::EntityKind kind,
    const float hpMultiplier,
    const float xpMultiplier) noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENEMY_MOB:
        return {scaleStat(72, hpMultiplier), scaleStat(35, xpMultiplier)};
    case gameplay::EntityKind::ENEMY_BOSS:
        return {scaleStat(240, hpMultiplier), scaleStat(140, xpMultiplier)};
    default:
        return {0, 0};
    }
}

void CombatSystem::setDifficultyModifiers(const systems::DifficultyModifiers& modifiers) noexcept {
    mobHpMultiplier_ = modifiers.mobHpMultiplier;
    mobXpMultiplier_ = modifiers.mobXpMultiplier;
}

void CombatSystem::syncScenery(const std::vector<gameplay::WorldEntitySnapshot>& scenery) {
    for (const gameplay::WorldEntitySnapshot& entity : scenery) {
        if (!entity.active || !isAttackableKind(entity.kind)) {
            continue;
        }

        if (mobs_.find(entity.id) == mobs_.end()) {
            const MobCombatProfile profile =
                profileFor(entity.kind, mobHpMultiplier_, mobXpMultiplier_);
            mobs_[entity.id] = MobState{profile.maxHp, profile.maxHp, profile.xpReward};
        }
    }
}

void CombatSystem::setTarget(std::uint32_t entityId) {
    if (mobs_.find(entityId) == mobs_.end()) {
        return;
    }
    attackTargetId_ = entityId;
}

void CombatSystem::clearTarget() noexcept {
    attackTargetId_.reset();
}

void CombatSystem::reset() noexcept {
    mobs_.clear();
    attackTargetId_.reset();
}

bool CombatSystem::hasTarget() const noexcept {
    return attackTargetId_.has_value();
}

std::optional<std::uint32_t> CombatSystem::targetId() const noexcept {
    return attackTargetId_;
}

std::optional<DamageResult> CombatSystem::applyDamage(std::uint32_t entityId, int damage) {
    const auto iterator = mobs_.find(entityId);
    if (iterator == mobs_.end() || damage <= 0) {
        return std::nullopt;
    }

    iterator->second.currentHp = std::max(0, iterator->second.currentHp - damage);

    DamageResult result{};
    result.targetId = entityId;
    result.damageDealt = damage;
    result.remainingHp = iterator->second.currentHp;

    if (iterator->second.currentHp > 0) {
        return result;
    }

    result.killed = true;
    result.xpReward = iterator->second.xpReward;
    mobs_.erase(iterator);

    if (attackTargetId_ == entityId) {
        attackTargetId_.reset();
    }

    return result;
}

bool CombatSystem::isMobAlive(std::uint32_t entityId) const {
    const auto iterator = mobs_.find(entityId);
    return iterator != mobs_.end() && iterator->second.currentHp > 0;
}

std::optional<MobHealthSnapshot> CombatSystem::mobHealth(const std::uint32_t entityId) const {
    const auto iterator = mobs_.find(entityId);
    if (iterator == mobs_.end()) {
        return std::nullopt;
    }

    MobHealthSnapshot snapshot{};
    snapshot.currentHp = iterator->second.currentHp;
    snapshot.maxHp = iterator->second.maxHp;
    return snapshot;
}

std::vector<MobHealthSaveEntry> CombatSystem::collectMobHealthEntries() const {
    std::vector<MobHealthSaveEntry> entries;
    entries.reserve(mobs_.size());
    for (const auto& [entityId, state] : mobs_) {
        entries.push_back(MobHealthSaveEntry{entityId, state.currentHp, state.maxHp});
    }
    return entries;
}

void CombatSystem::restoreMobHealthEntries(const std::vector<MobHealthSaveEntry>& entries) {
    for (const MobHealthSaveEntry& entry : entries) {
        const auto iterator = mobs_.find(entry.entityId);
        if (iterator == mobs_.end()) {
            continue;
        }
        iterator->second.maxHp = std::max(1, entry.maxHp);
        iterator->second.currentHp =
            std::clamp(entry.currentHp, 0, iterator->second.maxHp);
    }
}

} // namespace game
