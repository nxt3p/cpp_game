#include "ui/UiScale.hpp"

#include <algorithm>

namespace ui {

namespace {

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;

} // namespace

UiScale::UiScale(const int framebufferWidth, const int framebufferHeight) {
    width = std::max(framebufferWidth, 1);
    height = std::max(framebufferHeight, 1);
    scaleX = static_cast<float>(width) / static_cast<float>(kReferenceWidth);
    scaleY = static_cast<float>(height) / static_cast<float>(kReferenceHeight);
    uniform = std::clamp(std::min(scaleX, scaleY), 0.75F, 2.5F);
}

} // namespace ui
