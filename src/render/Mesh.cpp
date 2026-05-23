#include "render/Mesh.hpp"

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"

#include <array>
#include <utility>

namespace render {

namespace {

struct Vertex {
    float px;
    float py;
    float pz;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

const std::array<Vertex, 36> kCubeVertices = {{
    {-0.5F, -0.5F, -0.5F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F},
    {0.5F, -0.5F, -0.5F, 0.0F, 0.0F, -1.0F, 1.0F, 0.0F},
    {0.5F, 0.5F, -0.5F, 0.0F, 0.0F, -1.0F, 1.0F, 1.0F},
    {0.5F, 0.5F, -0.5F, 0.0F, 0.0F, -1.0F, 1.0F, 1.0F},
    {-0.5F, 0.5F, -0.5F, 0.0F, 0.0F, -1.0F, 0.0F, 1.0F},
    {-0.5F, -0.5F, -0.5F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F},

    {-0.5F, -0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F},
    {0.5F, -0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F},
    {0.5F, 0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F},
    {0.5F, 0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F},
    {-0.5F, 0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F},
    {-0.5F, -0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F},

    {-0.5F, 0.5F, 0.5F, -1.0F, 0.0F, 0.0F, 1.0F, 0.0F},
    {-0.5F, 0.5F, -0.5F, -1.0F, 0.0F, 0.0F, 1.0F, 1.0F},
    {-0.5F, -0.5F, -0.5F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {-0.5F, -0.5F, -0.5F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {-0.5F, -0.5F, 0.5F, -1.0F, 0.0F, 0.0F, 0.0F, 0.0F},
    {-0.5F, 0.5F, 0.5F, -1.0F, 0.0F, 0.0F, 1.0F, 0.0F},

    {0.5F, 0.5F, 0.5F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F},
    {0.5F, 0.5F, -0.5F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F},
    {0.5F, -0.5F, -0.5F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {0.5F, -0.5F, -0.5F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    {0.5F, -0.5F, 0.5F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F},
    {0.5F, 0.5F, 0.5F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F},

    {-0.5F, -0.5F, -0.5F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F},
    {0.5F, -0.5F, -0.5F, 0.0F, -1.0F, 0.0F, 1.0F, 1.0F},
    {0.5F, -0.5F, 0.5F, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F},
    {0.5F, -0.5F, 0.5F, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F},
    {-0.5F, -0.5F, 0.5F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F},
    {-0.5F, -0.5F, -0.5F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F},

    {-0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
    {0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F},
    {0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F},
    {0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F},
    {-0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
    {-0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
}};

} // namespace

Mesh::Mesh() = default;

Mesh::Mesh(unsigned int vao, unsigned int vbo, int vertexCount, const GLenum drawMode)
    : vao_(vao), vbo_(vbo), vertexCount_(vertexCount), drawMode_(drawMode) {}

Mesh::~Mesh() {
    if (vbo_ != 0U) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0U) {
        glDeleteVertexArrays(1, &vao_);
    }
}

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_),
      vbo_(other.vbo_),
      vertexCount_(other.vertexCount_),
      drawMode_(other.drawMode_) {
    other.vao_ = 0U;
    other.vbo_ = 0U;
    other.vertexCount_ = 0;
    other.drawMode_ = GL_TRIANGLES;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        this->~Mesh();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        vertexCount_ = other.vertexCount_;
        drawMode_ = other.drawMode_;
        other.vao_ = 0U;
        other.vbo_ = 0U;
        other.vertexCount_ = 0;
        other.drawMode_ = GL_TRIANGLES;
    }
    return *this;
}

Mesh Mesh::createUnitCube() {
    unsigned int vao = 0U;
    unsigned int vbo = 0U;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(kCubeVertices.size() * sizeof(Vertex)),
        kCubeVertices.data(),
        GL_STATIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    ENGINE_GL_CHECK();

    return Mesh(vao, vbo, static_cast<int>(kCubeVertices.size()), GL_TRIANGLES);
}

Mesh Mesh::createUnitCubeWireframe() {
    const std::array<Vertex, 24> wireVertices = {{
        {-0.5F, -0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, -0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, -0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, -0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},

        {-0.5F, -0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, -0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, -0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, -0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},

        {-0.5F, -0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, -0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, -0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, -0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, 0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {-0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
    }};

    unsigned int vao = 0U;
    unsigned int vbo = 0U;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(wireVertices.size() * sizeof(Vertex)),
        wireVertices.data(),
        GL_STATIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));

    glBindVertexArray(0);
    ENGINE_GL_CHECK();

    return Mesh(vao, vbo, static_cast<int>(wireVertices.size()), GL_LINES);
}

void Mesh::draw() const {
    if (vao_ == 0U || vertexCount_ <= 0) {
        return;
    }

    glBindVertexArray(vao_);
    glDrawArrays(drawMode_, 0, vertexCount_);
    glBindVertexArray(0);
    ENGINE_GL_CHECK();
}

} // namespace render
