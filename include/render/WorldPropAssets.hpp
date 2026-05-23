#pragma once

#include "gameplay/GameTypes.hpp"
#include "render/Texture.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace render {

enum class WorldPropCategory : std::uint8_t {
    Tree = 0,
    Bush,
    Rock,
    House,
    Chest,
    Mushroom,
    Count
};

[[nodiscard]] bool isWorldPropEntity(gameplay::EntityKind kind) noexcept;

[[nodiscard]] WorldPropCategory worldPropCategoryFor(gameplay::EntityKind kind) noexcept;

class WorldPropAssets {
public:
    [[nodiscard]] bool load(const std::string& worldDirectory);
    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    [[nodiscard]] bool hasProps(gameplay::EntityKind kind) const noexcept;
    [[nodiscard]] std::size_t variantCount(gameplay::EntityKind kind) const noexcept;

    [[nodiscard]] const Texture* texture(
        gameplay::EntityKind kind,
        std::uint8_t variant) const noexcept;

    [[nodiscard]] float worldHeight(
        gameplay::EntityKind kind,
        std::uint8_t variant) const noexcept;

    [[nodiscard]] float pickRadius(
        gameplay::EntityKind kind,
        std::uint8_t variant) const noexcept;

private:
    struct PropEntry {
        Texture texture;
        float worldHeight{1.0F};
        float pickRadius{1.0F};
    };

    [[nodiscard]] const std::vector<PropEntry>& entriesFor(gameplay::EntityKind kind) const noexcept;

    bool loaded_{false};
    std::array<std::vector<PropEntry>, static_cast<std::size_t>(WorldPropCategory::Count)> catalog_{};
};

} // namespace render
