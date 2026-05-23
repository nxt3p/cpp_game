#pragma once

#include "gameplay/GameTypes.hpp"
#include "gameplay/IsometricCamera.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace game {

struct ScreenRay {
    glm::vec3 origin{0.0F};
    glm::vec3 direction{0.0F};
};

[[nodiscard]] bool isInteractableEntity(gameplay::EntityKind kind) noexcept;
[[nodiscard]] bool isAttackableEntity(gameplay::EntityKind kind) noexcept;
[[nodiscard]] float entityPickRadius(gameplay::EntityKind kind) noexcept;
[[nodiscard]] const char* interactableHint(gameplay::EntityKind kind) noexcept;

[[nodiscard]] ScreenRay buildScreenRay(
    float mouseX,
    float mouseY,
    int screenWidth,
    int screenHeight,
    const gameplay::CameraMatrices& cameraMatrices);

[[nodiscard]] std::optional<std::uint32_t> pickInteractableEntity(
    const ScreenRay& ray,
    const std::vector<gameplay::WorldEntitySnapshot>& entities);

} // namespace game
