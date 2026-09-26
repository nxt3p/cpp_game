#include "render/MobAssets.hpp"

#include "game/AppFlow.hpp"
#include "render/AtlasMetadata.hpp"

namespace render {

namespace {

constexpr int kMobFrameSize = 72;

bool loadMobSheet(SpriteSheet& sheet, const std::string& path) {
    return sheet.loadFromFile(path, kMobFrameSize, kMobFrameSize);
}

[[nodiscard]] std::string generatedDirectory(std::string mobsDirectory) {
    while (!mobsDirectory.empty() && (mobsDirectory.back() == '/' || mobsDirectory.back() == '\\')) {
        mobsDirectory.pop_back();
    }
    const std::size_t slash = mobsDirectory.find_last_of("/\\");
    const std::string parent = slash == std::string::npos ? std::string{} : mobsDirectory.substr(0, slash);
    return parent.empty() ? std::string("generated") : parent + "/generated";
}

[[nodiscard]] bool loadDirectionalSheet(SpriteSheet& sheet, const std::string& pngPath, const std::string& jsonPath) {
    DirectionalAtlas atlas;
    if (!atlas.loadFromFile(jsonPath)) {
        return false;
    }
    if (!sheet.loadFromFile(pngPath, atlas.frameWidth, atlas.frameHeight)) {
        return false;
    }
    sheet.bindDirectionalClips(std::move(atlas));
    return sheet.hasDirectionalClips();
}

} // namespace

bool MobAssets::load(const std::string& mobsDirectory) {
    loaded_ = false;
    classSheetsLoaded_ = false;

    const std::string root =
        mobsDirectory.empty() || mobsDirectory.back() == '/' ? mobsDirectory : mobsDirectory + '/';

    for (int index = 1; index <= 6; ++index) {
        const std::string prefix = root + "Char_00" + std::to_string(index);
        const std::size_t sheetIndex = static_cast<std::size_t>(index - 1);
        if (!loadMobSheet(mobActionSheets_[sheetIndex], prefix + ".png") ||
            !loadMobSheet(mobIdleSheets_[sheetIndex], prefix + "_Idle.png")) {
            return false;
        }
    }

    classSheetsLoaded_ = classSheets_[0].loadFromFile(root + "warrior.png") &&
                         classSheets_[1].loadFromFile(root + "ranger.png") &&
                         classSheets_[2].loadFromFile(root + "mage.png");

    const std::string generated = generatedDirectory(root);
    SpriteSheet generatedWarrior;
    if (loadDirectionalSheet(generatedWarrior, generated + "/warrior_atlas.png", generated + "/warrior_atlas.json")) {
        classSheets_[0] = std::move(generatedWarrior);
        classSheetsLoaded_ = classSheets_[1].isValid() && classSheets_[2].isValid();
    }

    SpriteSheet generatedMonster;
    generatedMonster_ = loadDirectionalSheet(
        generatedMonster, generated + "/monster_atlas.png", generated + "/monster_atlas.json");
    if (generatedMonster_) {
        mobActionSheets_[0] = std::move(generatedMonster);
    }

    loaded_ = true;
    return true;
}

bool MobAssets::isSpriteEntity(const gameplay::EntityKind kind) const noexcept {
    switch (kind) {
    case gameplay::EntityKind::PLAYER:
    case gameplay::EntityKind::ENEMY_MOB:
    case gameplay::EntityKind::ENEMY_BOSS:
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return true;
    case gameplay::EntityKind::ENV_TREE:
    case gameplay::EntityKind::ENV_BUSH:
    case gameplay::EntityKind::ENV_CHEST:
    case gameplay::EntityKind::ENV_ROCK:
    case gameplay::EntityKind::ENV_HOUSE:
    case gameplay::EntityKind::ENV_MUSHROOM:
        return false;
    }
    return false;
}

std::size_t MobAssets::mobSheetIndex(
    const gameplay::EntityKind kind,
    const std::uint32_t entityId) const noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENEMY_MOB:
        return static_cast<std::size_t>(entityId % 3U);
    case gameplay::EntityKind::ENEMY_BOSS:
        return 4U;
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return 3U;
    case gameplay::EntityKind::PLAYER:
        return 0U;
    case gameplay::EntityKind::ENV_TREE:
    case gameplay::EntityKind::ENV_BUSH:
    case gameplay::EntityKind::ENV_CHEST:
    case gameplay::EntityKind::ENV_ROCK:
    case gameplay::EntityKind::ENV_HOUSE:
    case gameplay::EntityKind::ENV_MUSHROOM:
        break;
    }
    return 0U;
}

std::size_t MobAssets::classSheetIndex(const game::CharacterClass playerClass) const noexcept {
    switch (playerClass) {
    case game::CharacterClass::WARRIOR:
        return 0U;
    case game::CharacterClass::RANGER:
        return 1U;
    case game::CharacterClass::MAGE:
        return 2U;
    case game::CharacterClass::NONE:
        return 0U;
    }
    return 0U;
}

const SpriteSheet& MobAssets::classSheet(const game::CharacterClass playerClass) const noexcept {
    return classSheets_[classSheetIndex(playerClass)];
}

SpriteFrameSample MobAssets::sampleClassSprite(
    const game::CharacterClass playerClass,
    const SpriteClip animation,
    const SpriteFacing facing,
    const float elapsedSeconds) const noexcept {
    SpriteFacing8 facing8 = SpriteFacing8::South;
    switch (facing) {
    case SpriteFacing::Up:
        facing8 = SpriteFacing8::North;
        break;
    case SpriteFacing::Left:
        facing8 = SpriteFacing8::West;
        break;
    case SpriteFacing::Right:
        facing8 = SpriteFacing8::East;
        break;
    case SpriteFacing::Down:
        facing8 = SpriteFacing8::South;
        break;
    }
    return sampleClassSprite(playerClass, animation, facing8, elapsedSeconds);
}

SpriteFrameSample MobAssets::sampleClassSprite(
    const game::CharacterClass playerClass,
    const SpriteClip animation,
    const SpriteFacing8 facing,
    const float elapsedSeconds) const noexcept {
    if (!classSheetsLoaded_) {
        return {};
    }
    const SpriteSheet& sheet = classSheets_[classSheetIndex(playerClass)];
    if (sheet.hasDirectionalClips()) {
        const SpriteFrameSample directional =
            sheet.sampleDirectional(animation, static_cast<int>(facing), elapsedSeconds);
        if (directional.texture != nullptr) {
            return directional;
        }
    }
    return sheet.sample(animation, facing4From8(facing), elapsedSeconds);
}

SpriteFrameSample MobAssets::sampleMobSprite(
    const gameplay::EntityKind kind,
    const std::uint32_t entityId,
    const bool useIdlePose,
    const SpriteClip animation,
    const SpriteFacing facing,
    const float elapsedSeconds) const noexcept {
    if (!loaded_) {
        return {};
    }

    const std::size_t index = mobSheetIndex(kind, entityId);
    if (generatedMonster_ && index == 0U && mobActionSheets_[0].hasDirectionalClips()) {
        SpriteFacing8 facing8 = SpriteFacing8::South;
        switch (facing) {
        case SpriteFacing::Up:
            facing8 = SpriteFacing8::North;
            break;
        case SpriteFacing::Left:
            facing8 = SpriteFacing8::West;
            break;
        case SpriteFacing::Right:
            facing8 = SpriteFacing8::East;
            break;
        case SpriteFacing::Down:
            facing8 = SpriteFacing8::South;
            break;
        }
        const SpriteFrameSample directional =
            mobActionSheets_[0].sampleDirectional(animation, static_cast<int>(facing8), elapsedSeconds);
        if (directional.texture != nullptr) {
            return directional;
        }
    }
    const SpriteSheet& sheet = useIdlePose ? mobIdleSheets_[index] : mobActionSheets_[index];
    if (!sheet.isValid()) {
        return {};
    }
    return sheet.sampleMob(animation, facing, elapsedSeconds);
}

SpriteFrameSample MobAssets::sampleMobSprite(
    const gameplay::EntityKind kind,
    const std::uint32_t entityId,
    const bool useIdlePose,
    const SpriteClip animation,
    const SpriteFacing8 facing,
    const float elapsedSeconds) const noexcept {
    if (!loaded_) {
        return {};
    }
    const std::size_t index = mobSheetIndex(kind, entityId);
    if (generatedMonster_ && index == 0U && mobActionSheets_[0].hasDirectionalClips()) {
        const SpriteFrameSample directional =
            mobActionSheets_[0].sampleDirectional(animation, static_cast<int>(facing), elapsedSeconds);
        if (directional.texture != nullptr) {
            return directional;
        }
    }
    return sampleMobSprite(kind, entityId, useIdlePose, animation, facing4From8(facing), elapsedSeconds);
}

float MobAssets::spriteWorldHeight(const gameplay::EntityKind kind) const noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENEMY_BOSS:
        return 4.2F;
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return 2.6F;
    case gameplay::EntityKind::ENEMY_MOB:
        return 2.2F;
    case gameplay::EntityKind::PLAYER:
        return 2.8F;
    case gameplay::EntityKind::ENV_TREE:
    case gameplay::EntityKind::ENV_BUSH:
    case gameplay::EntityKind::ENV_CHEST:
    case gameplay::EntityKind::ENV_ROCK:
    case gameplay::EntityKind::ENV_HOUSE:
    case gameplay::EntityKind::ENV_MUSHROOM:
        return 1.0F;
    }
    return 2.0F;
}

} // namespace render
