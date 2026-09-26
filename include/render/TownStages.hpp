#pragma once

#include <cstdint>

namespace render {

/// Visual upgrade track for one town building. Repair currently shows Restored.
/// Patched and Upgraded are optional files; the loader walks backward to the
/// nearest stage that actually exists.
enum class TownArtStage : std::uint8_t {
    Ruined = 0,
    Patched = 1,
    Restored = 2,
    Upgraded = 3,
    Count = 4
};

inline constexpr int kTownArtStageCount = static_cast<int>(TownArtStage::Count);
inline constexpr int kTownArtBuildingCount = 3;

/// Building order matches systems::TownBuilding: blacksmith, tavern, chapel.
inline constexpr const char* kTownStageFiles[kTownArtBuildingCount][kTownArtStageCount] = {
    {"forge_ruined.png", "forge_patched.png", "forge_repaired.png", "forge_upgraded.png"},
    {"tavern_ruined.png", "tavern_patched.png", "tavern_repaired.png", "tavern_upgraded.png"},
    {"chapel_ruined.png", "chapel_patched.png", "chapel_repaired.png", "chapel_upgraded.png"},
};

[[nodiscard]] inline constexpr int townArtStage(const bool repaired) noexcept {
    return repaired ? static_cast<int>(TownArtStage::Restored) : static_cast<int>(TownArtStage::Ruined);
}

/// Highest stage at or below `requested` whose file was loaded.
[[nodiscard]] inline int townResolvedArtStage(const int requested, const bool present[kTownArtStageCount]) noexcept {
    int stage = requested;
    if (stage < 0) {
        stage = 0;
    }
    if (stage >= kTownArtStageCount) {
        stage = kTownArtStageCount - 1;
    }
    for (int index = stage; index >= 0; --index) {
        if (present[index]) {
            return index;
        }
    }
    return static_cast<int>(TownArtStage::Ruined);
}

[[nodiscard]] inline const char* townStageFile(const int building, const int stage) noexcept {
    if (building < 0 || building >= kTownArtBuildingCount || stage < 0 || stage >= kTownArtStageCount) {
        return "";
    }
    return kTownStageFiles[building][stage];
}

/// Ruined and restored ship with the game. The in-between stages are data.
[[nodiscard]] inline constexpr bool townStageRequired(const int stage) noexcept {
    return stage == static_cast<int>(TownArtStage::Ruined) || stage == static_cast<int>(TownArtStage::Restored);
}

} // namespace render
