#pragma once

#include <cstdint>
#include <string>

namespace systems {

enum class TownBuilding : std::uint8_t {
    Blacksmith = 0,
    Tavern,
    Healer,
    Count
};

enum class TownRepairResult : std::uint8_t {
    Repaired,
    AlreadyOpen,
    NeedLevel,
    NeedGold
};

struct TownBuildingDefinition {
    const char* name{""};
    const char* ruinedName{""};
    const char* serviceHint{""};
    int repairGold{0};
    int requiredLevel{1};
};

[[nodiscard]] TownBuildingDefinition townBuildingDefinition(TownBuilding building) noexcept;

/// Gold paid out for a kill so early town repairs are funded by fighting, not by item drops.
[[nodiscard]] int combatGoldBounty(bool boss, bool elite, int depth) noexcept;

[[nodiscard]] int healerTitheGold() noexcept;

class TownHub {
public:
    [[nodiscard]] bool isRepaired(TownBuilding building) const noexcept;
    [[nodiscard]] int repairMask() const noexcept { return repairMask_; }
    void applyRepairMask(int mask) noexcept;

    [[nodiscard]] TownRepairResult tryRepair(TownBuilding building, int& gold, int level) noexcept;

    [[nodiscard]] static std::string repairMessage(TownBuilding building, TownRepairResult result) ;

private:
    int repairMask_{0};
};

} // namespace systems
