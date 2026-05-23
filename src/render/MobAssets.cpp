#include "render/MobAssets.hpp"

#include "game/AppFlow.hpp"

namespace render {

namespace {

constexpr int kMobFrameSize = 72;

bool loadMobSheet(SpriteSheet& sheet, const std::string& path) {
    return sheet.loadFromFile(path, kMobFrameSize, kMobFrameSize);
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
    if (!classSheetsLoaded_) {
        return {};
    }
    return classSheets_[classSheetIndex(playerClass)].sample(animation, facing, elapsedSeconds);
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
    const SpriteSheet& sheet = useIdlePose ? mobIdleSheets_[index] : mobActionSheets_[index];
    if (!sheet.isValid()) {
        return {};
    }
    return sheet.sampleMob(animation, facing, elapsedSeconds);
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
