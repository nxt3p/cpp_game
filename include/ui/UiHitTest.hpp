#pragma once

namespace ui {

struct Rect {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};

    [[nodiscard]] bool contains(float px, float py) const noexcept;
};

} // namespace ui
