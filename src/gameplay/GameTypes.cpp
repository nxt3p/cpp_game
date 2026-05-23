#include "gameplay/GameTypes.hpp"

namespace gameplay {

bool AxisAlignedBounds::contains(float x, float z) const noexcept {
    return x >= minX && x <= maxX && z >= minZ && z <= maxZ;
}

} // namespace gameplay
