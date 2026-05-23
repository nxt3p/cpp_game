#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace {

constexpr float kEpsilon = 1e-4F;

bool matricesNear(const glm::mat4& lhs, const glm::mat4& rhs, float epsilon) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (std::fabs(lhs[column][row] - rhs[column][row]) > epsilon) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

TEST_CASE("GLM lookAt maps target to origin in view space", "[math][glm]") {
    const glm::vec3 eye(0.0F, 2.0F, 5.0F);
    const glm::vec3 center(0.0F, 0.0F, 0.0F);
    const glm::vec3 up(0.0F, 1.0F, 0.0F);

    const glm::mat4 view = glm::lookAt(eye, center, up);
    const glm::vec4 targetInView = view * glm::vec4(center, 1.0F);
    const float distance = glm::length(eye - center);

    CHECK(targetInView.x == Catch::Approx(0.0F).margin(kEpsilon));
    CHECK(targetInView.y == Catch::Approx(0.0F).margin(kEpsilon));
    CHECK(targetInView.z == Catch::Approx(-distance).margin(kEpsilon));
    CHECK(targetInView.w == Catch::Approx(1.0F).margin(kEpsilon));
}

TEST_CASE("GLM perspective maps frustum corners consistently", "[math][glm]") {
    const float fovY = glm::radians(45.0F);
    const float aspect = 16.0F / 9.0F;
    const float nearPlane = 0.1F;
    const float farPlane = 100.0F;

    const glm::mat4 projection = glm::perspective(fovY, aspect, nearPlane, farPlane);

    const float tanHalfFov = std::tan(fovY * 0.5F);
    const float expectedM00 = 1.0F / (aspect * tanHalfFov);
    const float expectedM11 = 1.0F / tanHalfFov;

    const float expectedDepth =
        -(2.0F * farPlane * nearPlane) / (farPlane - nearPlane);

    CHECK(projection[0][0] == Catch::Approx(expectedM00).margin(kEpsilon));
    CHECK(projection[1][1] == Catch::Approx(expectedM11).margin(kEpsilon));
    CHECK(projection[2][3] == Catch::Approx(-1.0F).margin(kEpsilon));
    CHECK(projection[3][2] == Catch::Approx(expectedDepth).margin(kEpsilon));
}

TEST_CASE("View-projection composition remains invertible", "[math][glm]") {
    const glm::mat4 view =
        glm::lookAt(glm::vec3(2.0F, 2.0F, 4.0F), glm::vec3(0.0F), glm::vec3(0.0F, 1.0F, 0.0F));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0F), 1.0F, 0.1F, 50.0F);
    const glm::mat4 viewProjection = projection * view;

    const glm::mat4 inverse = glm::inverse(viewProjection);
    const glm::mat4 identity = viewProjection * inverse;

    const glm::mat4 expected = glm::mat4(1.0F);
    CHECK(matricesNear(identity, expected, 2e-3F));
}
