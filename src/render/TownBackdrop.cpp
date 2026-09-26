#include "render/TownBackdrop.hpp"

#include <algorithm>
#include <cmath>

namespace render {

namespace {

struct Rgba {
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    std::uint8_t alpha{255};
};

[[nodiscard]] int hash2(const int x, const int y) noexcept {
    unsigned int h = static_cast<unsigned int>(x) * 374761393U + static_cast<unsigned int>(y) * 668265263U;
    h = (h ^ (h >> 13U)) * 1274126177U;
    return static_cast<int>(h & 0x7FFFFFFFU);
}

[[nodiscard]] Rgba mix(const Rgba from, const Rgba to, const float t) noexcept {
    const float clamped = std::clamp(t, 0.0F, 1.0F);
    const auto channel = [clamped](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(
            static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * clamped);
    };
    return {channel(from.red, to.red), channel(from.green, to.green), channel(from.blue, to.blue), 255};
}

[[nodiscard]] Rgba shade(const Rgba color, const float scale) noexcept {
    const auto channel = [scale](const std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(value) * scale, 0.0F, 255.0F));
    };
    return {channel(color.red), channel(color.green), channel(color.blue), color.alpha};
}

void blend(TownPixelBuffer& image, const int x, const int y, const Rgba color) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height || color.alpha == 0) {
        return;
    }
    const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                               static_cast<std::size_t>(x)) *
                              4U;
    const float src = static_cast<float>(color.alpha) / 255.0F;
    const float dst = 1.0F - src;
    const auto channel = [&](const std::size_t offset, const std::uint8_t incoming) {
        const float mixed = static_cast<float>(image.rgba[index + offset]) * dst + static_cast<float>(incoming) * src;
        image.rgba[index + offset] = static_cast<std::uint8_t>(std::clamp(mixed, 0.0F, 255.0F));
    };
    channel(0U, color.red);
    channel(1U, color.green);
    channel(2U, color.blue);
    const int alpha = static_cast<int>(image.rgba[index + 3U]) + static_cast<int>(color.alpha);
    image.rgba[index + 3U] = static_cast<std::uint8_t>(std::min(255, alpha));
}

void put(TownPixelBuffer& image, const int x, const int y, const Rgba color) {
    blend(image, x, y, color);
}

void fillRect(TownPixelBuffer& image, int x0, int y0, int x1, int y1, const Rgba color) {
    if (x1 < x0) {
        std::swap(x0, x1);
    }
    if (y1 < y0) {
        std::swap(y0, y1);
    }
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            put(image, x, y, color);
        }
    }
}

void disc(TownPixelBuffer& image, const int cx, const int cy, const int radius, const Rgba color) {
    const int r2 = radius * radius;
    for (int y = cy - radius; y <= cy + radius; ++y) {
        for (int x = cx - radius; x <= cx + radius; ++x) {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= r2) {
                put(image, x, y, color);
            }
        }
    }
}

void ellipse(TownPixelBuffer& image, const int cx, const int cy, const int rx, const int ry, const Rgba color) {
    if (rx <= 0 || ry <= 0) {
        return;
    }
    for (int y = cy - ry; y <= cy + ry; ++y) {
        for (int x = cx - rx; x <= cx + rx; ++x) {
            const float nx = static_cast<float>(x - cx) / static_cast<float>(rx);
            const float ny = static_cast<float>(y - cy) / static_cast<float>(ry);
            if (nx * nx + ny * ny <= 1.0F) {
                put(image, x, y, color);
            }
        }
    }
}

TownPixelBuffer makeImage(const int width, const int height, const bool clear) {
    TownPixelBuffer image{};
    image.width = std::max(8, width);
    image.height = std::max(8, height);
    image.rgba.assign(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4U, 0);
    if (!clear) {
        for (std::size_t index = 3; index < image.rgba.size(); index += 4U) {
            image.rgba[index] = 255;
        }
    }
    return image;
}

void paintSky(TownPixelBuffer& image) {
    const Rgba top{28, 34, 78, 255};
    const Rgba mid{92, 58, 86, 255};
    const Rgba dusk{214, 122, 72, 255};
    const int horizon = (image.height * 62) / 100;
    for (int y = 0; y < image.height; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(std::max(1, horizon));
        const Rgba sky = y < horizon ? (t < 0.55F ? mix(top, mid, t / 0.55F) : mix(mid, dusk, (t - 0.55F) / 0.45F))
                                     : Rgba{72, 86, 52, 255};
        for (int x = 0; x < image.width; ++x) {
            const int sparkle = hash2(x, y);
            Rgba pixel = sky;
            if (y < horizon / 2 && (sparkle % 173) == 0) {
                pixel = {236, 228, 196, 255};
            }
            put(image, x, y, pixel);
        }
    }

    const int moonX = (image.width * 78) / 100;
    const int moonY = (image.height * 16) / 100;
    const int moonR = std::max(6, image.width / 28);
    disc(image, moonX, moonY, moonR + moonR / 2, {255, 214, 150, 28});
    disc(image, moonX, moonY, moonR, {244, 226, 186, 255});
    disc(image, moonX - moonR / 3, moonY - moonR / 5, moonR / 3, {232, 206, 150, 255});

    for (int cloud = 0; cloud < 4; ++cloud) {
        const int cx = (image.width * (12 + cloud * 18)) / 100;
        const int cy = (image.height * (10 + (cloud % 3) * 6)) / 100;
        ellipse(image, cx, cy, image.width / 10, std::max(3, image.height / 40), {186, 150, 148, 50});
    }
}

void paintHills(TownPixelBuffer& image) {
    const int horizon = (image.height * 62) / 100;
    for (int band = 0; band < 3; ++band) {
        const Rgba hill = mix({36, 32, 58, 255}, {78, 52, 48, 255}, static_cast<float>(band) / 2.0F);
        const int base = horizon - 8 + band * (image.height / 18);
        for (int x = 0; x < image.width; ++x) {
            const float wave = std::sin(static_cast<float>(x) * (0.03F + static_cast<float>(band) * 0.01F) +
                                        static_cast<float>(band)) *
                               static_cast<float>(image.height / 28);
            const int crest = base + static_cast<int>(wave);
            fillRect(image, x, crest, x + 1, horizon + image.height / 10, hill);
        }
    }
    for (int tree = 0; tree < 7; ++tree) {
        const int x = (image.width * (6 + tree * 13)) / 100;
        const int ground = (image.height * 64) / 100;
        const int height = image.height / 7 + (hash2(tree, 3) % std::max(4, image.height / 18));
        const Rgba pine{24, 36, 32, 255};
        for (int y = 0; y < height; ++y) {
            const int half = std::max(1, (height - y) / 5);
            fillRect(image, x - half, ground - height + y, x + half + 1, ground - height + y + 1, pine);
        }
    }
}

void paintMeadow(TownPixelBuffer& image) {
    const int ground = (image.height * 64) / 100;
    for (int y = ground; y < image.height; ++y) {
        const float t = static_cast<float>(y - ground) / static_cast<float>(std::max(1, image.height - ground));
        const Rgba grass = mix({58, 78, 46, 255}, {36, 48, 30, 255}, t);
        for (int x = 0; x < image.width; ++x) {
            const int n = hash2(x, y);
            Rgba pixel = ((n % 11) == 0) ? shade(grass, 1.15F) : grass;
            if ((n % 29) == 0 && y < ground + image.height / 10) {
                pixel = {168, 92, 64, 255};
            }
            put(image, x, y, pixel);
        }
    }

    const int plazaTop = (image.height * 68) / 100;
    const int plazaBottom = (image.height * 90) / 100;
    for (int y = plazaTop; y < plazaBottom; ++y) {
        for (int x = image.width / 12; x < (image.width * 11) / 12; ++x) {
            const int n = hash2(x + 9, y + 4);
            const Rgba dirt = ((n % 5) == 0) ? Rgba{112, 78, 48, 255} : Rgba{92, 64, 40, 255};
            put(image, x, y, dirt);
        }
    }

    const int roadTop = (image.height * 86) / 100;
    for (int y = roadTop; y < image.height - 2; ++y) {
        for (int x = image.width / 5; x < (image.width * 4) / 5; ++x) {
            const bool mortar = ((x / 6 + y / 4) % 2) == 0;
            put(image, x, y, mortar ? Rgba{118, 96, 70, 255} : Rgba{78, 58, 40, 255});
        }
    }

    for (int lamp = 0; lamp < 3; ++lamp) {
        const int x = (image.width * (22 + lamp * 28)) / 100;
        const int y = (image.height * 74) / 100;
        fillRect(image, x, y, x + 2, roadTop, {48, 36, 28, 255});
        disc(image, x + 1, y - 2, std::max(2, image.width / 80), {255, 186, 84, 210});
    }
}

void paintWall(
    TownPixelBuffer& image,
    const int left,
    const int right,
    const int top,
    const int bottom,
    const Rgba stone,
    const bool restored) {
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const int n = hash2(x, y);
            const bool edge = x < left + 2 || x >= right - 2 || y < top + 2;
            Rgba pixel = edge ? shade(stone, 0.55F) : ((n % 6) == 0 ? shade(stone, 1.12F) : stone);
            if (!restored && (n % 17) == 0) {
                pixel = shade(stone, 0.45F);
            }
            if (!restored && y > top + (bottom - top) / 3 && (n % 23) == 0) {
                continue;
            }
            put(image, x, y, pixel);
        }
    }
}

void paintRoof(
    TownPixelBuffer& image,
    const int left,
    const int right,
    const int eave,
    const int peak,
    const Rgba tile,
    const bool restored,
    const bool steeple) {
    const int mid = (left + right) / 2;
    const int span = std::max(1, right - left);
    for (int y = peak; y < eave; ++y) {
        const float t = static_cast<float>(y - peak) / static_cast<float>(std::max(1, eave - peak));
        const int half = std::max(2, static_cast<int>(static_cast<float>(span / 2) * (0.15F + 0.85F * t)));
        for (int x = mid - half; x <= mid + half; ++x) {
            const int n = hash2(x, y);
            if (!restored && (n % 9) == 0) {
                continue;
            }
            const bool edge = x == mid - half || x == mid + half;
            put(image, x, y, edge ? shade(tile, 0.6F) : ((n % 4) == 0 ? shade(tile, 1.15F) : tile));
        }
    }
    if (steeple) {
        fillRect(image, mid - 2, peak - (eave - peak) / 2, mid + 3, peak + 2, shade(tile, 0.7F));
        fillRect(image, mid - 6, peak - (eave - peak) / 3, mid + 7, peak - (eave - peak) / 3 + 3, shade(tile, 0.85F));
    }
}

void paintWindow(TownPixelBuffer& image, const int x, const int y, const int w, const int h, const bool restored, const Rgba glow) {
    fillRect(image, x - 1, y - 1, x + w + 1, y + h + 1, {28, 22, 18, 255});
    if (restored) {
        disc(image, x + w / 2, y + h / 2, std::max(w, h), Rgba{glow.red, glow.green, glow.blue, 70});
        fillRect(image, x, y, x + w, y + h, glow);
        fillRect(image, x + w / 2, y, x + w / 2 + 1, y + h, shade(glow, 0.7F));
    } else {
        fillRect(image, x, y, x + w, y + h, {16, 14, 18, 255});
        fillRect(image, x, y + h / 2, x + w / 2, y + h / 2 + 1, {40, 36, 32, 255});
    }
}

void paintForge(TownPixelBuffer& image, const bool restored) {
    const int left = image.width / 5;
    const int right = (image.width * 4) / 5;
    const int ground = (image.height * 78) / 100;
    const int wallTop = (image.height * 42) / 100;
    const Rgba stone = restored ? Rgba{168, 124, 92, 255} : Rgba{96, 90, 86, 255};
    const Rgba roof = restored ? Rgba{92, 42, 32, 255} : Rgba{62, 48, 46, 255};
    ellipse(image, image.width / 2, ground + 6, (right - left) / 2, image.height / 16, {20, 14, 12, 140});
    paintRoof(image, left - 6, right + 6, wallTop + 4, image.height / 6, roof, restored, false);
    paintWall(image, left, right, wallTop, ground, stone, restored);
    fillRect(image, image.width / 2 - 2, image.height / 10, image.width / 2 + 4, wallTop, shade(stone, 0.7F));
    if (restored) {
        for (int puff = 0; puff < 3; ++puff) {
            disc(image, image.width / 2 + 8 + puff * 4, image.height / 12 - puff * 6, 4 + puff, {210, 206, 198, 90});
        }
    }
    paintWindow(image, left + 8, wallTop + 16, image.width / 10, image.height / 10, restored, {255, 176, 64, 255});
    paintWindow(image, right - image.width / 6, wallTop + 16, image.width / 10, image.height / 10, restored, {255, 150, 48, 255});
    const int doorX = image.width / 2 - image.width / 14;
    fillRect(image, doorX, ground - image.height / 5, doorX + image.width / 7, ground, {42, 28, 22, 255});
    if (restored) {
        disc(image, doorX + image.width / 14, ground - image.height / 10, image.width / 16, {255, 120, 36, 230});
    }
    const int anvil = ground - image.height / 14;
    fillRect(image, left + 4, anvil, left + image.width / 6, anvil + 4, {70, 72, 78, 255});
    fillRect(image, left + image.width / 18, anvil + 4, left + image.width / 9, ground, {54, 56, 62, 255});
}

void paintChapel(TownPixelBuffer& image, const bool restored) {
    const int left = image.width / 4;
    const int right = (image.width * 3) / 4;
    const int ground = (image.height * 80) / 100;
    const int wallTop = (image.height * 46) / 100;
    const Rgba stone = restored ? Rgba{214, 196, 160, 255} : Rgba{110, 104, 112, 255};
    const Rgba roof = restored ? Rgba{92, 58, 86, 255} : Rgba{58, 50, 64, 255};
    ellipse(image, image.width / 2, ground + 4, (right - left) / 2, image.height / 18, {20, 16, 18, 130});
    paintRoof(image, left - 4, right + 4, wallTop + 2, image.height / 5, roof, restored, true);
    paintWall(image, left, right, wallTop, ground, stone, restored);
    paintWindow(image, image.width / 2 - image.width / 16, wallTop + 10, image.width / 8, image.height / 6, restored, {255, 214, 120, 255});
    const int door = image.width / 2 - image.width / 16;
    fillRect(image, door, ground - image.height / 6, door + image.width / 8, ground, restored ? Rgba{92, 58, 36, 255} : Rgba{36, 30, 32, 255});
    if (restored) {
        disc(image, image.width / 2, wallTop - image.height / 10, image.width / 18, {255, 220, 140, 200});
    } else {
        fillRect(image, left + 6, ground - 8, left + image.width / 5, ground + 4, {72, 66, 64, 255});
    }
}

void paintTavern(TownPixelBuffer& image, const bool restored) {
    const int left = image.width / 6;
    const int right = (image.width * 5) / 6;
    const int ground = (image.height * 78) / 100;
    const int wallTop = (image.height * 40) / 100;
    const Rgba stone = restored ? Rgba{150, 72, 58, 255} : Rgba{88, 64, 60, 255};
    const Rgba roof = restored ? Rgba{132, 36, 38, 255} : Rgba{64, 40, 42, 255};
    ellipse(image, image.width / 2, ground + 6, (right - left) / 2, image.height / 16, {24, 12, 12, 140});
    paintRoof(image, left - 8, right + 8, wallTop + 6, image.height / 7, roof, restored, false);
    paintWall(image, left, right, wallTop, ground, stone, restored);
    paintWindow(image, left + 10, wallTop + 18, image.width / 9, image.height / 9, restored, {255, 186, 72, 255});
    paintWindow(image, image.width / 2 - image.width / 18, wallTop + 18, image.width / 9, image.height / 9, restored, {255, 170, 64, 255});
    paintWindow(image, right - image.width / 5, wallTop + 18, image.width / 9, image.height / 9, restored, {255, 160, 56, 255});
    const int signY = wallTop - image.height / 12;
    fillRect(image, image.width / 2 - image.width / 5, signY, image.width / 2 + image.width / 5, signY + image.height / 12, restored ? Rgba{92, 58, 32, 255} : Rgba{48, 40, 36, 255});
    if (restored) {
        disc(image, image.width / 2, signY + image.height / 24, image.height / 28, {196, 48, 52, 255});
        disc(image, left + 4, wallTop + 8, image.width / 22, {255, 176, 64, 230});
    } else {
        fillRect(image, image.width / 2, signY, image.width / 2 + 2, signY + image.height / 7, {36, 28, 26, 255});
    }
}

void paintRoad(TownPixelBuffer& image) {
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const bool stone = ((x / 8 + y / 5) % 2) == 0;
            put(image, x, y, stone ? Rgba{132, 104, 72, 255} : Rgba{86, 64, 44, 255});
        }
    }
    const int postH = (image.height * 3) / 4;
    fillRect(image, image.width / 6, image.height - postH, image.width / 6 + 6, image.height - 2, {58, 40, 28, 255});
    fillRect(image, (image.width * 5) / 6, image.height - postH, (image.width * 5) / 6 + 6, image.height - 2, {58, 40, 28, 255});
    fillRect(image, image.width / 6, image.height / 5, (image.width * 5) / 6 + 6, image.height / 5 + 8, {112, 74, 40, 255});
    disc(image, image.width / 6 + 3, image.height / 5 - 2, 5, {255, 196, 90, 230});
    disc(image, (image.width * 5) / 6 + 3, image.height / 5 - 2, 5, {255, 196, 90, 230});
    const int mid = image.width / 2;
    const int arrowY = image.height / 2;
    for (int step = 0; step < image.width / 10; ++step) {
        put(image, mid - image.width / 14 + step, arrowY, {244, 214, 140, 255});
        put(image, mid - image.width / 14 + step, arrowY + 1, {244, 214, 140, 255});
    }
    for (int dy = -5; dy <= 5; ++dy) {
        const int span = 5 - std::abs(dy);
        fillRect(image, mid + image.width / 16, arrowY + dy, mid + image.width / 16 + span, arrowY + dy + 1, {244, 214, 140, 255});
    }
}

} // namespace

TownPixelBuffer paintTownBackdrop(const int width, const int height) {
    TownPixelBuffer image = makeImage(width, height, false);
    paintSky(image);
    paintHills(image);
    paintMeadow(image);
    return image;
}

TownPixelBuffer paintTownPlate(const TownPlateKind kind, const bool restored, const int width, const int height) {
    TownPixelBuffer image = makeImage(width, height, true);
    switch (kind) {
    case TownPlateKind::Forge:
        paintForge(image, restored);
        break;
    case TownPlateKind::Chapel:
        paintChapel(image, restored);
        break;
    case TownPlateKind::Tavern:
        paintTavern(image, restored);
        break;
    case TownPlateKind::Road:
        paintRoad(image);
        break;
    }
    return image;
}

} // namespace render
