#include "systems/SlotMachineLoot.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace systems {

namespace {

// Base reel weights per source tier. Each row sums to 1.0 before coin/pity adjustments.
// Boss jackpots are occasional (~16% before the unique slice). Elites are rarer. Trash mobs almost never jackpot.
constexpr std::array<LootReelOdds, 4> kBaseOdds = {{
    /* Minor    */ {0.78F, 0.17F, 0.049F, 0.001F},
    /* Standard */ {0.52F, 0.33F, 0.142F, 0.008F},
    /* Elite    */ {0.30F, 0.42F, 0.252F, 0.028F},
    /* Boss     */ {0.08F, 0.32F, 0.44F, 0.16F},
}};

constexpr float kCoinPoolSaturation = 150.0F;
constexpr int kPityGraceSpins = 30;
constexpr float kPityJackpotPerDrySpin = 0.0025F;
constexpr int kCommonDrainCoins = 5;
constexpr int kMediumDrainCoins = 25;

constexpr const char* kConsumableNames[] = {"Healing Potion", "Bitter Tonic", "Red Draught", "Vial of Embers"};
constexpr const char* kJunkNames[] = {"Cracked Skull", "Rusty Nail", "Torn Banner", "Bent Spoon", "Moth-eaten Boot"};
constexpr const char* kMaterialNames[] = {"Soul Shard", "Ember Ore", "Wyrm Scale", "Runed Bone", "Void Silk"};

struct UniqueTemplate {
    const char* name;
    ItemCategory category;
    char icon;
    ItemStatBonuses bonuses;
};

constexpr UniqueTemplate kUniques[] = {
    {"Ashen Kingsblade", ItemCategory::Weapon, 'W', {8, 2, 0, 0, 0.12F, 14, 3.0F}},
    {"Crown of the Hollow Sun", ItemCategory::Head, 'H', {3, 3, 6, 60, 0.0F, 0, 12.0F}},
    {"Voidwalker Treads", ItemCategory::Feet, 'F', {0, 9, 2, 20, 0.22F, 0, 2.0F}},
    {"Heart of the Deep", ItemCategory::Amulet, 'A', {4, 4, 4, 45, 0.08F, 4, 6.0F}},
    {"Gravebinder Signet", ItemCategory::Ring, 'R', {6, 0, 0, 0, 0.0F, 8, 0.0F}},
    {"Mantle of Last Light", ItemCategory::Cloak, 'K', {0, 2, 8, 80, 0.0F, 0, 10.0F}},
};

} // namespace

const char* lootReelTierLabel(const LootReelTier tier) noexcept {
    switch (tier) {
    case LootReelTier::Nothing:
        return "Nothing";
    case LootReelTier::Common:
        return "Common";
    case LootReelTier::Medium:
        return "Medium";
    case LootReelTier::Jackpot:
        return "Jackpot";
    }
    return "Nothing";
}

const char* lootPrizeKindLabel(const LootPrizeKind kind) noexcept {
    switch (kind) {
    case LootPrizeKind::Gold:
        return "Gold";
    case LootPrizeKind::Consumable:
        return "Consumable";
    case LootPrizeKind::Junk:
        return "Junk";
    case LootPrizeKind::Material:
        return "Material";
    case LootPrizeKind::SocketedGear:
        return "Socketed Gear";
    case LootPrizeKind::Legendary:
        return "Legendary";
    case LootPrizeKind::Unique:
        return "Unique";
    }
    return "Gold";
}

SlotMachineLoot::SlotMachineLoot(const std::uint32_t seed)
    : seed_(seed), rng_(seed), itemGenerator_(seed ^ 0x7E11A5EDU) {}

void SlotMachineLoot::setSeed(const std::uint32_t seed) {
    seed_ = seed;
    rng_.seed(seed);
    itemGenerator_.setSeed(seed ^ 0x7E11A5EDU);
}

void SlotMachineLoot::insertCoins(const ActionType type) noexcept {
    const int coins = actionWeight(type);
    coinPool_ += coins;
    telemetry_.coinsInserted += coins;
}

void SlotMachineLoot::setCoinPool(const int coins) noexcept {
    coinPool_ = std::max(0, coins);
}

void SlotMachineLoot::setZoneDepth(const int depth) noexcept {
    zoneDepth_ = std::max(1, depth);
}

void SlotMachineLoot::setLootTierBonus(const float bonus) noexcept {
    lootTierBonus_ = std::max(0.0F, bonus);
}

void SlotMachineLoot::setLootCeiling(const LootCeiling ceiling) noexcept {
    lootCeiling_ = ceiling;
}

void SlotMachineLoot::setPityCounter(const int counter) noexcept {
    pityCounter_ = std::max(0, counter);
}

float SlotMachineLoot::unit() {
    std::uniform_real_distribution<float> distribution(0.0F, 1.0F);
    return distribution(rng_);
}

int SlotMachineLoot::rollInt(const int minInclusive, const int maxInclusive) {
    if (maxInclusive <= minInclusive) {
        return minInclusive;
    }
    std::uniform_int_distribution<int> distribution(minInclusive, maxInclusive);
    return distribution(rng_);
}

LootReelOdds SlotMachineLoot::oddsFor(const EntityTier tier) const noexcept {
    const std::size_t index = std::min<std::size_t>(static_cast<std::size_t>(tier), kBaseOdds.size() - 1);
    LootReelOdds odds = kBaseOdds[index];

    // Coins shift mass from "nothing" into common/medium and nudge the jackpot.
    const float poolFactor = std::clamp(static_cast<float>(coinPool_) / kCoinPoolSaturation, 0.0F, 1.0F);
    const float shift = std::min(odds.nothing, poolFactor * 0.25F);
    odds.nothing -= shift;
    odds.common += shift * 0.55F;
    odds.medium += shift * 0.45F;

    // Pity only starts helping after a grace window so the base rate stays legible,
    // then ramps hard enough that the cap is rarely reached.
    const int pityOverflow = std::max(0, pityCounter_ - kPityGraceSpins);
    const float pityScale = tier == EntityTier::Minor ? 0.35F : 1.0F;
    odds.jackpot += poolFactor * 0.01F + lootTierBonus_ +
                    static_cast<float>(pityOverflow) * kPityJackpotPerDrySpin * pityScale;
    if (pityCounter_ >= kPityHardCap) {
        odds.jackpot = 1.0F;
    }
    odds.jackpot = std::clamp(odds.jackpot, 0.0F, 1.0F);

    // Renormalise the non-jackpot mass so the four reels always sum to one.
    const float remainder = 1.0F - odds.jackpot;
    const float nonJackpot = odds.nothing + odds.common + odds.medium;
    if (nonJackpot > 0.0F) {
        const float scale = remainder / nonJackpot;
        odds.nothing *= scale;
        odds.common *= scale;
        odds.medium *= scale;
    } else {
        odds.nothing = remainder;
    }

    if (lootCeiling_ == LootCeiling::Magic) {
        odds.medium += odds.jackpot;
        odds.jackpot = 0.0F;
    }
    return odds;
}

LootReelTier SlotMachineLoot::rollReel(const LootReelOdds& odds) {
    const float roll = unit();
    if (roll < odds.jackpot) {
        return LootReelTier::Jackpot;
    }
    if (roll < odds.jackpot + odds.medium) {
        return LootReelTier::Medium;
    }
    if (roll < odds.jackpot + odds.medium + odds.common) {
        return LootReelTier::Common;
    }
    return LootReelTier::Nothing;
}

ItemMetadata SlotMachineLoot::makeConsumable() {
    ItemMetadata item{};
    item.itemId = 3000U + static_cast<std::uint32_t>(rollInt(0, static_cast<int>(std::size(kConsumableNames)) - 1));
    item.name = kConsumableNames[item.itemId - 3000U];
    item.rarity = ItemRarity::Common;
    item.category = ItemCategory::Consumable;
    item.iconLetter = 'P';
    item.value = 8 + zoneDepth_ * 2;
    item.itemLevel = zoneDepth_;
    item.bonuses = {0, 0, 0, 25, 0.0F, 0, 0.0F};
    return item;
}

ItemMetadata SlotMachineLoot::makeJunk() {
    ItemMetadata item{};
    const int index = rollInt(0, static_cast<int>(std::size(kJunkNames)) - 1);
    item.itemId = 3100U + static_cast<std::uint32_t>(index);
    item.name = kJunkNames[index];
    item.rarity = ItemRarity::Common;
    item.category = ItemCategory::Misc;
    item.iconLetter = 'J';
    item.value = 1 + rollInt(0, 3);
    item.itemLevel = 1;
    return item;
}

ItemMetadata SlotMachineLoot::makeMaterial(const EntityTier tier) {
    ItemMetadata item{};
    const int index = rollInt(0, static_cast<int>(std::size(kMaterialNames)) - 1);
    item.itemId = 3200U + static_cast<std::uint32_t>(index);
    item.name = kMaterialNames[index];
    item.rarity = ItemRarity::Magic;
    item.category = ItemCategory::Material;
    item.iconLetter = 'M';
    item.value = (30 + zoneDepth_ * 6) * (1 + static_cast<int>(tier));
    item.itemLevel = zoneDepth_;
    return item;
}

ItemMetadata SlotMachineLoot::makeSocketedGear(const EntityTier tier) {
    ItemGenerationContext context{};
    context.rarity = ItemRarity::Magic;
    context.tier = tier;
    context.zoneDepth = zoneDepth_;
    ItemMetadata item = itemGenerator_.generate(context);
    item.sockets = rollInt(1, tier >= EntityTier::Elite ? 3 : 2);
    item.name = "Socketed " + item.name;
    item.bonuses.damage += std::max(2, zoneDepth_);
    item.bonuses.maxHealth += 8 + zoneDepth_ * 4;
    item.value += item.sockets * 20 + item.bonuses.damage * 6;
    return item;
}

ItemMetadata SlotMachineLoot::makeLegendary(const EntityTier tier) {
    ItemGenerationContext context{};
    context.rarity = ItemRarity::Legendary;
    context.tier = tier;
    context.zoneDepth = zoneDepth_;
    return itemGenerator_.generate(context);
}

ItemMetadata SlotMachineLoot::makeUnique(const EntityTier tier) {
    const int index = rollInt(0, static_cast<int>(std::size(kUniques)) - 1);
    const UniqueTemplate& blueprint = kUniques[index];
    ItemMetadata item{};
    item.itemId = 9000U + static_cast<std::uint32_t>(index);
    item.name = blueprint.name;
    item.rarity = ItemRarity::Unique;
    item.category = blueprint.category;
    item.iconLetter = blueprint.icon;
    item.itemLevel = zoneDepth_ + static_cast<int>(tier);
    const int scale = std::max(1, item.itemLevel / 2);
    item.bonuses = blueprint.bonuses;
    item.bonuses.strength += item.bonuses.strength > 0 ? scale : 0;
    item.bonuses.dexterity += item.bonuses.dexterity > 0 ? scale : 0;
    item.bonuses.vitality += item.bonuses.vitality > 0 ? scale : 0;
    item.bonuses.damage += item.bonuses.damage > 0 ? scale * 2 : 0;
    item.bonuses.maxHealth += item.bonuses.maxHealth > 0 ? scale * 10 : 0;
    item.value = 400 + item.itemLevel * 60;
    return item;
}

LootPrize SlotMachineLoot::rollCommonPrize(const EntityTier tier) {
    LootPrize prize{};
    prize.tier = LootReelTier::Common;
    const float roll = unit();
    if (roll < 0.74F) {
        prize.kind = LootPrizeKind::Gold;
        prize.goldAmount = rollInt(4, 12 + zoneDepth_ * 4) * (1 + static_cast<int>(tier));
    } else if (roll < 0.90F) {
        prize.kind = LootPrizeKind::Consumable;
        prize.item = makeConsumable();
    } else {
        prize.kind = LootPrizeKind::Junk;
        prize.item = makeJunk();
    }
    return prize;
}

LootPrize SlotMachineLoot::rollMediumPrize(const EntityTier tier) {
    LootPrize prize{};
    prize.tier = LootReelTier::Medium;
    const float roll = unit();
    if (roll < 0.22F) {
        prize.kind = LootPrizeKind::Material;
        prize.item = makeMaterial(tier);
    } else if (roll < 0.78F) {
        prize.kind = LootPrizeKind::SocketedGear;
        prize.item = makeSocketedGear(tier);
    } else {
        prize.kind = LootPrizeKind::SocketedGear;
        prize.item = makeSocketedGear(tier);
        if (prize.item.has_value()) {
            prize.item->rarity = ItemRarity::Rare;
            prize.item->name = std::string("Rare ") + prize.item->name;
            prize.item->value += 40;
        }
    }
    return prize;
}

float SlotMachineLoot::jackpotUniqueSlice(const EntityTier tier) const noexcept {
    if (lootCeiling_ != LootCeiling::Unique) {
        return 0.0F;
    }
    return tier == EntityTier::Boss ? 0.12F : 0.06F;
}

float SlotMachineLoot::legendaryChance(const EntityTier tier) const noexcept {
    return oddsFor(tier).jackpot * (1.0F - jackpotUniqueSlice(tier));
}

void SlotMachineLoot::setTavernPitySpins(const int drySpins) noexcept {
    tavernDrySpins_ = std::max(0, drySpins);
}

TavernPrize tavernPrizeForRoll(const float roll, const TavernGambleOdds& odds) noexcept {
    float cursor = 0.0F;
    const auto hit = [&](const float chance) {
        cursor += chance;
        return roll < cursor;
    };
    if (hit(odds.mythical)) {
        return TavernPrize::Mythical;
    }
    if (hit(odds.unique)) {
        return TavernPrize::Unique;
    }
    if (hit(odds.legendary)) {
        return TavernPrize::Legendary;
    }
    if (hit(odds.rare)) {
        return TavernPrize::Rare;
    }
    if (hit(odds.magic)) {
        return TavernPrize::Magic;
    }
    if (hit(odds.common)) {
        return TavernPrize::Common;
    }
    if (hit(odds.gold)) {
        return TavernPrize::Gold;
    }
    return TavernPrize::Nothing;
}

LootPrize SlotMachineLoot::rollJackpotPrize(const EntityTier tier) {
    LootPrize prize{};
    prize.tier = LootReelTier::Jackpot;
    if (unit() < jackpotUniqueSlice(tier)) {
        prize.kind = LootPrizeKind::Unique;
        prize.item = makeUnique(tier);
    } else {
        prize.kind = LootPrizeKind::Legendary;
        prize.item = makeLegendary(tier);
    }
    return prize;
}

void SlotMachineLoot::drainCoins(const LootReelTier tier) noexcept {
    switch (tier) {
    case LootReelTier::Nothing:
        coinPool_ = std::max(0, coinPool_ - actionWeight(ActionType::ROCK_CLICK));
        break;
    case LootReelTier::Common:
        coinPool_ = std::max(0, coinPool_ - kCommonDrainCoins);
        break;
    case LootReelTier::Medium:
        coinPool_ = std::max(0, coinPool_ - kMediumDrainCoins);
        break;
    case LootReelTier::Jackpot:
        coinPool_ = 0;
        break;
    }
}

void SlotMachineLoot::record(const LootSpinResult& result) noexcept {
    ++telemetry_.spins;
    switch (result.tier) {
    case LootReelTier::Nothing:
        ++telemetry_.nothing;
        break;
    case LootReelTier::Common:
        ++telemetry_.common;
        break;
    case LootReelTier::Medium:
        ++telemetry_.medium;
        break;
    case LootReelTier::Jackpot:
        ++telemetry_.jackpot;
        break;
    }
    for (const LootPrize& prize : result.prizes) {
        switch (prize.kind) {
        case LootPrizeKind::Gold:
            telemetry_.goldTotal += prize.goldAmount;
            break;
        case LootPrizeKind::Consumable:
            ++telemetry_.consumables;
            break;
        case LootPrizeKind::Junk:
            ++telemetry_.junk;
            break;
        case LootPrizeKind::Material:
            ++telemetry_.materials;
            break;
        case LootPrizeKind::SocketedGear:
            ++telemetry_.socketedItems;
            break;
        case LootPrizeKind::Legendary:
            ++telemetry_.legendaryItems;
            break;
        case LootPrizeKind::Unique:
            ++telemetry_.uniqueItems;
            break;
        }
    }
}

LootSpinResult SlotMachineLoot::spin(const EntityTier tier) {
    LootSpinResult result{};
    result.coinPoolBefore = coinPool_;
    if (coinPool_ <= 0) {
        result.coinPoolAfter = coinPool_;
        result.pityCounterAfter = pityCounter_;
        return result;
    }

    result.spun = true;
    result.tier = rollReel(oddsFor(tier));

    switch (result.tier) {
    case LootReelTier::Nothing:
        break;
    case LootReelTier::Common:
        result.prizes.push_back(rollCommonPrize(tier));
        break;
    case LootReelTier::Medium:
        result.prizes.push_back(rollMediumPrize(tier));
        if (unit() < 0.35F) {
            result.prizes.push_back(rollCommonPrize(tier));
        }
        break;
    case LootReelTier::Jackpot: {
        result.jackpot = true;
        result.prizes.push_back(rollJackpotPrize(tier));
        result.prizes.push_back(rollMediumPrize(tier));
        LootPrize gold{};
        gold.tier = LootReelTier::Jackpot;
        gold.kind = LootPrizeKind::Gold;
        gold.goldAmount = (40 + zoneDepth_ * 15) * (1 + static_cast<int>(tier));
        result.prizes.push_back(gold);
        break;
    }
    }

    if (result.jackpot) {
        pityCounter_ = 0;
    } else {
        ++pityCounter_;
    }
    drainCoins(result.tier);

    result.coinPoolAfter = coinPool_;
    result.pityCounterAfter = pityCounter_;
    record(result);
    return result;
}

TavernGambleOdds SlotMachineLoot::tavernOdds() const noexcept {
    TavernGambleOdds odds{};
    odds.mythical = kTavernMythicalChance;
    odds.unique = 0.002F;
    const float pityLegendary = std::min(0.02F, static_cast<float>(tavernDrySpins_) * 0.000002F);
    odds.legendary = 0.008F + pityLegendary;
    odds.rare = 0.036F;
    odds.magic = 0.090F;
    odds.common = 0.180F;
    odds.gold = 0.420F;
    odds.nothing = 1.0F - (odds.mythical + odds.unique + odds.legendary + odds.rare + odds.magic + odds.common + odds.gold);
    if (odds.nothing < 0.0F) {
        odds.gold = std::max(0.0F, odds.gold + odds.nothing);
        odds.nothing = 0.0F;
    }
    return odds;
}

TavernGambleResult SlotMachineLoot::gambleTavern(int& playerGold) {
    TavernGambleResult result{};
    result.goldSpent = kTavernSpinCost;
    if (playerGold < kTavernSpinCost) {
        result.message = "The barkeep wants " + std::to_string(kTavernSpinCost) + " gold.";
        return result;
    }

    playerGold -= kTavernSpinCost;
    result.paid = true;

    const TavernGambleOdds odds = tavernOdds();
    const TavernPrize prize = tavernPrizeForRoll(unit(), odds);

    auto finishItem = [&](ItemMetadata item, const char* banner, const bool resetPity) {
        result.grantedItem = true;
        result.rarity = item.rarity;
        result.message = std::string(banner) + item.name;
        result.item = std::move(item);
        if (resetPity) {
            tavernDrySpins_ = 0;
        } else {
            ++tavernDrySpins_;
        }
    };

    if (prize == TavernPrize::Mythical) {
        ItemMetadata item = makeUnique(EntityTier::Boss);
        item.rarity = ItemRarity::Mythical;
        item.name = std::string("Mythic ") + item.name;
        item.value += 2000;
        item.bonuses.damage += 8;
        item.bonuses.maxHealth += 40;
        finishItem(std::move(item), "MYTHICAL! ", true);
        return result;
    }
    if (prize == TavernPrize::Unique) {
        finishItem(makeUnique(EntityTier::Elite), "Unique! ", true);
        return result;
    }
    if (prize == TavernPrize::Legendary) {
        finishItem(makeLegendary(EntityTier::Elite), "Legendary! ", true);
        return result;
    }
    if (prize == TavernPrize::Rare) {
        ItemMetadata item = makeSocketedGear(EntityTier::Standard);
        item.rarity = ItemRarity::Rare;
        item.name = std::string("Rare ") + item.name;
        finishItem(std::move(item), "", false);
        return result;
    }
    if (prize == TavernPrize::Magic) {
        finishItem(makeSocketedGear(EntityTier::Minor), "", false);
        return result;
    }
    if (prize == TavernPrize::Common) {
        if (unit() < 0.5F) {
            finishItem(makeConsumable(), "", false);
        } else {
            finishItem(makeJunk(), "", false);
        }
        return result;
    }
    if (prize == TavernPrize::Gold) {
        result.goldAwarded = rollInt(8, 20 + zoneDepth_ * 4);
        playerGold += result.goldAwarded;
        result.message = "+" + std::to_string(result.goldAwarded) + " gold";
        ++tavernDrySpins_;
        return result;
    }

    result.message = "The reels come up empty.";
    ++tavernDrySpins_;
    return result;
}

} // namespace systems
