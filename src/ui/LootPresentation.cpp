#include "ui/LootPresentation.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace ui {

void LootPresentation::clear() noexcept {
    beacons_.clear();
}

float LootPresentation::beamHeight(const float intensity) noexcept {
    return 128.0F + std::clamp(intensity, 0.35F, 3.0F) * 96.0F;
}

void LootPresentation::spawn(
    const float x,
    const float y,
    const float z,
    std::vector<LootLabel> labels,
    const float intensity) {
    if (labels.empty()) {
        return;
    }

    std::stable_sort(labels.begin(), labels.end(), [](const LootLabel& a, const LootLabel& b) {
        return a.rank > b.rank;
    });

    LootBeacon beacon{};
    beacon.x = x;
    beacon.y = y;
    beacon.z = z;
    beacon.labels = std::move(labels);
    beacon.intensity = std::clamp(intensity, 0.35F, 3.0F);
    beacon.lifetimeSeconds = beacon.intensity >= 1.6F ? 6.2F : 4.6F;
    beacons_.push_back(std::move(beacon));

    constexpr std::size_t kMaxBeacons = 12;
    if (beacons_.size() > kMaxBeacons) {
        beacons_.erase(beacons_.begin(), beacons_.end() - static_cast<std::ptrdiff_t>(kMaxBeacons));
    }
}

void LootPresentation::update(const float deltaSeconds) {
    if (deltaSeconds <= 0.0F) {
        return;
    }

    for (LootBeacon& beacon : beacons_) {
        beacon.ageSeconds += deltaSeconds;
    }
    beacons_.erase(
        std::remove_if(
            beacons_.begin(),
            beacons_.end(),
            [](const LootBeacon& beacon) { return beacon.ageSeconds >= beacon.lifetimeSeconds; }),
        beacons_.end());
}

} // namespace ui
