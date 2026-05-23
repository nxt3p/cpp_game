#pragma once

#include "engine/GlBindings.hpp"

namespace render {

class Mesh {
public:
    Mesh();
    explicit Mesh(unsigned int vao, unsigned int vbo, int vertexCount, GLenum drawMode);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    static Mesh createUnitCube();
    static Mesh createUnitCubeWireframe();

    void draw() const;

private:
    unsigned int vao_{0};
    unsigned int vbo_{0};
    int vertexCount_{0};
    GLenum drawMode_{GL_TRIANGLES};
};

} // namespace render
