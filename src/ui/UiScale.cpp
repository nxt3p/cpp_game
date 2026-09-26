#include "ui/UiScale.hpp"

#include <algorithm>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace ui {

namespace {

constexpr int kReferenceWidth = 1280;
constexpr int kReferenceHeight = 720;

[[nodiscard]] UiPlatformKind resolvePlatform(
    const int width,
    const int height,
    const UiPlatformKind requested) noexcept {
    if (requested == UiPlatformKind::Desktop) {
        return UiPlatformKind::Desktop;
    }
    if (requested == UiPlatformKind::Mobile) {
        return UiPlatformKind::Mobile;
    }
    if (width < 900 || height > width) {
        return UiPlatformKind::Mobile;
    }
    return UiPlatformKind::Browser;
}

} // namespace

UiPlatformKind detectUiPlatform() noexcept {
#if defined(__EMSCRIPTEN__)
    const int touchPoints = EM_ASM_INT({
        var points = (navigator && navigator.maxTouchPoints) ? navigator.maxTouchPoints : 0;
        var coarse = false;
        if (window.matchMedia) {
            coarse = window.matchMedia("(pointer: coarse)").matches;
        }
        return (points > 0 || coarse) ? 1 : 0;
    });
    return touchPoints != 0 ? UiPlatformKind::Mobile : UiPlatformKind::Browser;
#else
    return UiPlatformKind::Desktop;
#endif
}

UiScale::UiScale(const int framebufferWidth, const int framebufferHeight, const UiPlatformKind requested) {
    width = std::max(framebufferWidth, 1);
    height = std::max(framebufferHeight, 1);
    platform = resolvePlatform(width, height, requested);
    scaleX = static_cast<float>(width) / static_cast<float>(kReferenceWidth);
    scaleY = static_cast<float>(height) / static_cast<float>(kReferenceHeight);
    const float raw = std::min(scaleX, scaleY);
    const float floor = platform == UiPlatformKind::Desktop ? 0.75F : 0.35F;
    uniform = std::clamp(raw, floor, 2.5F);
    switch (platform) {
    case UiPlatformKind::Mobile:
        touchBoost = 1.35F;
        break;
    case UiPlatformKind::Browser:
        touchBoost = 1.15F;
        break;
    case UiPlatformKind::Desktop:
        touchBoost = 1.0F;
        break;
    }
}

float UiScale::minTouchTarget() const noexcept {
    switch (platform) {
    case UiPlatformKind::Mobile:
        return 48.0F;
    case UiPlatformKind::Browser:
        return 40.0F;
    case UiPlatformKind::Desktop:
        return 28.0F;
    }
    return 28.0F;
}

} // namespace ui
