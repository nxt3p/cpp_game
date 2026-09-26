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

CombatRarityWeights LootEngine::combatRarityWeights(const EntityTier tier) const noexcept {
    CombatRarityWeights weights{};
    switch (tier) {
    case EntityTier::Minor:
        weights.dropChance = 0.16F;
        weights.common = 0.84F;
        weights.magic = 0.11F;
        weights.rare = 0.038F;
        weights.legendary = 0.010F;
        weights.unique = 0.002F;
        break;
    case EntityTier::Elite:
        weights.dropChance = 0.34F;
        weights.common = 0.58F;
        weights.magic = 0.22F;
        weights.rare = 0.12F;
        weights.legendary = 0.055F;
        weights.unique = 0.025F;
        break;
    case EntityTier::Boss:
        weights.dropChance = 0.55F;
        weights.common = 0.42F;
        weights.magic = 0.26F;
        weights.rare = 0.18F;
        weights.legendary = 0.10F;
        weights.unique = 0.04F;
        break;
    case EntityTier::Standard:
        weights.dropChance = 0.24F;
        weights.common = 0.72F;
        weights.magic = 0.17F;
        weights.rare = 0.075F;
        weights.legendary = 0.025F;
        weights.unique = 0.010F;
        break;
    }
    weights.mythical = 0.0F;
    const float tierBoost = static_cast<float>(static_cast<int>(tier)) * 0.02F + lootTierBonus_ * 0.5F;
    weights.dropChance = std::clamp(weights.dropChance + tierBoost, 0.05F, 0.7F);
    return weights;
}

float LootEngine::rarityProbability(const ItemRarity rarity, const EntityTier tier) const {
    const CombatRarityWeights weights = combatRarityWeights(tier);
    switch (rarity) {
    case ItemRarity::Common:
        return weights.common;
    case ItemRarity::Magic:
        return weights.magic;
    case ItemRarity::Rare:
        return weights.rare;
    case ItemRarity::Legendary:
        return weights.legendary;
    case ItemRarity::Unique:
        return weights.unique;
    case ItemRarity::Mythical:
        return weights.mythical;
    }
    return 0.0F;
}

ItemRarity LootEngine::rollRarity(const EntityTier tier) {
    std::uniform_real_distribution<float> distribution(0.0F, 1.0F);
    const float roll = distribution(rng_);
    const CombatRarityWeights weights = combatRarityWeights(tier);

    float cursor = weights.mythical;
    if (roll < cursor) {
        return ItemRarity::Mythical;
    }
    cursor += weights.unique;
    if (roll < cursor) {
        return ItemRarity::Unique;
    }
    cursor += weights.legendary;
    if (roll < cursor) {
        return ItemRarity::Legendary;
    }
    cursor += weights.rare;
    if (roll < cursor) {
        return ItemRarity::Rare;
    }
    cursor += weights.magic;
    if (roll < cursor) {
        return ItemRarity::Magic;
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

    const CombatRarityWeights weights = combatRarityWeights(tier);
    std::uniform_real_distribution<float> dropRoll(0.0F, 1.0F);
    if (dropRoll(rng_) > weights.dropChance) {
        actionCoinPool_ = std::max(0, actionCoinPool_ - actionWeight(ActionType::ROCK_CLICK));
        result.coinPoolAfterRoll = actionCoinPool_;
        result.resolvedRarity = ItemRarity::Common;
        return result;
    }

    const ItemRarity rarity = rollRarity(tier);
    result.resolvedRarity = rarity;
    result.item = generateItem(rarity, tier);
    result.dropped = true;

    if (rarity != ItemRarity::Common) {
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
