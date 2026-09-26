#include "ui/UiTextLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace ui {

namespace {

constexpr float kStbGlyphWidth = 7.0F;

} // namespace

float estimateTextWidth(const char* text, const float scale) noexcept {
    if (text == nullptr) {
        return 0.0F;
    }
    return static_cast<float>(std::strlen(text)) * kStbGlyphWidth * std::max(scale, 0.01F);
}

std::string truncateWithEllipsis(
    const std::string& text,
    const float maxWidthPixels,
    const float scale,
    const TextWidthMeasureFn& measureWidth) {
    if (text.empty() || maxWidthPixels <= 0.0F) {
        return {};
    }

    const TextWidthMeasureFn measure =
        measureWidth ? measureWidth : TextWidthMeasureFn(estimateTextWidth);

    if (measure(text.c_str(), scale) <= maxWidthPixels) {
        return text;
    }

    constexpr const char* kEllipsis = "...";
    const float ellipsisWidth = measure(kEllipsis, scale);
    if (ellipsisWidth >= maxWidthPixels) {
        return kEllipsis;
    }

    std::string clipped = text;
    while (!clipped.empty() && measure(clipped.c_str(), scale) + ellipsisWidth > maxWidthPixels) {
        clipped.pop_back();
        while (!clipped.empty() && (clipped.back() == ' ' || static_cast<unsigned char>(clipped.back()) < 0x20)) {
            clipped.pop_back();
        }
    }

    clipped += kEllipsis;
    return clipped;
}

std::string truncateWithEllipsis(
    const std::string& text,
    const float maxWidthPixels,
    const float scale) noexcept {
    return truncateWithEllipsis(text, maxWidthPixels, scale, TextWidthMeasureFn{});
}

std::string formatGroupedNumber(const int value) {
    const bool negative = value < 0;
    std::string digits = std::to_string(negative ? -static_cast<long long>(value) : static_cast<long long>(value));
    std::string grouped;
    grouped.reserve(digits.size() + digits.size() / 3);
    for (std::size_t index = 0; index < digits.size(); ++index) {
        if (index > 0 && (digits.size() - index) % 3 == 0) {
            grouped.push_back(',');
        }
        grouped.push_back(digits[index]);
    }
    if (negative) {
        grouped.insert(grouped.begin(), '-');
    }
    return grouped;
}

} // namespace ui
