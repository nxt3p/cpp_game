#include "gameplay/IsometricCamera.hpp"

#include <algorithm>

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

CameraMatrices IsometricCamera::matricesForTarget(const Vec3& target) const {
    CameraMatrices result{};
    result.eye = eyePositionForTarget(target);
    const glm::vec3 center = toGlm(target);
    result.view = glm::lookAt(result.eye, center, glm::vec3(0.0F, 1.0F, 0.0F));

    const float aspect =
        static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
    result.projection =
        glm::perspective(glm::radians(fieldOfViewDegrees_), aspect, nearPlane_, farPlane_);
    return result;
}

glm::vec3 IsometricCamera::eyePositionForTarget(const Vec3& target) const noexcept {
    const glm::vec3 center = toGlm(target);
    return center + followOffset_;
}

} // namespace gameplay
