#include "render/TextRenderer.hpp"

#include "EngineAssert.hpp"

#include "engine/GlBindings.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

#define STB_EASY_FONT_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_easy_font.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace render {

namespace {

constexpr int kTextVertexBytes = 64 * 1024;

std::string sanitizePrintableAscii(const char* text) {
    if (text == nullptr) {
        return {};
    }
    std::string sanitized;
    sanitized.reserve(std::strlen(text));
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const unsigned char glyph = static_cast<unsigned char>(*cursor);
        if (glyph < 32U || glyph > 127U) {
            continue;
        }
        sanitized.push_back(static_cast<char>(glyph));
    }
    return sanitized;
}

} // namespace

TextRenderer::TextRenderer(const std::string& vertexShaderPath, const std::string& fragmentShaderPath)
    : shader_(vertexShaderPath, fragmentShaderPath) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    ENGINE_GL_CHECK();
}

void TextRenderer::resize(int width, int height) {
    screenWidth_ = std::max(width, 1);
    screenHeight_ = std::max(height, 1);
}

void TextRenderer::beginOverlay() const {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, screenWidth_, screenHeight_);
}

void TextRenderer::endOverlay() const {
    glBindVertexArray(0);
}

float TextRenderer::measureTextWidth(const char* text, float scale) const {
    if (text == nullptr) {
        return 0.0F;
    }

    float width = 0.0F;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const unsigned char glyph = static_cast<unsigned char>(*cursor);
        if (glyph < 32U || glyph > 127U) {
            continue;
        }
        width += static_cast<float>(stb_easy_font_charinfo[glyph - 32U].advance & 15);
        width += stb_easy_font_spacing_val;
    }

    return width * scale;
}

void TextRenderer::drawText(float x, float y, const char* text, float scale, const float color[4]) const {
    const std::string sanitized = sanitizePrintableAscii(text);
    if (sanitized.empty() || screenWidth_ <= 0 || screenHeight_ <= 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, screenWidth_, screenHeight_);

    std::vector<unsigned char> buffer(static_cast<std::size_t>(kTextVertexBytes));
    unsigned char tint[4] = {
        static_cast<unsigned char>(color[0] * 255.0F),
        static_cast<unsigned char>(color[1] * 255.0F),
        static_cast<unsigned char>(color[2] * 255.0F),
        static_cast<unsigned char>(color[3] * 255.0F)};

    const float safeScale = std::max(scale, 0.01F);
    const float drawX = x / safeScale;
    const float drawY = y / safeScale;
    std::vector<char> printBuffer(sanitized.begin(), sanitized.end());
    printBuffer.push_back('\0');
    const int quadCount = stb_easy_font_print(
        drawX,
        drawY,
        printBuffer.data(),
        tint,
        buffer.data(),
        static_cast<int>(buffer.size()));
    if (quadCount <= 0) {
        return;
    }

    struct TextVertex {
        float px;
        float py;
        float pz;
        unsigned char rgba[4];
    };

    std::vector<TextVertex> vertices;
    vertices.reserve(static_cast<std::size_t>(quadCount * 6));

    for (int quad = 0; quad < quadCount; ++quad) {
        TextVertex quadVertices[4];
        for (int corner = 0; corner < 4; ++corner) {
            const int sourceIndex = quad * 4 + corner;
            const float* source = reinterpret_cast<float*>(buffer.data() + sourceIndex * 16);
            quadVertices[corner].px = source[0] * safeScale;
            quadVertices[corner].py = source[1] * safeScale;
            quadVertices[corner].pz = source[2];
            std::memcpy(quadVertices[corner].rgba, buffer.data() + sourceIndex * 16 + 12, 4);
        }

        const int triangleIndices[6] = {0, 1, 2, 0, 2, 3};
        for (int triangleVertex : triangleIndices) {
            vertices.push_back(quadVertices[triangleVertex]);
        }
    }

    const int vertexCount = static_cast<int>(vertices.size());

    shader_.use();
    const glm::mat4 projection = glm::ortho(
        0.0F,
        static_cast<float>(screenWidth_),
        static_cast<float>(screenHeight_),
        0.0F,
        -1.0F,
        1.0F);
    shader_.setMat4("u_Projection", projection);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(TextVertex)),
        vertices.data(),
        GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TextVertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TextVertex), reinterpret_cast<void*>(offsetof(TextVertex, rgba)));

    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
    ENGINE_GL_CHECK();
}

void TextRenderer::drawTextCentered(
    const ui::Rect& bounds,
    const char* text,
    float scale,
    const float color[4]) const {
    if (text == nullptr || text[0] == '\0') {
        return;
    }

    const float textWidth = measureTextWidth(text, scale);
    const float x = bounds.x + (bounds.width - textWidth) * 0.5F;
    const float fontHeight = 8.0F * scale;
    const float y = bounds.y + (bounds.height - fontHeight) * 0.5F;
    drawText(x, y, text, scale, color);
}

} // namespace render
