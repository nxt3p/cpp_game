#pragma once

#include "systems/ItemTypes.hpp"

#include <random>

namespace systems {

struct ItemGenerationContext {
    ItemRarity rarity{ItemRarity::Common};
    EntityTier tier{EntityTier::Standard};
    int zoneDepth{1};
};

class ItemGenerator {
public:
    explicit ItemGenerator(std::uint32_t seed = 0xAFF1A000U);

    [[nodiscard]] ItemMetadata generate(const ItemGenerationContext& context);
    void rerollBonuses(ItemMetadata& item);
    void setSeed(std::uint32_t seed);

private:
    [[nodiscard]] ItemCategory rollCategory();
    [[nodiscard]] ItemStatBonuses rollBonuses(ItemCategory category, ItemRarity rarity, int itemLevel);
    [[nodiscard]] std::string buildName(
        ItemCategory category,
        ItemRarity rarity,
        const std::string& prefix,
        const std::string& suffix);
    [[nodiscard]] char iconForCategory(ItemCategory category) const noexcept;

    std::mt19937 rng_;
};

} // namespace systems
