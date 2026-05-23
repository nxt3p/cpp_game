#pragma once

#include "gameplay/GameTypes.hpp"

#include <cstdint>
#include <vector>

namespace gameplay {

struct PropSpawn {
    EntityKind kind;
    float x;
    float z;
    std::uint8_t variant;
};

struct ZoneLayoutSpec {
    std::uint32_t seed{1U};
    int depth{1};
    AxisAlignedBounds bounds{};
};

struct GeneratedZoneLayout {
    std::vector<PropSpawn> props;
    std::vector<Vec3> mobSpawns;
    Vec3 bossSpawn{};
};

[[nodiscard]] GeneratedZoneLayout generatePlainsLayout(const ZoneLayoutSpec& spec) noexcept;

} // namespace gameplay
