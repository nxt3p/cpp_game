#include "Shader.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

#ifdef __EMSCRIPTEN__
std::string stripUniformInitializers(std::string line) {
    const std::size_t uniformPos = line.find("uniform");
    if (uniformPos == std::string::npos) {
        return line;
    }
    const std::size_t equalsPos = line.find('=', uniformPos);
    const std::size_t semicolonPos = line.find(';', uniformPos);
    if (equalsPos != std::string::npos && semicolonPos != std::string::npos && equalsPos < semicolonPos) {
        line = line.substr(0, equalsPos) + line.substr(semicolonPos);
    }
    return line;
}

std::string adaptShaderSourceForWebGl(std::string source) {
    constexpr const char* kDesktopVersion = "#version 330 core";
    const std::size_t versionPosition = source.find(kDesktopVersion);
    if (versionPosition == std::string::npos) {
        return source;
    }

    source.replace(versionPosition, std::char_traits<char>::length(kDesktopVersion), "#version 300 es");
    const std::size_t lineEnd = source.find('\n', versionPosition);
    if (lineEnd != std::string::npos) {
        source.insert(lineEnd + 1, "precision highp float;\nprecision highp int;\n");
    }

    std::ostringstream adapted;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        adapted << stripUniformInitializers(std::move(line)) << '\n';
    }
    return adapted.str();
}
#endif

} // namespace

namespace engine {

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vertexSource = readFile(vertexPath);
    const std::string fragmentSource = readFile(fragmentPath);

    const unsigned int vertex = compileStage(GL_VERTEX_SHADER, vertexSource);
    const unsigned int fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource);

    try {
        linkProgram(vertex, fragment);
    } catch (...) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        throw;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    ENGINE_GL_CHECK();
}

Shader::~Shader() {
    if (programId_ != 0U) {
        glDeleteProgram(programId_);
        programId_ = 0U;
    }
}

Shader::Shader(Shader&& other) noexcept : programId_(other.programId_) {
    other.programId_ = 0U;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (programId_ != 0U) {
            glDeleteProgram(programId_);
        }
        programId_ = other.programId_;
        other.programId_ = 0U;
    }
    return *this;
}

void Shader::use() const {
    glUseProgram(programId_);
    ENGINE_GL_CHECK();
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    glUseProgram(programId_);
    const int location = uniformLocation(name);
    if (location < 0) {
        return;
    }
    glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
    ENGINE_GL_CHECK();
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const {
    glUseProgram(programId_);
    const int location = uniformLocation(name);
    if (location < 0) {
        return;
    }
    glUniform2fv(location, 1, &value[0]);
    ENGINE_GL_CHECK();
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    glUseProgram(programId_);
    const int location = uniformLocation(name);
    if (location < 0) {
        return;
    }
    glUniform3fv(location, 1, &value[0]);
    ENGINE_GL_CHECK();
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    glUseProgram(programId_);
    const int location = uniformLocation(name);
    if (location < 0) {
        return;
    }
    glUniform4fv(location, 1, &value[0]);
    ENGINE_GL_CHECK();
}

void Shader::setFloat(const std::string& name, const float value) const {
    glUseProgram(programId_);
    const int location = uniformLocation(name);
    if (location < 0) {
        return;
    }
    glUniform1f(location, value);
    ENGINE_GL_CHECK();
}

void Shader::setInt(const std::string& name, const int value) const {
    glUseProgram(programId_);
    const int location = uniformLocation(name);
    if (location < 0) {
        return;
    }
    glUniform1i(location, value);
    ENGINE_GL_CHECK();
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string contents = buffer.str();
    if (contents.empty()) {
        throw std::runtime_error("Shader file is empty: " + path);
    }
#ifdef __EMSCRIPTEN__
    return adaptShaderSourceForWebGl(contents);
#else
    return contents;
#endif
}

unsigned int Shader::compileStage(unsigned int type, const std::string& source) {
    const unsigned int shader = glCreateShader(type);
    const char* sourceCStr = source.c_str();
    const GLint sourceLength = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &sourceCStr, &sourceLength);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        const char* stage = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        glDeleteShader(shader);
        throw std::runtime_error(std::string(stage) + " shader compilation failed:\n" + log.data());
    }

    ENGINE_GL_CHECK();
    return shader;
}

void Shader::linkProgram(unsigned int vertex, unsigned int fragment) {
    programId_ = glCreateProgram();
    glAttachShader(programId_, vertex);
    glAttachShader(programId_, fragment);
    glLinkProgram(programId_);

    GLint success = GL_FALSE;
    glGetProgramiv(programId_, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint logLength = 0;
        glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)));
        glGetProgramInfoLog(programId_, logLength, nullptr, log.data());

        glDeleteProgram(programId_);
        programId_ = 0U;
        throw std::runtime_error(std::string("Shader program link failed:\n") + log.data());
    }

    ENGINE_GL_CHECK();
}

int Shader::uniformLocation(const std::string& name) const {
    const int location = glGetUniformLocation(programId_, name.c_str());
#ifndef __EMSCRIPTEN__
    if (location < 0) {
        throw std::runtime_error("Uniform not found in shader program: " + name);
    }
#endif
    return location;
}

} // namespace engine
