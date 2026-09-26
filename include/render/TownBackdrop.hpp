#pragma once

#include <cstdint>
#include <vector>

namespace render {

struct TownPixelBuffer {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba{};
};

/// Which painted plate sits on a town hotspot.
enum class TownPlateKind : std::uint8_t {
    Forge = 0,
    Chapel,
    Tavern,
    Road
};

/// Original dusk-town illustration. Buildings are separate plates so ruins and repairs read apart.
[[nodiscard]] TownPixelBuffer paintTownBackdrop(int width, int height);

/// Building or road plate. `restored` is ignored for the road, which is always open.
[[nodiscard]] TownPixelBuffer paintTownPlate(TownPlateKind kind, bool restored, int width, int height);

} // namespace render
