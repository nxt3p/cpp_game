#pragma once

#include <string>
#include <vector>

namespace ui {

struct LootLabel {
    std::string name;
    float red{0.82F};
    float green{0.82F};
    float blue{0.78F};
    int rank{0};
};

/// A pile of dropped names that share one vertical beam.
struct LootBeacon {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    std::vector<LootLabel> labels;
    float ageSeconds{0.0F};
    float lifetimeSeconds{4.8F};
    float intensity{1.0F};
};

/// Screen-space loot beams and stacked rarity nameplates. Items still go straight into the bag.
class LootPresentation {
public:
    void clear() noexcept;
    void spawn(float x, float y, float z, std::vector<LootLabel> labels, float intensity);
    void update(float deltaSeconds);

    [[nodiscard]] const std::vector<LootBeacon>& beacons() const noexcept { return beacons_; }

    /// Reference-pixel beam height before UiScale. Taller for legendary and unique piles.
    [[nodiscard]] static float beamHeight(float intensity) noexcept;

private:
    std::vector<LootBeacon> beacons_;
};

} // namespace ui
