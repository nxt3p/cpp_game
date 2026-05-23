#pragma once

#include <cstdint>
#include <string>

namespace gameplay {

enum class GameState : std::uint8_t {
    TOWN,
    PLAINS,
    TRADING,
    CHARACTER_MENU
};

enum class WorldZone : std::uint8_t {
    TOWN,
    PLAINS
};

enum class EntityKind : std::uint8_t {
    PLAYER,
    ENEMY_MOB,
    ENEMY_BOSS,
    NPC_BLACKSMITH,
    ENV_TREE,
    ENV_BUSH,
    ENV_ROCK,
    ENV_CHEST,
    ENV_HOUSE,
    ENV_MUSHROOM
};

struct Vec3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct AxisAlignedBounds {
    float minX{0.0F};
    float maxX{0.0F};
    float minZ{0.0F};
    float maxZ{0.0F};

    [[nodiscard]] bool contains(float x, float z) const noexcept;
};

struct WorldEntitySnapshot {
    std::uint32_t id{0};
    EntityKind kind{EntityKind::ENV_TREE};
    Vec3 position{};
    bool active{true};
    /// Index into the sliced prop catalog for this entity kind (wraps at runtime).
    std::uint8_t variant{0};
};

} // namespace gameplay
