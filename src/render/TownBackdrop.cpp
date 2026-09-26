#include "render/TownBackdrop.hpp"

#include <algorithm>

namespace render {

namespace {

struct Rgba {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha{255};
};

void put(TownPixelBuffer& image, const int x, const int y, const Rgba color) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return;
    }
    const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                               static_cast<std::size_t>(x)) *
                              4U;
    image.rgba[index] = color.red;
    image.rgba[index + 1U] = color.green;
    image.rgba[index + 2U] = color.blue;
    image.rgba[index + 3U] = color.alpha;
}

void fillRect(
    TownPixelBuffer& image,
    const int x0,
    const int y0,
    const int x1,
    const int y1,
    const Rgba color) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            put(image, x, y, color);
        }
    }
}

Rgba mix(const Rgba from, const Rgba to, const float t) {
    const float clamped = std::clamp(t, 0.0F, 1.0F);
    const auto channel = [clamped](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * clamped);
    };
    return {channel(from.red, to.red), channel(from.green, to.green), channel(from.blue, to.blue), 255};
}

void paintRuin(
    TownPixelBuffer& image,
    const int left,
    const int right,
    const int ground,
    const int top,
    const Rgba stone,
    const Rgba shadow,
    const bool chapel) {
    const int width = right - left;
    for (int x = left; x < right; ++x) {
        const int local = x - left;
        const int jagged = ((local * 17 + (chapel ? 11 : 3)) % 7) - 2;
        const int roof = top + jagged + (chapel && local > width / 3 && local < (width * 2) / 3 ? -8 : 0);
        for (int y = roof; y < ground; ++y) {
            const bool edge = x == left || x + 1 == right || y < roof + 2;
            put(image, x, y, edge ? shadow : stone);
        }
        if ((local % 11) == 4 && local > 4 && local + 6 < width) {
            fillRect(image, x, ground - 18, x + 4, ground - 10, {18, 16, 22, 255});
        }
    }
    if (chapel) {
        fillRect(image, left + width / 2 - 2, top - 14, left + width / 2 + 2, top + 2, shadow);
        fillRect(image, left + width / 2 - 6, top - 10, left + width / 2 + 6, top - 8, shadow);
    }
}

} // namespace

TownPixelBuffer paintTownBackdrop(const int width, const int height) {
    TownPixelBuffer image{};
    image.width = std::max(8, width);
    image.height = std::max(8, height);
    image.rgba.assign(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4U, 0);

    const Rgba skyTop{18, 22, 48, 255};
    const Rgba skyHorizon{92, 54, 36, 255};
    const int horizon = (image.height * 58) / 100;
    for (int y = 0; y < image.height; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(std::max(1, horizon));
        const Rgba sky = y < horizon ? mix(skyTop, skyHorizon, t) : Rgba{46, 32, 24, 255};
        for (int x = 0; x < image.width; ++x) {
            put(image, x, y, sky);
        }
    }

    const int moonX = (image.width * 78) / 100;
    const int moonY = (image.height * 16) / 100;
    for (int y = moonY - 8; y <= moonY + 8; ++y) {
        for (int x = moonX - 8; x <= moonX + 8; ++x) {
            const int dx = x - moonX;
            const int dy = y - moonY;
            if (dx * dx + dy * dy <= 49) {
                put(image, x, y, {232, 214, 170, 255});
            }
        }
    }

    for (int band = 0; band < 4; ++band) {
        const int y = horizon - 18 + band * 6;
        const Rgba hill = mix({28, 26, 40, 255}, {54, 36, 32, 255}, static_cast<float>(band) / 3.0F);
        const int crest = (image.height * (8 + band * 3)) / 100;
        for (int x = 0; x < image.width; ++x) {
            const int wave = ((x * (5 + band) / std::max(1, image.width / 16)) % 9) - 4;
            fillRect(image, x, y + wave, x + 1, horizon + crest, hill);
        }
    }

    const int ground = (image.height * 78) / 100;
    fillRect(image, 0, ground, image.width, image.height, {34, 28, 22, 255});
    const int roadTop = ground + (image.height * 4) / 100;
    fillRect(image, image.width / 5, roadTop, (image.width * 4) / 5, image.height - 2, {72, 48, 28, 255});
    for (int x = image.width / 5; x < (image.width * 4) / 5; x += 7) {
        fillRect(image, x, roadTop + 2, x + 3, image.height - 4, {58, 38, 22, 255});
    }

    const int span = image.width / 14;
    paintRuin(image, span, span * 4, ground, horizon - image.height / 10, {58, 56, 62, 255}, {28, 26, 32, 255}, false);
    paintRuin(
        image,
        span * 5,
        span * 9,
        ground,
        horizon - image.height / 7,
        {70, 64, 72, 255},
        {36, 30, 40, 255},
        true);
    paintRuin(
        image,
        span * 10,
        span * 13,
        ground,
        horizon - image.height / 11,
        {78, 48, 40, 255},
        {40, 24, 22, 255},
        false);

    fillRect(image, span + 6, ground - 6, span * 4 - 6, ground, {22, 18, 16, 255});
    fillRect(image, span * 5 + 8, ground - 8, span * 9 - 8, ground, {24, 18, 20, 255});
    fillRect(image, span * 10 + 6, ground - 6, span * 13 - 6, ground, {36, 18, 16, 255});

    return image;
}

} // namespace render
