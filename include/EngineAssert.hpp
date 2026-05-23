#pragma once

#include "engine/GlBindings.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace engine {

inline std::string glErrorString(GLenum error) {
    switch (error) {
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default:
        return "UNKNOWN_GL_ERROR";
    }
}

inline void assertNoGlError(const char* file, int line) {
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return;
    }

    const std::string message =
        std::string("OpenGL error at ") + file + ":" + std::to_string(line) + " -> " +
        glErrorString(error);
    throw std::runtime_error(message);
}

} // namespace engine

#define ENGINE_GL_CHECK() ::engine::assertNoGlError(__FILE__, __LINE__)
