#pragma once

#include "gameplay/GameTypes.hpp"

#include <glm/glm.hpp>

namespace gameplay {

struct CameraMatrices {
    glm::mat4 view{1.0F};
    glm::mat4 projection{1.0F};
    glm::vec3 eye{0.0F};
};

class IsometricCamera {
public:
    IsometricCamera();

    void setViewportSize(int width, int height);
    void setFollowOffset(const glm::vec3& offset) noexcept;

    [[nodiscard]] CameraMatrices matricesForTarget(const Vec3& target) const;
    [[nodiscard]] glm::vec3 eyePositionForTarget(const Vec3& target) const noexcept;

private:
    glm::vec3 followOffset_{12.0F, 18.0F, 12.0F};
    float fieldOfViewDegrees_{45.0F};
    float nearPlane_{0.1F};
    float farPlane_{500.0F};
    int viewportWidth_{1280};
    int viewportHeight_{720};
};

} // namespace gameplay
