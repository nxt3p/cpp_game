#pragma once

#include <glm/glm.hpp>

#include <string>

namespace engine {

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setFloat(const std::string& name, float value) const;
    void setInt(const std::string& name, int value) const;

    void setModel(const glm::mat4& model) const { setMat4("u_Model", model); }
    void setView(const glm::mat4& view) const { setMat4("u_View", view); }
    void setProjection(const glm::mat4& projection) const {
        setMat4("u_Projection", projection);
    }

    [[nodiscard]] unsigned int programId() const noexcept { return programId_; }

private:
    static std::string readFile(const std::string& path);
    static unsigned int compileStage(unsigned int type, const std::string& source);
    void linkProgram(unsigned int vertex, unsigned int fragment);
    [[nodiscard]] int uniformLocation(const std::string& name) const;

    unsigned int programId_{0};
};

} // namespace engine
