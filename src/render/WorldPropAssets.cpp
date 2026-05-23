#include "render/WorldPropAssets.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace render {

namespace {

constexpr std::size_t categoryIndex(const WorldPropCategory category) noexcept {
    return static_cast<std::size_t>(category);
}

[[nodiscard]] WorldPropCategory categoryFromToken(const std::string& token) {
    if (token == "tree") {
        return WorldPropCategory::Tree;
    }
    if (token == "bush") {
        return WorldPropCategory::Bush;
    }
    if (token == "rock") {
        return WorldPropCategory::Rock;
    }
    if (token == "house") {
        return WorldPropCategory::House;
    }
    if (token == "chest") {
        return WorldPropCategory::Chest;
    }
    if (token == "mushroom") {
        return WorldPropCategory::Mushroom;
    }
    return WorldPropCategory::Count;
}

[[nodiscard]] std::string trim(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

} // namespace

bool isWorldPropEntity(const gameplay::EntityKind kind) noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENV_TREE:
    case gameplay::EntityKind::ENV_BUSH:
    case gameplay::EntityKind::ENV_ROCK:
    case gameplay::EntityKind::ENV_CHEST:
    case gameplay::EntityKind::ENV_HOUSE:
    case gameplay::EntityKind::ENV_MUSHROOM:
        return true;
    case gameplay::EntityKind::PLAYER:
    case gameplay::EntityKind::ENEMY_MOB:
    case gameplay::EntityKind::ENEMY_BOSS:
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return false;
    }
    return false;
}

WorldPropCategory worldPropCategoryFor(const gameplay::EntityKind kind) noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENV_TREE:
        return WorldPropCategory::Tree;
    case gameplay::EntityKind::ENV_BUSH:
        return WorldPropCategory::Bush;
    case gameplay::EntityKind::ENV_ROCK:
        return WorldPropCategory::Rock;
    case gameplay::EntityKind::ENV_CHEST:
        return WorldPropCategory::Chest;
    case gameplay::EntityKind::ENV_HOUSE:
        return WorldPropCategory::House;
    case gameplay::EntityKind::ENV_MUSHROOM:
        return WorldPropCategory::Mushroom;
    case gameplay::EntityKind::PLAYER:
    case gameplay::EntityKind::ENEMY_MOB:
    case gameplay::EntityKind::ENEMY_BOSS:
    case gameplay::EntityKind::NPC_BLACKSMITH:
        break;
    }
    return WorldPropCategory::Count;
}

bool WorldPropAssets::load(const std::string& worldDirectory) {
    loaded_ = false;
    for (std::vector<PropEntry>& entries : catalog_) {
        entries.clear();
    }

    const std::string root =
        worldDirectory.empty() || worldDirectory.back() == '/' ? worldDirectory : worldDirectory + '/';
    const std::string manifestPath = root + "world_props.csv";

    std::ifstream manifest(manifestPath);
    if (!manifest.is_open()) {
        return false;
    }

    std::string line;
    if (!std::getline(manifest, line)) {
        return false;
    }

    while (std::getline(manifest, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }

        std::stringstream stream(line);
        std::string categoryToken;
        std::string slugToken;
        std::string fileName;
        std::string worldHeightToken;
        std::string pickRadiusToken;

        if (!std::getline(stream, categoryToken, ',') || !std::getline(stream, slugToken, ',') ||
            !std::getline(stream, fileName, ',') || !std::getline(stream, worldHeightToken, ',') ||
            !std::getline(stream, pickRadiusToken, ',')) {
            continue;
        }

        categoryToken = trim(categoryToken);
        fileName = trim(fileName);
        const WorldPropCategory category = categoryFromToken(categoryToken);
        if (category == WorldPropCategory::Count) {
            continue;
        }

        PropEntry entry{};
        if (!entry.texture.loadFromFile(root + fileName, true)) {
            continue;
        }

        entry.worldHeight = std::stof(trim(worldHeightToken));
        entry.pickRadius = std::stof(trim(pickRadiusToken));
        catalog_[categoryIndex(category)].push_back(std::move(entry));
    }

    loaded_ = false;
    for (const std::vector<PropEntry>& entries : catalog_) {
        if (!entries.empty()) {
            loaded_ = true;
            break;
        }
    }

    return loaded_;
}

const std::vector<WorldPropAssets::PropEntry>& WorldPropAssets::entriesFor(
    const gameplay::EntityKind kind) const noexcept {
    const WorldPropCategory category = worldPropCategoryFor(kind);
    if (category == WorldPropCategory::Count) {
        static const std::vector<PropEntry> kEmpty{};
        return kEmpty;
    }
    return catalog_[categoryIndex(category)];
}

bool WorldPropAssets::hasProps(const gameplay::EntityKind kind) const noexcept {
    return loaded_ && isWorldPropEntity(kind) && !entriesFor(kind).empty();
}

std::size_t WorldPropAssets::variantCount(const gameplay::EntityKind kind) const noexcept {
    return entriesFor(kind).size();
}

const Texture* WorldPropAssets::texture(
    const gameplay::EntityKind kind,
    const std::uint8_t variant) const noexcept {
    const std::vector<PropEntry>& entries = entriesFor(kind);
    if (entries.empty()) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(variant) % entries.size();
    return &entries[index].texture;
}

float WorldPropAssets::worldHeight(
    const gameplay::EntityKind kind,
    const std::uint8_t variant) const noexcept {
    const std::vector<PropEntry>& entries = entriesFor(kind);
    if (entries.empty()) {
        return 1.0F;
    }
    const std::size_t index = static_cast<std::size_t>(variant) % entries.size();
    return entries[index].worldHeight;
}

float WorldPropAssets::pickRadius(
    const gameplay::EntityKind kind,
    const std::uint8_t variant) const noexcept {
    const std::vector<PropEntry>& entries = entriesFor(kind);
    if (entries.empty()) {
        return 1.0F;
    }
    const std::size_t index = static_cast<std::size_t>(variant) % entries.size();
    return entries[index].pickRadius;
}

} // namespace render
