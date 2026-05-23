#pragma once

#include "gameplay/GameTypes.hpp"
#include "render/SpriteSheet.hpp"
#include "render/Texture.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace game {
enum class CharacterClass : std::uint8_t;
}

namespace render {

/// Loads mob/enemy legacy sprites and class sprite sheets (warrior/ranger/mage).
class MobAssets {
public:
    [[nodiscard]] bool load(const std::string& mobsDirectory);
    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }
    [[nodiscard]] bool hasClassSheets() const noexcept { return classSheetsLoaded_; }

    [[nodiscard]] bool isSpriteEntity(gameplay::EntityKind kind) const noexcept;

    [[nodiscard]] SpriteFrameSample sampleMobSprite(
        gameplay::EntityKind kind,
        std::uint32_t entityId,
        bool useIdlePose,
        SpriteClip animation,
        SpriteFacing facing,
        float elapsedSeconds) const noexcept;

    [[nodiscard]] const SpriteSheet& classSheet(game::CharacterClass playerClass) const noexcept;

    [[nodiscard]] SpriteFrameSample sampleClassSprite(
        game::CharacterClass playerClass,
        SpriteClip animation,
        SpriteFacing facing,
        float elapsedSeconds) const noexcept;

    [[nodiscard]] float spriteWorldHeight(gameplay::EntityKind kind) const noexcept;

private:
    [[nodiscard]] std::size_t mobSheetIndex(
        gameplay::EntityKind kind,
        std::uint32_t entityId) const noexcept;

    [[nodiscard]] std::size_t classSheetIndex(game::CharacterClass playerClass) const noexcept;

    bool loaded_{false};
    bool classSheetsLoaded_{false};
    std::array<SpriteSheet, 6> mobActionSheets_{};
    std::array<SpriteSheet, 6> mobIdleSheets_{};
    std::array<SpriteSheet, 3> classSheets_{};
};

} // namespace render
