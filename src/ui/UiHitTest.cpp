#include "ui/UiHitTest.hpp"

namespace ui {

bool Rect::contains(float px, float py) const noexcept {
    return px >= x && px <= (x + width) && py >= y && py <= (y + height);
}

} // namespace ui
