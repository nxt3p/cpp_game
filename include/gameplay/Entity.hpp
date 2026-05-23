#pragma once

#include "gameplay/GameTypes.hpp"

namespace gameplay {

class Entity {
public:
    Entity(std::uint32_t id, EntityKind kind, Vec3 position);
    virtual ~Entity() = default;

    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }
    [[nodiscard]] EntityKind kind() const noexcept { return kind_; }
    [[nodiscard]] const Vec3& position() const noexcept { return position_; }
    [[nodiscard]] bool isActive() const noexcept { return active_; }

    void setPosition(const Vec3& position) noexcept;
    void setActive(bool active) noexcept;

    [[nodiscard]] WorldEntitySnapshot snapshot() const;

protected:
    std::uint32_t id_{0};
    EntityKind kind_{EntityKind::ENEMY_MOB};
    Vec3 position_{};
    bool active_{true};
};

class PlayerEntity final : public Entity {
public:
    PlayerEntity(std::uint32_t id, Vec3 position);

    [[nodiscard]] int gold() const noexcept { return gold_; }
    void setGold(int amount) noexcept;
    void addGold(int amount);
    bool spendGold(int amount);

    [[nodiscard]] bool attacksEnabled() const noexcept { return attacksEnabled_; }
    void setAttacksEnabled(bool enabled) noexcept;

private:
    int gold_{100};
    bool attacksEnabled_{true};
};

class NpcBlacksmithEntity final : public Entity {
public:
    explicit NpcBlacksmithEntity(Vec3 position);

    [[nodiscard]] float interactionRadius() const noexcept { return interactionRadius_; }

private:
    float interactionRadius_{3.0F};
};

} // namespace gameplay
