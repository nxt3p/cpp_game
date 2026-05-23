#include "systems/LootEngine.hpp"

#include "systems/ItemGenerator.hpp"
#include "systems/ItemStats.hpp"

#include <algorithm>

namespace systems {

constexpr int kDrainThresholdCoins = 25;

LootEngine::LootEngine(const std::uint32_t seed)
    : rngSeed_(seed), rng_(seed), itemGenerator_(seed ^ 0x10ADBEEFU) {}

void LootEngine::registerAction(const ActionType type) {
    actionCoinPool_ += actionWeight(type);
}

void LootEngine::setSeed(const std::uint32_t seed) {
    rngSeed_ = seed;
    rng_.seed(seed);
    itemGenerator_.setSeed(seed ^ 0x10ADBEEFU);
}

void LootEngine::setCoinPool(const int coins) noexcept {
    actionCoinPool_ = std::max(0, coins);
}

void LootEngine::setZoneDepth(const int depth) noexcept {
    zoneDepth_ = std::max(1, depth);
}

void LootEngine::setLootTierBonus(const float bonus) noexcept {
    lootTierBonus_ = std::max(0.0F, bonus);
}

float LootEngine::rarityProbability(const ItemRarity rarity, const EntityTier tier) const {
    const float poolFactor = std::clamp(static_cast<float>(actionCoinPool_) / 100.0F, 0.0F, 1.0F);
    const float tierBoost = static_cast<float>(static_cast<int>(tier)) * 0.05F + lootTierBonus_;

    switch (rarity) {
    case ItemRarity::Common:
        return std::max(0.12F, 0.75F - poolFactor * 0.45F - tierBoost);
    case ItemRarity::Rare:
        return std::clamp(0.18F + poolFactor * 0.35F + tierBoost, 0.05F, 0.72F);
    case ItemRarity::Legendary:
        return std::clamp(0.02F + poolFactor * 0.25F + tierBoost * 1.5F, 0.01F, 0.38F);
    }
    return 0.0F;
}

ItemRarity LootEngine::rollRarity(const EntityTier tier) {
    std::uniform_real_distribution<float> distribution(0.0F, 1.0F);
    const float roll = distribution(rng_);

    const float legendaryChance = rarityProbability(ItemRarity::Legendary, tier);
    const float rareChance = rarityProbability(ItemRarity::Rare, tier);

    if (roll < legendaryChance) {
        return ItemRarity::Legendary;
    }
    if (roll < legendaryChance + rareChance) {
        return ItemRarity::Rare;
    }
    return ItemRarity::Common;
}

ItemMetadata LootEngine::generateItem(const ItemRarity rarity, const EntityTier tier) {
    ItemGenerationContext context{};
    context.rarity = rarity;
    context.tier = tier;
    context.zoneDepth = zoneDepth_;
    return itemGenerator_.generate(context);
}

LootDropResult LootEngine::triggerDropCheck(const EntityTier tier) {
    LootDropResult result{};
    result.coinPoolBeforeRoll = actionCoinPool_;

    if (actionCoinPool_ <= 0) {
        result.coinPoolAfterRoll = actionCoinPool_;
        return result;
    }

    const ItemRarity rarity = rollRarity(tier);
    result.resolvedRarity = rarity;
    result.item = generateItem(rarity, tier);
    result.dropped = true;

    if (rarity == ItemRarity::Rare || rarity == ItemRarity::Legendary) {
        actionCoinPool_ = 0;
        result.poolDrained = true;
    } else if (actionCoinPool_ >= kDrainThresholdCoins) {
        actionCoinPool_ = std::max(0, actionCoinPool_ - kDrainThresholdCoins);
    } else {
        actionCoinPool_ = std::max(0, actionCoinPool_ - actionWeight(ActionType::ROCK_CLICK));
    }

    result.coinPoolAfterRoll = actionCoinPool_;
    return result;
}

} // namespace systems
