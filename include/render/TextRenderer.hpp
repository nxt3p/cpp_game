#pragma once

#include "Shader.hpp"
#include "ui/UiHitTest.hpp"

#include <string>

namespace render {

class TextRenderer {
public:
    explicit TextRenderer(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

    void resize(int width, int height);
    void beginOverlay() const;
    void endOverlay() const;
    void drawText(float x, float y, const char* text, float scale, const float color[4]) const;
    void drawTextCentered(const ui::Rect& bounds, const char* text, float scale, const float color[4]) const;

    [[nodiscard]] float measureTextWidth(const char* text, float scale) const;

private:
    engine::Shader shader_;
    mutable unsigned int vao_{0};
    mutable unsigned int vbo_{0};
    int screenWidth_{0};
    int screenHeight_{0};
};

} // namespace render
