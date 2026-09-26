#include "ui/UiHitTest.hpp"

namespace ui {

bool Rect::contains(float px, float py) const noexcept {
    return px >= x && px <= (x + width) && py >= y && py <= (y + height);
}

bool rectsOverlap(const Rect& a, const Rect& b) noexcept {
    return a.width > 0.0F && a.height > 0.0F && b.width > 0.0F && b.height > 0.0F && a.x < b.x + b.width &&
           a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
}

} // namespace ui
