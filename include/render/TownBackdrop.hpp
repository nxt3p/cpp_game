#pragma once

#include <cstdint>
#include <vector>

namespace render {

struct TownPixelBuffer {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba{};
};

/// Original dusk-town illustration. Buildings are painted ruined; the UI tints them once repaired.
[[nodiscard]] TownPixelBuffer paintTownBackdrop(int width, int height);

} // namespace render
