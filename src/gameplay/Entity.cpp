#include "gameplay/Entity.hpp"

#include <algorithm>

namespace gameplay {

Entity::Entity(std::uint32_t id, EntityKind kind, Vec3 position)
    : id_(id), kind_(kind), position_(position) {}

void Entity::setPosition(const Vec3& position) noexcept {
    position_ = position;
}

void Entity::setActive(bool active) noexcept {
    active_ = active;
}

WorldEntitySnapshot Entity::snapshot() const {
    return WorldEntitySnapshot{id_, kind_, position_, active_};
}

PlayerEntity::PlayerEntity(std::uint32_t id, Vec3 position)
    : Entity(id, EntityKind::PLAYER, position) {}

void PlayerEntity::addGold(int amount) {
    if (amount > 0) {
        gold_ += amount;
    }
}

void PlayerEntity::setGold(const int amount) noexcept {
    gold_ = std::max(0, amount);
}

bool PlayerEntity::spendGold(int amount) {
    if (amount < 0 || gold_ < amount) {
        return false;
    }
    gold_ -= amount;
    return true;
}

void PlayerEntity::setAttacksEnabled(bool enabled) noexcept {
    attacksEnabled_ = enabled;
}

NpcBlacksmithEntity::NpcBlacksmithEntity(Vec3 position)
    : Entity(2U, EntityKind::NPC_BLACKSMITH, position) {}

} // namespace gameplay
