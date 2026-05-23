#include "gameplay/ProceduralZoneGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {

struct RngState {
    std::uint32_t value{1U};

    explicit RngState(const std::uint32_t seed) : value(seed != 0U ? seed : 1U) {}

    float nextUnitFloat() {
        value = value * 1664525U + 1013904223U;
        return static_cast<float>(value & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
    }

    float nextRange(const float minValue, const float maxValue) {
        return minValue + (maxValue - minValue) * nextUnitFloat();
    }

    int nextInt(const int minValue, const int maxValueInclusive) {
        const float roll = nextUnitFloat();
        return minValue + static_cast<int>(roll * static_cast<float>(maxValueInclusive - minValue + 1));
    }
};

bool isFarEnough(const Vec3& candidate, const std::vector<Vec3>& existing, const float minDistance) {
    const float minDistanceSq = minDistance * minDistance;
    for (const Vec3& point : existing) {
        const float dx = candidate.x - point.x;
        const float dz = candidate.z - point.z;
        if ((dx * dx + dz * dz) < minDistanceSq) {
            return false;
        }
    }
    return true;
}

Vec3 samplePoint(RngState& rng, const AxisAlignedBounds& bounds, const float margin) {
    return Vec3{
        rng.nextRange(bounds.minX + margin, bounds.maxX - margin),
        0.0F,
        rng.nextRange(bounds.minZ + margin, bounds.maxZ - margin)};
}

void appendScatterProps(
    GeneratedZoneLayout& layout,
    RngState& rng,
    const EntityKind kind,
    const int count,
    const AxisAlignedBounds& bounds,
    const float margin) {
    for (int index = 0; index < count; ++index) {
        const Vec3 point = samplePoint(rng, bounds, margin);
        layout.props.push_back(
            PropSpawn{kind, point.x, point.z, static_cast<std::uint8_t>(index % 8)});
    }
}

} // namespace

GeneratedZoneLayout generatePlainsLayout(const ZoneLayoutSpec& spec) noexcept {
    GeneratedZoneLayout layout{};
    RngState rng(spec.seed ^ static_cast<std::uint32_t>(spec.depth * 7919));

    const AxisAlignedBounds& bounds = spec.bounds;
    const float margin = 8.0F;

    const int treeCount = 6 + spec.depth;
    const int bushCount = 8 + spec.depth / 2;
    const int rockCount = 8 + spec.depth;
    const int mushroomCount = 4 + spec.depth / 3;
    const int houseCount = std::min(3, 1 + spec.depth / 3);
    const int chestCount = 1 + spec.depth / 2;

    appendScatterProps(layout, rng, EntityKind::ENV_TREE, treeCount, bounds, margin);
    appendScatterProps(layout, rng, EntityKind::ENV_BUSH, bushCount, bounds, margin);
    appendScatterProps(layout, rng, EntityKind::ENV_ROCK, rockCount, bounds, margin);
    appendScatterProps(layout, rng, EntityKind::ENV_MUSHROOM, mushroomCount, bounds, margin);
    appendScatterProps(layout, rng, EntityKind::ENV_HOUSE, houseCount, bounds, margin + 6.0F);

    for (int index = 0; index < chestCount; ++index) {
        const Vec3 point = samplePoint(rng, bounds, margin);
        layout.props.push_back(PropSpawn{EntityKind::ENV_CHEST, point.x, point.z, 0});
    }

    const int mobBudget = std::min(12, 4 + spec.depth);
    const float mobSpacing = 14.0F;
    for (int attempt = 0; attempt < mobBudget * 6 && static_cast<int>(layout.mobSpawns.size()) < mobBudget;
         ++attempt) {
        const Vec3 candidate = samplePoint(rng, bounds, margin + 4.0F);
        if (isFarEnough(candidate, layout.mobSpawns, mobSpacing)) {
            layout.mobSpawns.push_back(candidate);
        }
    }

    for (int attempt = 0; attempt < 32; ++attempt) {
        const Vec3 candidate = samplePoint(rng, bounds, margin + 10.0F);
        if (isFarEnough(candidate, layout.mobSpawns, mobSpacing * 1.5F)) {
            layout.bossSpawn = candidate;
            break;
        }
    }

    if (layout.bossSpawn.x == 0.0F && layout.bossSpawn.z == 0.0F) {
        layout.bossSpawn = Vec3{
            rng.nextRange(bounds.minX + 20.0F, bounds.maxX - 20.0F),
            0.0F,
            rng.nextRange(bounds.minZ + 40.0F, bounds.maxZ - 10.0F)};
    }

    return layout;
}

} // namespace gameplay
