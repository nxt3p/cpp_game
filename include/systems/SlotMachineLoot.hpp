#pragma once

#include "systems/ItemGenerator.hpp"
#include "systems/ItemTypes.hpp"

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace systems {

/// Highest rarity a node is allowed to pay out. Early roads stop at magic (blue) gear.
enum class LootCeiling : std::uint8_t {
    Magic,
    Legendary,
    Unique,
};

/// Which reel the spin landed on.
enum class LootReelTier : std::uint8_t {
    Nothing,
    Common,
    Medium,
    Jackpot,
};

/// Concrete prize category inside a reel.
enum class LootPrizeKind : std::uint8_t {
    Gold,
    Consumable,
    Junk,
    Material,
    SocketedGear,
    Legendary,
    Unique,
};

struct LootPrize {
    LootReelTier tier{LootReelTier::Nothing};
    LootPrizeKind kind{LootPrizeKind::Gold};
    int goldAmount{0};
    std::optional<ItemMetadata> item{};
};

struct LootReelOdds {
    float nothing{0.0F};
    float common{0.0F};
    float medium{0.0F};
    float jackpot{0.0F};
};

struct LootSpinResult {
    bool spun{false};
    LootReelTier tier{LootReelTier::Nothing};
    int coinPoolBefore{0};
    int coinPoolAfter{0};
    int pityCounterAfter{0};
    bool jackpot{false};
    std::vector<LootPrize> prizes{};
};

/// Tavern mystery gamble. Mythical is fixed at 1/10000 and is not a combat drop.
struct TavernGambleOdds {
    float nothing{0.0F};
    float gold{0.0F};
    float common{0.0F};
    float magic{0.0F};
    float rare{0.0F};
    float legendary{0.0F};
    float unique{0.0F};
    float mythical{0.0F};
};

enum class TavernPrize : std::uint8_t {
    Mythical,
    Unique,
    Legendary,
    Rare,
    Magic,
    Common,
    Gold,
    Nothing
};

/// Maps a unit roll onto the tavern table. Mythical occupies [0, odds.mythical).
[[nodiscard]] TavernPrize tavernPrizeForRoll(float roll, const TavernGambleOdds& odds) noexcept;

struct TavernGambleResult {
    bool paid{false};
    bool grantedItem{false};
    int goldSpent{0};
    int goldAwarded{0};
    ItemRarity rarity{ItemRarity::Common};
    std::optional<ItemMetadata> item{};
    std::string message;
};

/// Aggregate counters for balancing harnesses.
struct LootTelemetry {
    int spins{0};
    int nothing{0};
    int common{0};
    int medium{0};
    int jackpot{0};
    int legendaryItems{0};
    int uniqueItems{0};
    int socketedItems{0};
    int consumables{0};
    int materials{0};
    int junk{0};
    int goldTotal{0};
    int coinsInserted{0};

    [[nodiscard]] float jackpotRate() const noexcept {
        return spins > 0 ? static_cast<float>(jackpot) / static_cast<float>(spins) : 0.0F;
    }
    [[nodiscard]] float mediumRate() const noexcept {
        return spins > 0 ? static_cast<float>(medium) / static_cast<float>(spins) : 0.0F;
    }
    [[nodiscard]] float nothingRate() const noexcept {
        return spins > 0 ? static_cast<float>(nothing) / static_cast<float>(spins) : 0.0F;
    }
};

/// "Every action is a coin": kills, chests and rocks add coins; each drop check spins the reels.
/// Coins bias odds toward the richer reels, a pity counter guarantees jackpots eventually,
/// and the pool drains on payout so streaks do not snowball.
class SlotMachineLoot {
public:
    explicit SlotMachineLoot(std::uint32_t seed = 0x51075EEDU);

    void insertCoins(ActionType type) noexcept;
    void setCoinPool(int coins) noexcept;
    [[nodiscard]] int coinPool() const noexcept { return coinPool_; }

    void setSeed(std::uint32_t seed);
    [[nodiscard]] std::uint32_t rngSeed() const noexcept { return seed_; }

    void setZoneDepth(int depth) noexcept;
    void setLootTierBonus(float bonus) noexcept;
    void setLootCeiling(LootCeiling ceiling) noexcept;
    void setPityCounter(int counter) noexcept;
    [[nodiscard]] int pityCounter() const noexcept { return pityCounter_; }

    [[nodiscard]] LootReelOdds oddsFor(EntityTier tier) const noexcept;

    /// Combat reels never pay mythical gear.
    [[nodiscard]] float combatMythicalChance() const noexcept { return 0.0F; }

    /// Fraction of a jackpot that is unique instead of legendary. Mythical is not on this reel.
    [[nodiscard]] float jackpotUniqueSlice(EntityTier tier) const noexcept;

    /// Legendary chance for the current coin pool and pity: jackpot odds times (1 - unique slice).
    /// Zero the pool and pity counter to read the base table (bosses sit near one in seven).
    [[nodiscard]] float legendaryChance(EntityTier tier) const noexcept;

    [[nodiscard]] TavernGambleOdds tavernOdds() const noexcept;

    /// Dry spins raise tavern legendary odds only, and only up to +0.02. Mythical stays 1/10000.
    void setTavernPitySpins(int drySpins) noexcept;

    TavernGambleResult gambleTavern(int& playerGold);

    static constexpr float kTavernMythicalChance = 0.0001F;
    static constexpr int kTavernSpinCost = 25;

    LootSpinResult spin(EntityTier tier);

    [[nodiscard]] const LootTelemetry& telemetry() const noexcept { return telemetry_; }
    void resetTelemetry() noexcept { telemetry_ = LootTelemetry{}; }

    /// Spins until a jackpot lands (bounded by `maxSpins`); used by the pity-timer test harness.
    static constexpr int kPityHardCap = 140;

private:
    [[nodiscard]] float unit();
    [[nodiscard]] int rollInt(int minInclusive, int maxInclusive);
    [[nodiscard]] LootReelTier rollReel(const LootReelOdds& odds);

    [[nodiscard]] LootPrize rollCommonPrize(EntityTier tier);
    [[nodiscard]] LootPrize rollMediumPrize(EntityTier tier);
    [[nodiscard]] LootPrize rollJackpotPrize(EntityTier tier);

    [[nodiscard]] ItemMetadata makeConsumable();
    [[nodiscard]] ItemMetadata makeJunk();
    [[nodiscard]] ItemMetadata makeMaterial(EntityTier tier);
    [[nodiscard]] ItemMetadata makeSocketedGear(EntityTier tier);
    [[nodiscard]] ItemMetadata makeLegendary(EntityTier tier);
    [[nodiscard]] ItemMetadata makeUnique(EntityTier tier);

    void drainCoins(LootReelTier tier) noexcept;
    void record(const LootSpinResult& result) noexcept;

    std::uint32_t seed_{0x51075EEDU};
    std::mt19937 rng_;
    ItemGenerator itemGenerator_;
    int coinPool_{0};
    int zoneDepth_{1};
    float lootTierBonus_{0.0F};
    LootCeiling lootCeiling_{LootCeiling::Unique};
    int pityCounter_{0};
    int tavernDrySpins_{0};
    LootTelemetry telemetry_{};
};

[[nodiscard]] const char* lootReelTierLabel(LootReelTier tier) noexcept;
[[nodiscard]] const char* lootPrizeKindLabel(LootPrizeKind kind) noexcept;

} // namespace systems
