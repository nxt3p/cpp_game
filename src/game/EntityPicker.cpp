#include "game/EntityPicker.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {

namespace {

glm::vec3 toGlm(const gameplay::Vec3& value) {
    return glm::vec3(value.x, value.y, value.z);
}

bool rayIntersectsSphere(
    const ScreenRay& ray,
    const glm::vec3& center,
    float radius,
    float& outDistance) {
    const glm::vec3 originToCenter = ray.origin - center;
    const float b = glm::dot(originToCenter, ray.direction);
    const float c = glm::dot(originToCenter, originToCenter) - radius * radius;
    const float discriminant = (b * b) - c;

    if (discriminant < 0.0F) {
        return false;
    }

    const float sqrtDiscriminant = std::sqrt(discriminant);
    float nearest = -b - sqrtDiscriminant;
    if (nearest < 0.0F) {
        nearest = -b + sqrtDiscriminant;
    }

    if (nearest < 0.0F) {
        return false;
    }

    outDistance = nearest;
    return true;
}

} // namespace

bool isAttackableEntity(gameplay::EntityKind kind) noexcept {
    return kind == gameplay::EntityKind::ENEMY_MOB || kind == gameplay::EntityKind::ENEMY_BOSS;
}

bool isInteractableEntity(gameplay::EntityKind kind) noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENEMY_MOB:
    case gameplay::EntityKind::ENEMY_BOSS:
    case gameplay::EntityKind::NPC_BLACKSMITH:
    case gameplay::EntityKind::ENV_CHEST:
    case gameplay::EntityKind::ENV_ROCK:
        return true;
    case gameplay::EntityKind::PLAYER:
    case gameplay::EntityKind::ENV_TREE:
    case gameplay::EntityKind::ENV_BUSH:
    case gameplay::EntityKind::ENV_HOUSE:
    case gameplay::EntityKind::ENV_MUSHROOM:
        return false;
    }
    return false;
}

float entityPickRadius(gameplay::EntityKind kind) noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENEMY_MOB:
        return 1.1F;
    case gameplay::EntityKind::ENEMY_BOSS:
        return 2.4F;
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return 1.5F;
    case gameplay::EntityKind::ENV_CHEST:
        return 0.9F;
    case gameplay::EntityKind::ENV_ROCK:
        return 1.2F;
    case gameplay::EntityKind::ENV_TREE:
        return 1.4F;
    case gameplay::EntityKind::ENV_BUSH:
        return 0.9F;
    case gameplay::EntityKind::ENV_HOUSE:
        return 2.8F;
    case gameplay::EntityKind::ENV_MUSHROOM:
        return 0.45F;
    default:
        return 0.8F;
    }
}

const char* interactableHint(gameplay::EntityKind kind) noexcept {
    switch (kind) {
    case gameplay::EntityKind::ENEMY_MOB:
        return "Mob [click]";
    case gameplay::EntityKind::ENEMY_BOSS:
        return "Boss [click]";
    case gameplay::EntityKind::NPC_BLACKSMITH:
        return "Blacksmith [E]";
    case gameplay::EntityKind::ENV_CHEST:
        return "Chest [click]";
    case gameplay::EntityKind::ENV_ROCK:
        return "Rock [click]";
    default:
        return "";
    }
}

ScreenRay buildScreenRay(
    float mouseX,
    float mouseY,
    int screenWidth,
    int screenHeight,
    const gameplay::CameraMatrices& cameraMatrices) {
    const float normalizedX = (2.0F * mouseX) / static_cast<float>(screenWidth) - 1.0F;
    const float normalizedY = 1.0F - (2.0F * mouseY) / static_cast<float>(screenHeight);

    glm::vec4 rayClip(normalizedX, normalizedY, -1.0F, 1.0F);
    glm::vec4 rayEye = glm::inverse(cameraMatrices.projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0F, 0.0F);
    const glm::vec4 rayWorld = glm::inverse(cameraMatrices.view) * rayEye;

    ScreenRay ray{};
    ray.origin = cameraMatrices.eye;
    ray.direction = glm::normalize(glm::vec3(rayWorld));
    return ray;
}

std::optional<std::uint32_t> pickInteractableEntity(
    const ScreenRay& ray,
    const std::vector<gameplay::WorldEntitySnapshot>& entities) {
    std::optional<std::uint32_t> bestId;
    float bestDistance = std::numeric_limits<float>::max();

    for (const gameplay::WorldEntitySnapshot& entity : entities) {
        if (!entity.active || !isInteractableEntity(entity.kind)) {
            continue;
        }

        const glm::vec3 center = toGlm(entity.position);
        const float radius = entityPickRadius(entity.kind);
        float hitDistance = 0.0F;
        if (!rayIntersectsSphere(ray, center, radius, hitDistance)) {
            continue;
        }

        if (hitDistance < bestDistance) {
            bestDistance = hitDistance;
            bestId = entity.id;
        }
    }

    return bestId;
}

} // namespace game
