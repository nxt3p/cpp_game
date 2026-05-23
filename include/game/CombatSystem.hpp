#pragma once

#include "gameplay/GameTypes.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace systems {
struct DifficultyModifiers;
}

namespace game {

struct MobCombatProfile {
    int maxHp{0};
    int xpReward{0};
};

struct DamageResult {
    std::uint32_t targetId{0};
    int damageDealt{0};
    int remainingHp{0};
    bool killed{false};
    int xpReward{0};
};

struct MobHealthSnapshot {
    int currentHp{0};
    int maxHp{0};
};

struct MobHealthSaveEntry {
    std::uint32_t entityId{0};
    int currentHp{0};
    int maxHp{0};
};

class CombatSystem {
public:
    void syncScenery(const std::vector<gameplay::WorldEntitySnapshot>& scenery);

    void setDifficultyModifiers(const systems::DifficultyModifiers& modifiers) noexcept;

    void setTarget(std::uint32_t entityId);
    void clearTarget() noexcept;
    void reset() noexcept;
    [[nodiscard]] bool hasTarget() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> targetId() const noexcept;

    [[nodiscard]] std::optional<DamageResult> applyDamage(std::uint32_t entityId, int damage);
    [[nodiscard]] bool isMobAlive(std::uint32_t entityId) const;
    [[nodiscard]] std::optional<MobHealthSnapshot> mobHealth(std::uint32_t entityId) const;
    [[nodiscard]] std::vector<MobHealthSaveEntry> collectMobHealthEntries() const;
    void restoreMobHealthEntries(const std::vector<MobHealthSaveEntry>& entries);

private:
    struct MobState {
        int currentHp{0};
        int maxHp{0};
        int xpReward{0};
    };

    [[nodiscard]] static MobCombatProfile profileFor(
        gameplay::EntityKind kind,
        float hpMultiplier,
        float xpMultiplier) noexcept;

    float mobHpMultiplier_{1.0F};
    float mobXpMultiplier_{1.0F};

    std::unordered_map<std::uint32_t, MobState> mobs_{};
    std::optional<std::uint32_t> attackTargetId_{};
};

} // namespace game
