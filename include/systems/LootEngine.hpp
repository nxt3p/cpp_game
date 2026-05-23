#pragma once

#include "systems/ItemGenerator.hpp"
#include "systems/ItemTypes.hpp"

#include <cstdint>
#include <random>

namespace systems {

struct LootDropResult {
    bool dropped{false};
    ItemMetadata item{};
    ItemRarity resolvedRarity{ItemRarity::Common};
    int coinPoolBeforeRoll{0};
    int coinPoolAfterRoll{0};
    bool poolDrained{false};
};

class LootEngine {
public:
    explicit LootEngine(std::uint32_t seed = 0xC0FFEE42U);

    void registerAction(ActionType type);
    [[nodiscard]] int coinPool() const noexcept { return actionCoinPool_; }
    [[nodiscard]] std::uint32_t rngSeed() const noexcept { return rngSeed_; }

    void setZoneDepth(int depth) noexcept;
    void setLootTierBonus(float bonus) noexcept;
    void setCoinPool(int coins) noexcept;

    LootDropResult triggerDropCheck(EntityTier tier);
    void setSeed(std::uint32_t seed);

private:
    [[nodiscard]] float rarityProbability(ItemRarity rarity, EntityTier tier) const;
    [[nodiscard]] ItemMetadata generateItem(ItemRarity rarity, EntityTier tier);
    [[nodiscard]] ItemRarity rollRarity(EntityTier tier);

    int actionCoinPool_{0};
    int zoneDepth_{1};
    float lootTierBonus_{0.0F};
    std::uint32_t rngSeed_{0xC0FFEE42U};
    std::mt19937 rng_;
    ItemGenerator itemGenerator_;
};

} // namespace systems
