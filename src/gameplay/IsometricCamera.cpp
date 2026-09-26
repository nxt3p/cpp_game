#include "gameplay/IsometricCamera.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace gameplay {

namespace {

glm::vec3 toGlm(const Vec3& value) {
    return glm::vec3(value.x, value.y, value.z);
}

} // namespace

IsometricCamera::IsometricCamera() = default;

void IsometricCamera::setViewportSize(int width, int height) {
    viewportWidth_ = std::max(width, 1);
    viewportHeight_ = std::max(height, 1);
}

void IsometricCamera::setFollowOffset(const glm::vec3& offset) noexcept {
    followOffset_ = offset;
}

void IsometricCamera::setOrthoHeight(const float height) noexcept {
    orthoHeight_ = std::max(height, 8.0F);
}

CameraMatrices IsometricCamera::matricesForTarget(const Vec3& target) const {
    CameraMatrices result{};
    result.eye = eyePositionForTarget(target);
    const glm::vec3 center = toGlm(target);
    result.view = glm::lookAt(result.eye, center, glm::vec3(0.0F, 1.0F, 0.0F));

    const float aspect =
        static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
    const float halfHeight = orthoHeight_ * 0.5F;
    const float halfWidth = halfHeight * aspect;
    result.projection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane_, farPlane_);
    return result;
}

namespace {

[[nodiscard]] glm::vec3 unprojectNdc(
    const float ndcX,
    const float ndcY,
    const float ndcZ,
    const glm::mat4& view,
    const glm::mat4& projection) {
    glm::vec4 world = glm::inverse(projection) * glm::vec4(ndcX, ndcY, ndcZ, 1.0F);
    world = glm::inverse(view) * world;
    if (std::abs(world.w) > 1.0e-6F) {
        world /= world.w;
    }
    return glm::vec3(world);
}

} // namespace

PointerRay pointerRayFromScreen(
    const float mouseX,
    const float mouseY,
    const int screenWidth,
    const int screenHeight,
    const CameraMatrices& camera) {
    PointerRay ray{};
    if (screenWidth <= 0 || screenHeight <= 0) {
        return ray;
    }

    const float ndcX = (2.0F * mouseX) / static_cast<float>(screenWidth) - 1.0F;
    const float ndcY = 1.0F - (2.0F * mouseY) / static_cast<float>(screenHeight);
    const glm::vec3 nearPoint = unprojectNdc(ndcX, ndcY, -1.0F, camera.view, camera.projection);
    const glm::vec3 farPoint = unprojectNdc(ndcX, ndcY, 1.0F, camera.view, camera.projection);
    const glm::vec3 delta = farPoint - nearPoint;
    if (glm::dot(delta, delta) < 1.0e-8F) {
        return ray;
    }

    ray.origin = nearPoint;
    ray.direction = glm::normalize(delta);
    ray.valid = true;
    return ray;
}

bool screenPointToGround(
    const float mouseX,
    const float mouseY,
    const int screenWidth,
    const int screenHeight,
    const CameraMatrices& camera,
    glm::vec3& outPoint) {
    const PointerRay ray = pointerRayFromScreen(mouseX, mouseY, screenWidth, screenHeight, camera);
    if (!ray.valid || std::abs(ray.direction.y) < 1.0e-5F) {
        return false;
    }

    const float distance = -ray.origin.y / ray.direction.y;
    if (distance < 0.0F) {
        return false;
    }

    outPoint = ray.origin + ray.direction * distance;
    outPoint.y = 0.0F;
    return true;
}

GroundAabb visibleGroundAabb(
    const CameraMatrices& camera,
    const int screenWidth,
    const int screenHeight,
    const glm::vec3& focus,
    const float margin) noexcept {
    GroundAabb bounds{focus.x, focus.x, focus.z, focus.z};
    if (screenWidth > 1 && screenHeight > 1) {
        const float sampleX[3] = {0.0F, static_cast<float>(screenWidth) * 0.5F, static_cast<float>(screenWidth - 1)};
        const float sampleY[3] = {0.0F, static_cast<float>(screenHeight) * 0.5F, static_cast<float>(screenHeight - 1)};
        for (const float sx : sampleX) {
            for (const float sy : sampleY) {
                glm::vec3 ground{};
                if (!screenPointToGround(sx, sy, screenWidth, screenHeight, camera, ground)) {
                    continue;
                }
                bounds.minX = std::min(bounds.minX, ground.x);
                bounds.maxX = std::max(bounds.maxX, ground.x);
                bounds.minZ = std::min(bounds.minZ, ground.z);
                bounds.maxZ = std::max(bounds.maxZ, ground.z);
            }
        }
    }

    // A failed unproject must not collapse the cull box onto the player.
    if (bounds.maxX - bounds.minX < 8.0F) {
        bounds.minX = focus.x - kIsometricOrthoHeight;
        bounds.maxX = focus.x + kIsometricOrthoHeight;
    }
    if (bounds.maxZ - bounds.minZ < 8.0F) {
        bounds.minZ = focus.z - kIsometricOrthoHeight;
        bounds.maxZ = focus.z + kIsometricOrthoHeight;
    }

    const float pad = std::max(0.0F, margin);
    bounds.minX -= pad;
    bounds.maxX += pad;
    bounds.minZ -= pad;
    bounds.maxZ += pad;
    return bounds;
}

glm::vec3 IsometricCamera::eyePositionForTarget(const Vec3& target) const noexcept {
    const glm::vec3 center = toGlm(target);
    return center + followOffset_;
}

} // namespace gameplay
