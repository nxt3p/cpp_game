#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace ui {

/// Width callback used for truncation (pass TextRenderer::measureTextWidth in-game).
using TextWidthMeasureFn = std::function<float(const char*, float)>;

[[nodiscard]] float estimateTextWidth(const char* text, float scale) noexcept;

[[nodiscard]] std::string truncateWithEllipsis(
    const std::string& text,
    float maxWidthPixels,
    float scale,
    const TextWidthMeasureFn& measureWidth);

[[nodiscard]] std::string truncateWithEllipsis(
    const std::string& text,
    float maxWidthPixels,
    float scale) noexcept;

/// 39007431 -> "39,007,431". Negative values keep the sign.
[[nodiscard]] std::string formatGroupedNumber(int value);

} // namespace ui
