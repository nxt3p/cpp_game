#include "game/SaveGame.hpp"
#include "game/SaveGamePlatform.hpp"

#include "systems/Equipment.hpp"
#include "systems/ItemStats.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_map>

namespace game {

namespace {

std::string escapeJsonString(const std::string& value) {
    std::ostringstream stream;
    stream << '"';
    for (const char character : value) {
        switch (character) {
        case '"':
            stream << "\\\"";
            break;
        case '\\':
            stream << "\\\\";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            stream << character;
            break;
        }
    }
    stream << '"';
    return stream.str();
}

std::string unescapeJsonString(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char character = value[index];
        if (character != '\\') {
            result.push_back(character);
            continue;
        }
        if (index + 1 >= value.size()) {
            break;
        }
        const char next = value[++index];
        switch (next) {
        case '"':
            result.push_back('"');
            break;
        case '\\':
            result.push_back('\\');
            break;
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        case 't':
            result.push_back('\t');
            break;
        default:
            result.push_back(next);
            break;
        }
    }
    return result;
}

void skipWhitespace(const std::string& json, std::size_t& cursor) {
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])) != 0) {
        ++cursor;
    }
}

[[nodiscard]] bool expectChar(const std::string& json, std::size_t& cursor, char expected) {
    skipWhitespace(json, cursor);
    if (cursor >= json.size() || json[cursor] != expected) {
        return false;
    }
    ++cursor;
    return true;
}

[[nodiscard]] bool parseStringValue(const std::string& json, std::size_t& cursor, std::string& out) {
    skipWhitespace(json, cursor);
    if (cursor >= json.size() || json[cursor] != '"') {
        return false;
    }
    ++cursor;
    std::string raw;
    while (cursor < json.size() && json[cursor] != '"') {
        raw.push_back(json[cursor]);
        ++cursor;
    }
    if (cursor >= json.size()) {
        return false;
    }
    ++cursor;
    out = unescapeJsonString(raw);
    return true;
}

[[nodiscard]] bool parseNumberValue(const std::string& json, std::size_t& cursor, double& out) {
    skipWhitespace(json, cursor);
    const std::size_t start = cursor;
    if (cursor < json.size() && (json[cursor] == '-' || json[cursor] == '+')) {
        ++cursor;
    }
    while (cursor < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[cursor])) != 0 || json[cursor] == '.')) {
        ++cursor;
    }
    if (start == cursor) {
        return false;
    }
    out = std::strtod(json.c_str() + static_cast<long>(start), nullptr);
    return true;
}

[[nodiscard]] bool parseBoolValue(const std::string& json, std::size_t& cursor, bool& out) {
    skipWhitespace(json, cursor);
    if (json.compare(cursor, 4, "true") == 0) {
        cursor += 4;
        out = true;
        return true;
    }
    if (json.compare(cursor, 5, "false") == 0) {
        cursor += 5;
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parseNullValue(const std::string& json, std::size_t& cursor) {
    skipWhitespace(json, cursor);
    if (json.compare(cursor, 4, "null") == 0) {
        cursor += 4;
        return true;
    }
    return false;
}

[[nodiscard]] const char* characterClassToString(const CharacterClass characterClass) noexcept {
    switch (characterClass) {
    case CharacterClass::WARRIOR:
        return "WARRIOR";
    case CharacterClass::RANGER:
        return "RANGER";
    case CharacterClass::MAGE:
        return "MAGE";
    case CharacterClass::NONE:
        return "NONE";
    }
    return "NONE";
}

[[nodiscard]] CharacterClass characterClassFromString(const std::string& value) noexcept {
    if (value == "WARRIOR") {
        return CharacterClass::WARRIOR;
    }
    if (value == "RANGER") {
        return CharacterClass::RANGER;
    }
    if (value == "MAGE") {
        return CharacterClass::MAGE;
    }
    return CharacterClass::NONE;
}

[[nodiscard]] const char* worldZoneToString(const gameplay::WorldZone zone) noexcept {
    return zone == gameplay::WorldZone::PLAINS ? "PLAINS" : "TOWN";
}

[[nodiscard]] gameplay::WorldZone worldZoneFromString(const std::string& value) noexcept {
    return value == "PLAINS" ? gameplay::WorldZone::PLAINS : gameplay::WorldZone::TOWN;
}

[[nodiscard]] const char* entityKindToString(const gameplay::EntityKind kind) noexcept {
    switch (kind) {
    case gameplay::EntityKind::PLAYER:
        return "PLAYER";
    case gameplay::EntityKind::ENEMY_MOB:
        return "ENEMY_MOB";
    case gameplay::EntityKind::ENEMY_BOSS:
        return "ENEMY_BOSS";
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return "NPC_BLACKSMITH";
    case gameplay::EntityKind::ENV_TREE:
        return "ENV_TREE";
    case gameplay::EntityKind::ENV_BUSH:
        return "ENV_BUSH";
    case gameplay::EntityKind::ENV_ROCK:
        return "ENV_ROCK";
    case gameplay::EntityKind::ENV_CHEST:
        return "ENV_CHEST";
    case gameplay::EntityKind::ENV_HOUSE:
        return "ENV_HOUSE";
    case gameplay::EntityKind::ENV_MUSHROOM:
        return "ENV_MUSHROOM";
    }
    return "ENV_TREE";
}

[[nodiscard]] gameplay::EntityKind entityKindFromString(const std::string& value) noexcept {
    static const std::unordered_map<std::string, gameplay::EntityKind> lookup = {
        {"PLAYER", gameplay::EntityKind::PLAYER},
        {"ENEMY_MOB", gameplay::EntityKind::ENEMY_MOB},
        {"ENEMY_BOSS", gameplay::EntityKind::ENEMY_BOSS},
        {"NPC_BLACKSMITH", gameplay::EntityKind::NPC_BLACKSMITH},
        {"ENV_TREE", gameplay::EntityKind::ENV_TREE},
        {"ENV_BUSH", gameplay::EntityKind::ENV_BUSH},
        {"ENV_ROCK", gameplay::EntityKind::ENV_ROCK},
        {"ENV_CHEST", gameplay::EntityKind::ENV_CHEST},
        {"ENV_HOUSE", gameplay::EntityKind::ENV_HOUSE},
        {"ENV_MUSHROOM", gameplay::EntityKind::ENV_MUSHROOM},
    };
    const auto iterator = lookup.find(value);
    return iterator == lookup.end() ? gameplay::EntityKind::ENV_TREE : iterator->second;
}

[[nodiscard]] systems::ItemCategory itemCategoryFromLabel(const std::string& value) noexcept {
    static const std::unordered_map<std::string, systems::ItemCategory> lookup = {
        {"Head", systems::ItemCategory::Head},
        {"Shoulders", systems::ItemCategory::Shoulders},
        {"Chest", systems::ItemCategory::Chest},
        {"Hands", systems::ItemCategory::Hands},
        {"Waist", systems::ItemCategory::Waist},
        {"Legs", systems::ItemCategory::Legs},
        {"Feet", systems::ItemCategory::Feet},
        {"Weapon", systems::ItemCategory::Weapon},
        {"Off-Hand", systems::ItemCategory::OffHand},
        {"Amulet", systems::ItemCategory::Amulet},
        {"Ring", systems::ItemCategory::Ring},
        {"Cloak", systems::ItemCategory::Cloak},
        {"Charm", systems::ItemCategory::Charm},
        {"Relic", systems::ItemCategory::Relic},
        {"Consumable", systems::ItemCategory::Consumable},
        {"Misc", systems::ItemCategory::Misc},
    };
    const auto iterator = lookup.find(value);
    return iterator == lookup.end() ? systems::ItemCategory::Misc : iterator->second;
}

[[nodiscard]] systems::ItemRarity itemRarityFromLabel(const std::string& value) noexcept {
    if (value == "Rare") {
        return systems::ItemRarity::Rare;
    }
    if (value == "Legendary") {
        return systems::ItemRarity::Legendary;
    }
    return systems::ItemRarity::Common;
}

[[nodiscard]] const char* itemRarityToString(const systems::ItemRarity rarity) noexcept {
    return systems::rarityLabel(rarity);
}

[[nodiscard]] const char* equipmentSlotToString(const systems::EquipmentSlotKind slot) noexcept {
    return systems::Equipment::slotLabel(slot);
}

[[nodiscard]] systems::EquipmentSlotKind equipmentSlotFromString(const std::string& value) noexcept {
    for (int index = 0; index < static_cast<int>(systems::EquipmentSlotKind::Count); ++index) {
        const auto slot = static_cast<systems::EquipmentSlotKind>(index);
        if (value == systems::Equipment::slotLabel(slot)) {
            return slot;
        }
    }
    return systems::EquipmentSlotKind::Head;
}

void writeItemBonusesJson(std::ostringstream& stream, const systems::ItemStatBonuses& bonuses) {
    stream << "{"
           << "\"strength\":" << bonuses.strength << ","
           << "\"dexterity\":" << bonuses.dexterity << ","
           << "\"vitality\":" << bonuses.vitality << ","
           << "\"maxHealth\":" << bonuses.maxHealth << ","
           << "\"attackSpeed\":" << bonuses.attackSpeed << ","
           << "\"damage\":" << bonuses.damage << ","
           << "\"lightRadius\":" << bonuses.lightRadius << "}";
}

void writeItemJson(std::ostringstream& stream, const systems::ItemMetadata& item) {
    stream << "{"
           << "\"itemId\":" << item.itemId << ","
           << "\"name\":" << escapeJsonString(item.name) << ","
           << "\"rarity\":" << escapeJsonString(itemRarityToString(item.rarity)) << ","
           << "\"value\":" << item.value << ","
           << "\"category\":" << escapeJsonString(systems::itemCategoryLabel(item.category))
           << ","
           << "\"iconLetter\":" << escapeJsonString(std::string(1, item.iconLetter)) << ","
           << "\"itemLevel\":" << item.itemLevel << ","
           << "\"masteryLevel\":" << item.masteryLevel << ","
           << "\"masteryXp\":" << item.masteryXp << ","
           << "\"upgradeLevel\":" << item.upgradeLevel << ","
           << "\"bonuses\":";
    writeItemBonusesJson(stream, item.bonuses);
    stream << "}";
}

[[nodiscard]] bool parseItemBonuses(
    const std::string& json,
    std::size_t& cursor,
    systems::ItemStatBonuses& bonuses) {
    if (!expectChar(json, cursor, '{')) {
        return false;
    }
    while (true) {
        skipWhitespace(json, cursor);
        if (cursor < json.size() && json[cursor] == '}') {
            ++cursor;
            return true;
        }
        std::string key;
        if (!parseStringValue(json, cursor, key)) {
            return false;
        }
        if (!expectChar(json, cursor, ':')) {
            return false;
        }
        double number = 0.0;
        if (!parseNumberValue(json, cursor, number)) {
            return false;
        }
        if (key == "strength") {
            bonuses.strength = static_cast<int>(number);
        } else if (key == "dexterity") {
            bonuses.dexterity = static_cast<int>(number);
        } else if (key == "vitality") {
            bonuses.vitality = static_cast<int>(number);
        } else if (key == "maxHealth") {
            bonuses.maxHealth = static_cast<int>(number);
        } else if (key == "attackSpeed") {
            bonuses.attackSpeed = static_cast<float>(number);
        } else if (key == "damage") {
            bonuses.damage = static_cast<int>(number);
        } else if (key == "lightRadius") {
            bonuses.lightRadius = static_cast<float>(number);
        }
        skipWhitespace(json, cursor);
        if (cursor < json.size() && json[cursor] == ',') {
            ++cursor;
            continue;
        }
        if (cursor < json.size() && json[cursor] == '}') {
            ++cursor;
            return true;
        }
        return false;
    }
}

[[nodiscard]] bool parseItemJson(
    const std::string& json,
    std::size_t& cursor,
    systems::ItemMetadata& item) {
    if (!expectChar(json, cursor, '{')) {
        return false;
    }
    while (true) {
        skipWhitespace(json, cursor);
        if (cursor < json.size() && json[cursor] == '}') {
            ++cursor;
            systems::applyItemDefinition(item);
            return true;
        }
        std::string key;
        if (!parseStringValue(json, cursor, key)) {
            return false;
        }
        if (!expectChar(json, cursor, ':')) {
            return false;
        }
        if (key == "itemId") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                return false;
            }
            item.itemId = static_cast<std::uint32_t>(number);
        } else if (key == "name") {
            if (!parseStringValue(json, cursor, item.name)) {
                return false;
            }
        } else if (key == "rarity") {
            std::string rarity;
            if (!parseStringValue(json, cursor, rarity)) {
                return false;
            }
            item.rarity = itemRarityFromLabel(rarity);
        } else if (key == "value") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                return false;
            }
            item.value = static_cast<int>(number);
        } else if (key == "category") {
            std::string category;
            if (!parseStringValue(json, cursor, category)) {
                return false;
            }
            item.category = itemCategoryFromLabel(category);
        } else if (key == "iconLetter") {
            std::string letter;
            if (!parseStringValue(json, cursor, letter)) {
                return false;
            }
            item.iconLetter = letter.empty() ? '?' : letter.front();
        } else if (key == "itemLevel") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                return false;
            }
            item.itemLevel = static_cast<int>(number);
        } else if (key == "masteryLevel") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                return false;
            }
            item.masteryLevel = static_cast<int>(number);
        } else if (key == "masteryXp") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                return false;
            }
            item.masteryXp = static_cast<int>(number);
        } else if (key == "upgradeLevel") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                return false;
            }
            item.upgradeLevel = static_cast<int>(number);
        } else if (key == "bonuses") {
            if (!parseItemBonuses(json, cursor, item.bonuses)) {
                return false;
            }
        } else {
            return false;
        }
        skipWhitespace(json, cursor);
        if (cursor < json.size() && json[cursor] == ',') {
            ++cursor;
            continue;
        }
        if (cursor < json.size() && json[cursor] == '}') {
            ++cursor;
            systems::applyItemDefinition(item);
            return true;
        }
        return false;
    }
}

[[nodiscard]] std::filesystem::path userDataDirectory() {
#if defined(__EMSCRIPTEN__)
    return std::filesystem::path("localStorage://cppGame");
#elif defined(_WIN32)
    if (const char* localAppData = std::getenv("LOCALAPPDATA");
        localAppData != nullptr && localAppData[0] != '\0') {
        return std::filesystem::path(localAppData) / "cppGame";
    }
    if (const char* appData = std::getenv("APPDATA");
        appData != nullptr && appData[0] != '\0') {
        return std::filesystem::path(appData) / "cppGame";
    }
    return std::filesystem::path("cppGame");
#else
    if (const char* xdgData = std::getenv("XDG_DATA_HOME");
        xdgData != nullptr && xdgData[0] != '\0') {
        return std::filesystem::path(xdgData) / "cppGame";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / ".local" / "share" / "cppGame";
    }
    return std::filesystem::path("cppGame");
#endif
}

} // namespace

std::filesystem::path SaveGameIO::defaultSavePath() {
    return userDataDirectory() / "savegame.json";
}

bool SaveGameIO::saveExists() {
    return save_platform::payloadExists(defaultSavePath());
}

SaveGameResult SaveGameIO::saveToFile(
    const SaveGameSnapshot& snapshot,
    const std::filesystem::path& path) {
    const std::string payload = serializeSnapshot(snapshot);
    SaveGameResult result = save_platform::writePayload(path, payload);
    if (result.success) {
        result.message = "Saved to " + save_platform::storageDescription(path) + ".";
    }
    return result;
}

SaveGameResult SaveGameIO::loadFromFile(
    SaveGameSnapshot& outSnapshot,
    const std::filesystem::path& path) {
    std::string payload;
    SaveGameResult result = save_platform::readPayload(path, payload);
    if (!result.success) {
        return result;
    }

    return deserializeSnapshot(payload, outSnapshot);
}

std::string SaveGameIO::serializeSnapshot(const SaveGameSnapshot& snapshot) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "  \"version\":" << snapshot.version << ",\n";
    stream << "  \"characterClass\":" << escapeJsonString(characterClassToString(snapshot.characterClass))
           << ",\n";

    stream << "  \"character\":{"
           << "\"level\":" << snapshot.character.level << ","
           << "\"experience\":" << snapshot.character.experience << ","
           << "\"experienceToNextLevel\":" << snapshot.character.experienceToNextLevel << ","
           << "\"carriedSouls\":" << snapshot.character.carriedSouls << ","
           << "\"statUpgradesPurchased\":" << snapshot.character.statUpgradesPurchased << ","
           << "\"soulGainMultiplier\":" << snapshot.character.soulGainMultiplier << ","
           << "\"strength\":" << snapshot.character.strength << ","
           << "\"dexterity\":" << snapshot.character.dexterity << ","
           << "\"vitality\":" << snapshot.character.vitality << ","
           << "\"unspentPoints\":" << snapshot.character.unspentPoints << ","
           << "\"currentHealth\":" << snapshot.character.currentHealth << ","
           << "\"gold\":" << snapshot.character.gold << "},\n";

    stream << "  \"progression\":{"
           << "\"depth\":" << snapshot.progression.depth << ","
           << "\"runSeed\":" << snapshot.progression.runSeed << ","
           << "\"totalBossKills\":" << snapshot.progression.totalBossKills << ","
           << "\"mobsKilledThisDepth\":" << snapshot.progression.mobsKilledThisDepth << ","
           << "\"lifetimeMobKills\":" << snapshot.progression.lifetimeMobKills << ","
           << "\"lootCoinPool\":" << snapshot.progression.lootCoinPool << ","
           << "\"lootRngSeed\":" << snapshot.progression.lootRngSeed << "},\n";

    stream << "  \"world\":{"
           << "\"activeZone\":" << escapeJsonString(worldZoneToString(snapshot.world.activeZone)) << ","
           << "\"playerX\":" << snapshot.world.playerPosition.x << ","
           << "\"playerY\":" << snapshot.world.playerPosition.y << ","
           << "\"playerZ\":" << snapshot.world.playerPosition.z << ","
           << "\"attacksEnabled\":" << (snapshot.world.attacksEnabled ? "true" : "false") << ","
           << "\"plainsSeed\":" << snapshot.world.plainsSeed << ","
           << "\"plainsDepth\":" << snapshot.world.plainsDepth << ","
           << "\"scenery\":[";
    for (std::size_t index = 0; index < snapshot.world.scenery.size(); ++index) {
        const gameplay::WorldEntitySnapshot& entity = snapshot.world.scenery[index];
        if (index > 0) {
            stream << ",";
        }
        stream << "{"
               << "\"id\":" << entity.id << ","
               << "\"kind\":" << escapeJsonString(entityKindToString(entity.kind)) << ","
               << "\"x\":" << entity.position.x << ","
               << "\"y\":" << entity.position.y << ","
               << "\"z\":" << entity.position.z << ","
               << "\"active\":" << (entity.active ? "true" : "false") << ","
               << "\"variant\":" << static_cast<int>(entity.variant) << "}";
    }
    stream << "],\"mobHealth\":[";
    for (std::size_t index = 0; index < snapshot.world.mobHealth.size(); ++index) {
        const SaveMobHealthEntry& entry = snapshot.world.mobHealth[index];
        if (index > 0) {
            stream << ",";
        }
        stream << "{"
               << "\"entityId\":" << entry.entityId << ","
               << "\"currentHp\":" << entry.currentHp << ","
               << "\"maxHp\":" << entry.maxHp << "}";
    }
    stream << "]},\n";

    stream << "  \"inventory\":{"
           << "\"columns\":" << snapshot.inventoryColumns << ","
           << "\"rows\":" << snapshot.inventoryRows << ","
           << "\"slots\":[";
    for (std::size_t index = 0; index < snapshot.inventory.size(); ++index) {
        const SaveInventorySlot& slot = snapshot.inventory[index];
        if (index > 0) {
            stream << ",";
        }
        stream << "{"
               << "\"index\":" << slot.index << ","
               << "\"item\":";
        if (slot.item.has_value()) {
            writeItemJson(stream, *slot.item);
        } else {
            stream << "null";
        }
        stream << "}";
    }
    stream << "]},\n";

    stream << "  \"equipment\":[";
    for (std::size_t index = 0; index < snapshot.equipment.size(); ++index) {
        const SaveEquipmentSlot& slot = snapshot.equipment[index];
        if (index > 0) {
            stream << ",";
        }
        stream << "{"
               << "\"slot\":" << escapeJsonString(equipmentSlotToString(slot.slot)) << ","
               << "\"item\":";
        if (slot.item.has_value()) {
            writeItemJson(stream, *slot.item);
        } else {
            stream << "null";
        }
        stream << "}";
    }
    stream << "]\n";
    stream << "}\n";
    return stream.str();
}

SaveGameResult SaveGameIO::deserializeSnapshot(
    const std::string& json,
    SaveGameSnapshot& outSnapshot) {
    SaveGameResult result{};
    outSnapshot = SaveGameSnapshot{};
    std::size_t cursor = 0;

    if (!expectChar(json, cursor, '{')) {
        result.message = "Invalid save file (missing root object).";
        return result;
    }

    while (true) {
        skipWhitespace(json, cursor);
        if (cursor < json.size() && json[cursor] == '}') {
            ++cursor;
            break;
        }

        std::string key;
        if (!parseStringValue(json, cursor, key)) {
            result.message = "Invalid save file (malformed key).";
            return result;
        }
        if (!expectChar(json, cursor, ':')) {
            result.message = "Invalid save file (missing colon).";
            return result;
        }

        if (key == "version") {
            double number = 0.0;
            if (!parseNumberValue(json, cursor, number)) {
                result.message = "Invalid save file (version).";
                return result;
            }
            outSnapshot.version = static_cast<int>(number);
        } else if (key == "characterClass") {
            std::string className;
            if (!parseStringValue(json, cursor, className)) {
                result.message = "Invalid save file (characterClass).";
                return result;
            }
            outSnapshot.characterClass = characterClassFromString(className);
        } else if (key == "character") {
            if (!expectChar(json, cursor, '{')) {
                result.message = "Invalid save file (character).";
                return result;
            }
            while (true) {
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == '}') {
                    ++cursor;
                    break;
                }
                std::string field;
                if (!parseStringValue(json, cursor, field)) {
                    result.message = "Invalid save file (character field).";
                    return result;
                }
                if (!expectChar(json, cursor, ':')) {
                    result.message = "Invalid save file (character field colon).";
                    return result;
                }
                double number = 0.0;
                if (!parseNumberValue(json, cursor, number)) {
                    result.message = "Invalid save file (character number).";
                    return result;
                }
                if (field == "level") {
                    outSnapshot.character.level = static_cast<int>(number);
                } else if (field == "experience") {
                    outSnapshot.character.experience = static_cast<int>(number);
                } else if (field == "experienceToNextLevel") {
                    outSnapshot.character.experienceToNextLevel = static_cast<int>(number);
                } else if (field == "carriedSouls") {
                    outSnapshot.character.carriedSouls = static_cast<int>(number);
                } else if (field == "statUpgradesPurchased") {
                    outSnapshot.character.statUpgradesPurchased = static_cast<int>(number);
                } else if (field == "soulGainMultiplier") {
                    outSnapshot.character.soulGainMultiplier = static_cast<float>(number);
                } else if (field == "strength") {
                    outSnapshot.character.strength = static_cast<int>(number);
                } else if (field == "dexterity") {
                    outSnapshot.character.dexterity = static_cast<int>(number);
                } else if (field == "vitality") {
                    outSnapshot.character.vitality = static_cast<int>(number);
                } else if (field == "unspentPoints") {
                    outSnapshot.character.unspentPoints = static_cast<int>(number);
                } else if (field == "currentHealth") {
                    outSnapshot.character.currentHealth = static_cast<int>(number);
                } else if (field == "gold") {
                    outSnapshot.character.gold = static_cast<int>(number);
                }
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == ',') {
                    ++cursor;
                }
            }
        } else if (key == "progression") {
            if (!expectChar(json, cursor, '{')) {
                result.message = "Invalid save file (progression).";
                return result;
            }
            while (true) {
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == '}') {
                    ++cursor;
                    break;
                }
                std::string field;
                if (!parseStringValue(json, cursor, field)) {
                    result.message = "Invalid save file (progression field).";
                    return result;
                }
                if (!expectChar(json, cursor, ':')) {
                    result.message = "Invalid save file (progression colon).";
                    return result;
                }
                double number = 0.0;
                if (!parseNumberValue(json, cursor, number)) {
                    result.message = "Invalid save file (progression number).";
                    return result;
                }
                if (field == "depth") {
                    outSnapshot.progression.depth = static_cast<int>(number);
                } else if (field == "runSeed") {
                    outSnapshot.progression.runSeed = static_cast<std::uint32_t>(number);
                } else if (field == "totalBossKills") {
                    outSnapshot.progression.totalBossKills = static_cast<int>(number);
                } else if (field == "mobsKilledThisDepth") {
                    outSnapshot.progression.mobsKilledThisDepth = static_cast<int>(number);
                } else if (field == "lifetimeMobKills") {
                    outSnapshot.progression.lifetimeMobKills = static_cast<int>(number);
                } else if (field == "lootCoinPool") {
                    outSnapshot.progression.lootCoinPool = static_cast<int>(number);
                } else if (field == "lootRngSeed") {
                    outSnapshot.progression.lootRngSeed = static_cast<std::uint32_t>(number);
                }
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == ',') {
                    ++cursor;
                }
            }
        } else if (key == "world") {
            if (!expectChar(json, cursor, '{')) {
                result.message = "Invalid save file (world).";
                return result;
            }
            while (true) {
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == '}') {
                    ++cursor;
                    break;
                }
                std::string field;
                if (!parseStringValue(json, cursor, field)) {
                    result.message = "Invalid save file (world field).";
                    return result;
                }
                if (!expectChar(json, cursor, ':')) {
                    result.message = "Invalid save file (world colon).";
                    return result;
                }

                if (field == "activeZone") {
                    std::string zoneName;
                    if (!parseStringValue(json, cursor, zoneName)) {
                        result.message = "Invalid save file (activeZone).";
                        return result;
                    }
                    outSnapshot.world.activeZone = worldZoneFromString(zoneName);
                } else if (field == "playerX" || field == "playerY" || field == "playerZ") {
                    double number = 0.0;
                    if (!parseNumberValue(json, cursor, number)) {
                        result.message = "Invalid save file (player position).";
                        return result;
                    }
                    if (field == "playerX") {
                        outSnapshot.world.playerPosition.x = static_cast<float>(number);
                    } else if (field == "playerY") {
                        outSnapshot.world.playerPosition.y = static_cast<float>(number);
                    } else {
                        outSnapshot.world.playerPosition.z = static_cast<float>(number);
                    }
                } else if (field == "attacksEnabled") {
                    if (!parseBoolValue(json, cursor, outSnapshot.world.attacksEnabled)) {
                        result.message = "Invalid save file (attacksEnabled).";
                        return result;
                    }
                } else if (field == "plainsSeed") {
                    double number = 0.0;
                    if (!parseNumberValue(json, cursor, number)) {
                        result.message = "Invalid save file (plainsSeed).";
                        return result;
                    }
                    outSnapshot.world.plainsSeed = static_cast<std::uint32_t>(number);
                } else if (field == "plainsDepth") {
                    double number = 0.0;
                    if (!parseNumberValue(json, cursor, number)) {
                        result.message = "Invalid save file (plainsDepth).";
                        return result;
                    }
                    outSnapshot.world.plainsDepth = static_cast<int>(number);
                } else if (field == "scenery") {
                    if (!expectChar(json, cursor, '[')) {
                        result.message = "Invalid save file (scenery array).";
                        return result;
                    }
                    while (true) {
                        skipWhitespace(json, cursor);
                        if (cursor < json.size() && json[cursor] == ']') {
                            ++cursor;
                            break;
                        }
                        if (!expectChar(json, cursor, '{')) {
                            result.message = "Invalid save file (scenery entry).";
                            return result;
                        }
                        gameplay::WorldEntitySnapshot entity{};
                        while (true) {
                            skipWhitespace(json, cursor);
                            if (cursor < json.size() && json[cursor] == '}') {
                                ++cursor;
                                break;
                            }
                            std::string entityField;
                            if (!parseStringValue(json, cursor, entityField)) {
                                result.message = "Invalid save file (scenery field).";
                                return result;
                            }
                            if (!expectChar(json, cursor, ':')) {
                                result.message = "Invalid save file (scenery colon).";
                                return result;
                            }
                            if (entityField == "kind") {
                                std::string kindName;
                                if (!parseStringValue(json, cursor, kindName)) {
                                    result.message = "Invalid save file (entity kind).";
                                    return result;
                                }
                                entity.kind = entityKindFromString(kindName);
                            } else if (entityField == "active") {
                                if (!parseBoolValue(json, cursor, entity.active)) {
                                    result.message = "Invalid save file (entity active).";
                                    return result;
                                }
                            } else {
                                double number = 0.0;
                                if (!parseNumberValue(json, cursor, number)) {
                                    result.message = "Invalid save file (entity number).";
                                    return result;
                                }
                                if (entityField == "id") {
                                    entity.id = static_cast<std::uint32_t>(number);
                                } else if (entityField == "x") {
                                    entity.position.x = static_cast<float>(number);
                                } else if (entityField == "y") {
                                    entity.position.y = static_cast<float>(number);
                                } else if (entityField == "z") {
                                    entity.position.z = static_cast<float>(number);
                                } else if (entityField == "variant") {
                                    entity.variant =
                                        static_cast<std::uint8_t>(static_cast<int>(number));
                                }
                            }
                            skipWhitespace(json, cursor);
                            if (cursor < json.size() && json[cursor] == ',') {
                                ++cursor;
                            }
                        }
                        outSnapshot.world.scenery.push_back(entity);
                        skipWhitespace(json, cursor);
                        if (cursor < json.size() && json[cursor] == ',') {
                            ++cursor;
                        }
                    }
                } else if (field == "mobHealth") {
                    if (!expectChar(json, cursor, '[')) {
                        result.message = "Invalid save file (mobHealth array).";
                        return result;
                    }
                    while (true) {
                        skipWhitespace(json, cursor);
                        if (cursor < json.size() && json[cursor] == ']') {
                            ++cursor;
                            break;
                        }
                        if (!expectChar(json, cursor, '{')) {
                            result.message = "Invalid save file (mobHealth entry).";
                            return result;
                        }
                        SaveMobHealthEntry entry{};
                        while (true) {
                            skipWhitespace(json, cursor);
                            if (cursor < json.size() && json[cursor] == '}') {
                                ++cursor;
                                break;
                            }
                            std::string mobField;
                            if (!parseStringValue(json, cursor, mobField)) {
                                result.message = "Invalid save file (mobHealth field).";
                                return result;
                            }
                            if (!expectChar(json, cursor, ':')) {
                                result.message = "Invalid save file (mobHealth colon).";
                                return result;
                            }
                            double number = 0.0;
                            if (!parseNumberValue(json, cursor, number)) {
                                result.message = "Invalid save file (mobHealth number).";
                                return result;
                            }
                            if (mobField == "entityId") {
                                entry.entityId = static_cast<std::uint32_t>(number);
                            } else if (mobField == "currentHp") {
                                entry.currentHp = static_cast<int>(number);
                            } else if (mobField == "maxHp") {
                                entry.maxHp = static_cast<int>(number);
                            }
                            skipWhitespace(json, cursor);
                            if (cursor < json.size() && json[cursor] == ',') {
                                ++cursor;
                            }
                        }
                        outSnapshot.world.mobHealth.push_back(entry);
                        skipWhitespace(json, cursor);
                        if (cursor < json.size() && json[cursor] == ',') {
                            ++cursor;
                        }
                    }
                } else {
                    result.message = "Invalid save file (unknown world field).";
                    return result;
                }

                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == ',') {
                    ++cursor;
                }
            }
        } else if (key == "inventory") {
            if (!expectChar(json, cursor, '{')) {
                result.message = "Invalid save file (inventory).";
                return result;
            }
            while (true) {
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == '}') {
                    ++cursor;
                    break;
                }
                std::string field;
                if (!parseStringValue(json, cursor, field)) {
                    result.message = "Invalid save file (inventory field).";
                    return result;
                }
                if (!expectChar(json, cursor, ':')) {
                    result.message = "Invalid save file (inventory colon).";
                    return result;
                }

                if (field == "columns") {
                    double number = 0.0;
                    if (!parseNumberValue(json, cursor, number)) {
                        result.message = "Invalid save file (inventory columns).";
                        return result;
                    }
                    outSnapshot.inventoryColumns = static_cast<int>(number);
                } else if (field == "rows") {
                    double number = 0.0;
                    if (!parseNumberValue(json, cursor, number)) {
                        result.message = "Invalid save file (inventory rows).";
                        return result;
                    }
                    outSnapshot.inventoryRows = static_cast<int>(number);
                } else if (field == "slots") {
                    if (!expectChar(json, cursor, '[')) {
                        result.message = "Invalid save file (inventory slots).";
                        return result;
                    }
                    while (true) {
                        skipWhitespace(json, cursor);
                        if (cursor < json.size() && json[cursor] == ']') {
                            ++cursor;
                            break;
                        }
                        if (!expectChar(json, cursor, '{')) {
                            result.message = "Invalid save file (inventory slot).";
                            return result;
                        }
                        SaveInventorySlot slot{};
                        while (true) {
                            skipWhitespace(json, cursor);
                            if (cursor < json.size() && json[cursor] == '}') {
                                ++cursor;
                                break;
                            }
                            std::string slotField;
                            if (!parseStringValue(json, cursor, slotField)) {
                                result.message = "Invalid save file (inventory slot field).";
                                return result;
                            }
                            if (!expectChar(json, cursor, ':')) {
                                result.message = "Invalid save file (inventory slot colon).";
                                return result;
                            }
                            if (slotField == "index") {
                                double number = 0.0;
                                if (!parseNumberValue(json, cursor, number)) {
                                    result.message = "Invalid save file (inventory index).";
                                    return result;
                                }
                                slot.index = static_cast<int>(number);
                            } else if (slotField == "item") {
                                skipWhitespace(json, cursor);
                                if (parseNullValue(json, cursor)) {
                                    slot.item.reset();
                                } else {
                                    systems::ItemMetadata item{};
                                    if (!parseItemJson(json, cursor, item)) {
                                        result.message = "Invalid save file (inventory item).";
                                        return result;
                                    }
                                    slot.item = std::move(item);
                                }
                            }
                            skipWhitespace(json, cursor);
                            if (cursor < json.size() && json[cursor] == ',') {
                                ++cursor;
                            }
                        }
                        outSnapshot.inventory.push_back(std::move(slot));
                        skipWhitespace(json, cursor);
                        if (cursor < json.size() && json[cursor] == ',') {
                            ++cursor;
                        }
                    }
                } else {
                    result.message = "Invalid save file (unknown inventory field).";
                    return result;
                }

                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == ',') {
                    ++cursor;
                }
            }
        } else if (key == "equipment") {
            if (!expectChar(json, cursor, '[')) {
                result.message = "Invalid save file (equipment).";
                return result;
            }
            while (true) {
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == ']') {
                    ++cursor;
                    break;
                }
                if (!expectChar(json, cursor, '{')) {
                    result.message = "Invalid save file (equipment slot).";
                    return result;
                }
                SaveEquipmentSlot slot{};
                while (true) {
                    skipWhitespace(json, cursor);
                    if (cursor < json.size() && json[cursor] == '}') {
                        ++cursor;
                        break;
                    }
                    std::string slotField;
                    if (!parseStringValue(json, cursor, slotField)) {
                        result.message = "Invalid save file (equipment field).";
                        return result;
                    }
                    if (!expectChar(json, cursor, ':')) {
                        result.message = "Invalid save file (equipment colon).";
                        return result;
                    }
                    if (slotField == "slot") {
                        std::string slotName;
                        if (!parseStringValue(json, cursor, slotName)) {
                            result.message = "Invalid save file (equipment slot name).";
                            return result;
                        }
                        slot.slot = equipmentSlotFromString(slotName);
                    } else if (slotField == "item") {
                        skipWhitespace(json, cursor);
                        if (parseNullValue(json, cursor)) {
                            slot.item.reset();
                        } else {
                            systems::ItemMetadata item{};
                            if (!parseItemJson(json, cursor, item)) {
                                result.message = "Invalid save file (equipment item).";
                                return result;
                            }
                            slot.item = std::move(item);
                        }
                    }
                    skipWhitespace(json, cursor);
                    if (cursor < json.size() && json[cursor] == ',') {
                        ++cursor;
                    }
                }
                outSnapshot.equipment.push_back(std::move(slot));
                skipWhitespace(json, cursor);
                if (cursor < json.size() && json[cursor] == ',') {
                    ++cursor;
                }
            }
        } else {
            result.message = "Invalid save file (unknown root field: " + key + ").";
            return result;
        }

        skipWhitespace(json, cursor);
        if (cursor < json.size() && json[cursor] == ',') {
            ++cursor;
        }
    }

    if (outSnapshot.version != SaveGameSnapshot::kCurrentVersion) {
        result.message = "Unsupported save version.";
        return result;
    }
    if (outSnapshot.characterClass == CharacterClass::NONE) {
        result.message = "Save file missing character class.";
        return result;
    }

    result.success = true;
    result.message = "Save loaded.";
    return result;
}

} // namespace game
