#pragma once

#include "game/AppFlow.hpp"
#include "gameplay/GameTypes.hpp"
#include "systems/Equipment.hpp"
#include "systems/ItemTypes.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace game {

struct SaveCharacterData {
    int level{1};
    int experience{0};
    int experienceToNextLevel{100};
    int carriedSouls{0};
    int statUpgradesPurchased{0};
    float soulGainMultiplier{1.0F};
    int strength{10};
    int dexterity{10};
    int vitality{10};
    int unspentPoints{0};
    int currentHealth{0};
    int gold{100};
};

struct SaveProgressionData {
    int depth{1};
    std::uint32_t runSeed{0xCAFE0001U};
    int totalBossKills{0};
    int mobsKilledThisDepth{0};
    int lifetimeMobKills{0};
    int lootCoinPool{0};
    std::uint32_t lootRngSeed{0xC0FFEE42U};
};

struct SaveMobHealthEntry {
    std::uint32_t entityId{0};
    int currentHp{0};
    int maxHp{0};
};

struct SaveWorldData {
    gameplay::WorldZone activeZone{gameplay::WorldZone::TOWN};
    gameplay::Vec3 playerPosition{};
    bool attacksEnabled{false};
    std::uint32_t plainsSeed{0x9A100001U};
    int plainsDepth{1};
    std::vector<gameplay::WorldEntitySnapshot> scenery{};
    std::vector<SaveMobHealthEntry> mobHealth{};
};

struct SaveInventorySlot {
    int index{0};
    std::optional<systems::ItemMetadata> item{};
};

struct SaveEquipmentSlot {
    systems::EquipmentSlotKind slot{systems::EquipmentSlotKind::Head};
    std::optional<systems::ItemMetadata> item{};
};

struct SaveGameSnapshot {
    static constexpr int kCurrentVersion = 1;

    int version{kCurrentVersion};
    CharacterClass characterClass{CharacterClass::NONE};
    SaveCharacterData character{};
    SaveProgressionData progression{};
    SaveWorldData world{};
    int inventoryColumns{6};
    int inventoryRows{4};
    std::vector<SaveInventorySlot> inventory{};
    std::vector<SaveEquipmentSlot> equipment{};
};

struct SaveGameResult {
    bool success{false};
    std::string message;
};

class SaveGameIO {
public:
    [[nodiscard]] static std::filesystem::path defaultSavePath();
    [[nodiscard]] static bool saveExists();
    [[nodiscard]] static SaveGameResult saveToFile(
        const SaveGameSnapshot& snapshot,
        const std::filesystem::path& path = defaultSavePath());
    [[nodiscard]] static SaveGameResult loadFromFile(
        SaveGameSnapshot& outSnapshot,
        const std::filesystem::path& path = defaultSavePath());

    [[nodiscard]] static std::string serializeSnapshot(const SaveGameSnapshot& snapshot);
    [[nodiscard]] static SaveGameResult deserializeSnapshot(
        const std::string& json,
        SaveGameSnapshot& outSnapshot);
};

} // namespace game
