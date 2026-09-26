#pragma once

#include "gameplay/GameTypes.hpp"

#include <glm/glm.hpp>

namespace gameplay {

/// Eye sits on the isometric diagonal. Framing comes from the orthographic height, not perspective FOV.
inline const glm::vec3 kIsometricEyeOffset = glm::normalize(glm::vec3(1.0F, 1.0F, 1.0F)) * 48.0F;

/// Visible world height of the orthographic view at the reference aspect.
inline constexpr float kIsometricOrthoHeight = 36.0F;

struct CameraMatrices {
    glm::mat4 view{1.0F};
    glm::mat4 projection{1.0F};
    glm::vec3 eye{0.0F};
};

struct PointerRay {
    glm::vec3 origin{0.0F};
    glm::vec3 direction{0.0F, -1.0F, 0.0F};
    bool valid{false};
};

/// Unprojects the screen point through the near and far planes. Works for orthographic and perspective.
[[nodiscard]] PointerRay pointerRayFromScreen(
    float mouseX,
    float mouseY,
    int screenWidth,
    int screenHeight,
    const CameraMatrices& camera);

/// Intersects `pointerRayFromScreen` with the ground plane y = 0.
[[nodiscard]] bool screenPointToGround(
    float mouseX,
    float mouseY,
    int screenWidth,
    int screenHeight,
    const CameraMatrices& camera,
    glm::vec3& outPoint);

/// View-space z of a sprite's ground contact. Smaller (more negative) values are farther from the camera.
[[nodiscard]] inline float isometricSortKey(const glm::mat4& view, const glm::vec3& groundContact) noexcept {
    const glm::vec4 viewPosition = view * glm::vec4(groundContact.x, 0.0F, groundContact.z, 1.0F);
    return viewPosition.z;
}

/// Axis-aligned patch of the ground plane covered by the current view, plus `margin`.
struct GroundAabb {
    float minX{0.0F};
    float maxX{0.0F};
    float minZ{0.0F};
    float maxZ{0.0F};

    [[nodiscard]] bool contains(const float x, const float z) const noexcept {
        return x >= minX && x <= maxX && z >= minZ && z <= maxZ;
    }
};

/// Ground footprint of the orthographic view. Entities outside this box are off screen.
[[nodiscard]] GroundAabb visibleGroundAabb(
    const CameraMatrices& camera,
    int screenWidth,
    int screenHeight,
    const glm::vec3& focus,
    float margin) noexcept;

class IsometricCamera {
public:
    IsometricCamera();

    void setViewportSize(int width, int height);
    void setFollowOffset(const glm::vec3& offset) noexcept;
    void setOrthoHeight(float height) noexcept;

    [[nodiscard]] CameraMatrices matricesForTarget(const Vec3& target) const;
    [[nodiscard]] glm::vec3 eyePositionForTarget(const Vec3& target) const noexcept;

private:
    glm::vec3 followOffset_{kIsometricEyeOffset};
    float orthoHeight_{kIsometricOrthoHeight};
    float nearPlane_{0.1F};
    float farPlane_{500.0F};
    int viewportWidth_{1280};
    int viewportHeight_{720};
};

} // namespace gameplay
