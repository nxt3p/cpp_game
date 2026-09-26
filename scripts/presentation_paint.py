"""Original painted UI, item, and character frames for the generated atlases.

Procedural pixel art only. Nothing here is copied from another game's textures.
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

CELL = 64


def _noise(x: int, y: int, seed: int) -> int:
    n = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177 & 0xFFFFFFFF
    return (n ^ (n >> 16)) & 255


def _sx(cx: float, sign: int, local: float) -> float:
    return cx + sign * local


def _ordered_box(x0: float, y0: float, x1: float, y1: float) -> tuple[float, float, float, float]:
    return (min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1))


def _box(cx: float, sign: int, local_a: float, local_b: float, top: float, bottom: float) -> tuple[float, float, float, float]:
    left = _sx(cx, sign, local_a)
    right = _sx(cx, sign, local_b)
    return (min(left, right), top, max(left, right), bottom)


def paint_ornate_frame(width: int, height: int, border: int) -> Image.Image:
    """Nine-slice frame. Corners hold filigree; edges are straight metal; center is empty."""
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    metal = (38, 26, 20, 255)
    metal_dark = (18, 12, 12, 255)
    gold = (196, 150, 58, 255)
    gold_hi = (236, 208, 128, 255)
    gold_dim = (120, 84, 32, 255)

    draw.rectangle((0, 0, width - 1, border - 1), fill=metal)
    draw.rectangle((0, height - border, width - 1, height - 1), fill=metal)
    draw.rectangle((0, 0, border - 1, height - 1), fill=metal)
    draw.rectangle((width - border, 0, width - 1, height - 1), fill=metal)

    draw.rectangle((0, 0, width - 1, 3), fill=gold_hi)
    draw.rectangle((0, height - 4, width - 1, height - 1), fill=gold)
    draw.rectangle((0, 0, 3, height - 1), fill=gold_hi)
    draw.rectangle((width - 4, 0, width - 1, height - 1), fill=gold)
    inner = border - 5
    draw.rectangle((inner, inner, width - 1 - inner, inner + 2), fill=gold)
    draw.rectangle((inner, height - 3 - inner, width - 1 - inner, height - 1 - inner), fill=gold_dim)
    draw.rectangle((inner, inner, inner + 2, height - 1 - inner), fill=gold)
    draw.rectangle((width - 3 - inner, inner, width - 1 - inner, height - 1 - inner), fill=gold_dim)
    draw.rectangle((6, 6, width - 7, 8), fill=metal_dark)
    draw.rectangle((6, height - 9, width - 7, height - 7), fill=metal_dark)

    def corner(ox: int, oy: int, fx: int, fy: int) -> None:
        draw.polygon(
            [(ox, oy), (ox + fx * 14, oy), (ox, oy + fy * 14)],
            fill=gold_hi,
        )
        draw.polygon(
            [(ox + fx * 6, oy + fy * 6), (ox + fx * 16, oy + fy * 4), (ox + fx * 10, oy + fy * 16)],
            fill=(168, 48, 42, 255),
        )
        draw.ellipse(_ordered_box(ox + fx * 16, oy + fy * 16, ox + fx * 26, oy + fy * 26), fill=gold)
        draw.ellipse(_ordered_box(ox + fx * 19, oy + fy * 19, ox + fx * 23, oy + fy * 23), fill=(90, 28, 24, 255))

    corner(4, 4, 1, 1)
    corner(width - 5, 4, -1, 1)
    corner(4, height - 5, 1, -1)
    corner(width - 5, height - 5, -1, -1)
    return image


def paint_globe_ring(size: int = 192) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    mask = Image.new("L", (size, size), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.ellipse((3, 3, size - 4, size - 4), fill=255)
    margin = int(size * (1.0 - 0.64) * 0.5)
    mask_draw.ellipse((margin, margin, size - 1 - margin, size - 1 - margin), fill=0)
    metal = Image.new("RGBA", (size, size), (46, 34, 26, 255))
    image = Image.composite(metal, image, mask)
    draw = ImageDraw.Draw(image)
    draw.ellipse((3, 3, size - 4, size - 4), outline=(228, 196, 104, 255), width=5)
    draw.ellipse((10, 10, size - 11, size - 11), outline=(92, 64, 28, 255), width=2)
    draw.ellipse(
        (margin - 4, margin - 4, size - margin + 3, size - margin + 3),
        outline=(210, 170, 78, 255),
        width=4,
    )
    center = size * 0.5
    radius = size * 0.5 - 16
    for index in range(12):
        angle = math.radians(index * 30 + 15)
        x = center + math.cos(angle) * radius
        y = center + math.sin(angle) * radius
        draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(232, 204, 120, 255), outline=(70, 42, 16, 255))
    draw.arc((14, 14, size - 15, size - 15), 206, 332, fill=(255, 236, 186, 255), width=3)
    # Crown spike at the top of the bezel, still inside the metal ring.
    draw.polygon(
        [(center - 10, 18), (center, 4), (center + 10, 18)],
        fill=(220, 180, 80, 255),
    )
    return image


def paint_hotbar_bezel(size: int = 96) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    border = 14
    metal = (32, 22, 16, 245)
    gold = (206, 162, 68, 255)
    hi = (240, 214, 140, 255)
    draw.rectangle((0, 0, size - 1, border), fill=metal)
    draw.rectangle((0, size - border, size - 1, size - 1), fill=metal)
    draw.rectangle((0, 0, border, size - 1), fill=metal)
    draw.rectangle((size - border, 0, size - 1, size - 1), fill=metal)
    draw.rectangle((1, 1, size - 2, 4), fill=hi)
    draw.rectangle((1, size - 5, size - 2, size - 2), fill=gold)
    draw.rectangle((1, 1, 4, size - 2), fill=hi)
    draw.rectangle((size - 5, 1, size - 2, size - 2), fill=gold)
    draw.rectangle((border - 3, border - 3, size - border + 2, border - 1), fill=gold)
    draw.rectangle((border - 3, size - border, size - border + 2, size - border + 2), fill=gold)
    draw.rectangle((border - 3, border - 3, border - 1, size - border + 2), fill=gold)
    draw.rectangle((size - border, border - 3, size - border + 2, size - border + 2), fill=gold)
    for ox, oy in ((3, 3), (size - 16, 3), (3, size - 16), (size - 16, size - 16)):
        draw.polygon([(ox + 6, oy), (ox + 12, oy + 6), (ox + 6, oy + 12), (ox, oy + 6)], fill=hi)
    return image


def paint_level_badge() -> Image.Image:
    image = Image.new("RGBA", (160, 40), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.polygon(
        [(8, 20), (22, 4), (138, 4), (152, 20), (138, 36), (22, 36)],
        fill=(48, 16, 14, 235),
    )
    draw.line([(8, 20), (22, 4), (138, 4), (152, 20), (138, 36), (22, 36), (8, 20)], fill=(220, 176, 72, 255), width=3)
    draw.polygon([(22, 20), (30, 12), (38, 20), (30, 28)], fill=(196, 64, 48, 255))
    draw.polygon([(138, 20), (130, 12), (122, 20), (130, 28)], fill=(196, 64, 48, 255))
    return image


def paint_xp_track() -> Image.Image:
    image = Image.new("RGBA", (256, 24), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((0, 2, 255, 21), radius=6, fill=(24, 16, 12, 230))
    draw.rounded_rectangle((0, 2, 255, 21), radius=6, outline=(196, 150, 58, 255), width=2)
    draw.rectangle((4, 6, 251, 9), fill=(90, 64, 28, 255))
    return image


def paint_potion_vial(size: int = 96) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((34, 8, 62, 28), radius=3, fill=(214, 170, 84, 255))
    draw.rectangle((40, 22, 56, 34), fill=(168, 122, 52, 255))
    draw.rounded_rectangle((28, 30, 68, 86), radius=14, fill=(186, 36, 48, 255))
    draw.polygon([(28, 48), (68, 48), (62, 30), (34, 30)], fill=(120, 24, 36, 255))
    draw.rounded_rectangle((36, 40, 46, 78), radius=4, fill=(255, 150, 140, 180))
    draw.arc((30, 34, 66, 82), 200, 340, fill=(255, 220, 210, 200), width=2)
    draw.ellipse((30, 72, 66, 88), fill=(140, 18, 28, 255))
    return image


def paint_parchment(size: int = 256) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            fiber = _noise(x, y, 3)
            grain = _noise(x // 2, y // 3, 9)
            edge = min(x, y, size - 1 - x, size - 1 - y)
            shade = 0 if edge > 18 else (18 - edge) * 4
            red = 214 - shade // 2 + (fiber - 128) // 10
            green = 176 - shade // 2 + (grain - 128) // 12
            blue = 122 - shade // 2
            pixels[x, y] = (
                max(0, min(255, red)),
                max(0, min(255, green)),
                max(0, min(255, blue)),
                245,
            )
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, size - 1, size - 1), outline=(92, 58, 28, 255), width=4)
    draw.rectangle((6, 6, size - 7, size - 7), outline=(168, 124, 68, 180), width=2)
    return image


def paint_nebula(size: int = 256) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            blend = x / float(size - 1)
            n = _noise(x, y, 11) / 255.0
            red = int(48 * (1.0 - blend) + 10 * blend + n * 18)
            green = int(8 * (1.0 - blend) + 16 * blend + n * 8)
            blue = int(16 * (1.0 - blend) + 72 * blend + n * 20)
            pixels[x, y] = (red, green, blue, 245)
    draw = ImageDraw.Draw(image)
    draw.ellipse((18, 30, 120, 150), fill=(120, 24, 18, 70))
    draw.ellipse((130, 80, 240, 220), fill=(24, 48, 140, 80))
    for index in range(48):
        sx = _noise(index, 2, 4) % size
        sy = _noise(index, 8, 5) % size
        draw.point((sx, sy), fill=(240, 230, 200, 220))
    draw.rectangle((0, 0, size - 1, size - 1), outline=(186, 148, 64, 255), width=3)
    return image


def _icon_plate(draw: ImageDraw.ImageDraw, accent: tuple[int, int, int, int]) -> None:
    draw.rounded_rectangle((2, 2, 61, 61), radius=8, fill=(16, 12, 14, 235))
    draw.rounded_rectangle((2, 2, 61, 61), radius=8, outline=accent, width=2)
    draw.rectangle((6, 5, 58, 8), fill=(255, 255, 255, 28))


def paint_skill_icon(kind: str) -> Image.Image:
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    accents = {
        "power_strike": (255, 150, 48, 255),
        "whirlwind": (120, 210, 255, 255),
        "heal": (80, 230, 120, 255),
        "dash": (170, 190, 255, 255),
        "cleave": (255, 110, 48, 255),
        "firebolt": (255, 120, 36, 255),
        "shout": (240, 190, 70, 255),
        "slam": (220, 140, 60, 255),
    }
    accent = accents.get(kind, (220, 180, 80, 255))
    _icon_plate(draw, accent)
    if kind == "power_strike":
        draw.polygon([(18, 46), (40, 12), (46, 16), (24, 50)], fill=(230, 236, 242, 255))
        draw.polygon([(16, 48), (28, 44), (22, 54)], fill=(196, 150, 48, 255))
        draw.arc((14, 14, 52, 52), 300, 40, fill=accent, width=3)
    elif kind == "whirlwind":
        draw.arc((12, 12, 52, 52), 20, 300, fill=accent, width=4)
        draw.arc((20, 20, 44, 44), 200, 120, fill=(255, 255, 255, 230), width=3)
        draw.polygon([(46, 16), (54, 18), (46, 26)], fill=accent)
    elif kind == "heal":
        draw.rounded_rectangle((26, 12, 38, 52), radius=3, fill=accent)
        draw.rounded_rectangle((12, 26, 52, 38), radius=3, fill=accent)
        draw.ellipse((22, 22, 42, 42), outline=(255, 255, 220, 255), width=2)
    elif kind == "dash":
        draw.polygon([(10, 34), (28, 22), (30, 30), (48, 26), (50, 36), (30, 40), (28, 48)], fill=accent)
        draw.line((12, 40, 22, 44), fill=(255, 255, 255, 180), width=2)
        draw.line((14, 46, 26, 50), fill=(255, 255, 255, 120), width=2)
    elif kind == "cleave":
        draw.polygon([(14, 40), (44, 10), (52, 18), (22, 48)], fill=(210, 216, 224, 255))
        draw.polygon([(18, 44), (30, 40), (24, 52)], fill=(140, 84, 36, 255))
        draw.arc((8, 18, 56, 58), 200, 20, fill=(255, 80, 40, 255), width=4)
    elif kind == "firebolt":
        draw.ellipse((22, 22, 50, 50), fill=(255, 90, 24, 255))
        draw.ellipse((28, 28, 44, 44), fill=(255, 210, 80, 255))
        draw.polygon([(18, 30), (8, 34), (18, 40)], fill=(255, 140, 40, 255))
        draw.polygon([(20, 22), (12, 16), (26, 20)], fill=(255, 180, 60, 255))
    elif kind == "shout":
        draw.polygon([(14, 40), (28, 18), (34, 22), (22, 46)], fill=(186, 140, 64, 255))
        draw.ellipse((26, 16, 40, 34), fill=(220, 180, 80, 255))
        draw.arc((32, 18, 54, 46), 300, 60, fill=accent, width=3)
        draw.arc((38, 14, 58, 50), 300, 60, fill=(255, 230, 160, 255), width=2)
    else:
        draw.polygon([(30, 10), (42, 28), (36, 28), (36, 40), (28, 40), (28, 28), (22, 28)], fill=(180, 186, 196, 255))
        draw.rectangle((24, 40, 40, 48), fill=(140, 84, 36, 255))
        draw.ellipse((16, 44, 48, 56), fill=(90, 58, 28, 255))
        draw.arc((12, 36, 52, 58), 10, 170, fill=accent, width=3)
    return image


def paint_menu_icon(kind: str) -> Image.Image:
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((2, 2, 61, 61), fill=(48, 28, 16, 255))
    draw.ellipse((2, 2, 61, 61), outline=(220, 176, 72, 255), width=3)
    draw.ellipse((8, 8, 55, 55), fill=(20, 12, 12, 255))
    gold = (232, 196, 104, 255)
    if kind == "abilities":
        draw.polygon([(16, 18), (32, 12), (48, 18), (48, 46), (32, 52), (16, 46)], fill=(92, 48, 36, 255))
        draw.line((32, 14, 32, 50), fill=gold, width=2)
        draw.ellipse((26, 24, 38, 36), fill=(120, 200, 255, 255))
    elif kind == "inventory":
        draw.rounded_rectangle((18, 24, 46, 50), radius=4, fill=(120, 72, 36, 255))
        draw.arc((22, 14, 42, 32), 200, 340, fill=gold, width=3)
        draw.rectangle((28, 32, 36, 40), fill=gold)
    elif kind == "map":
        draw.polygon([(18, 22), (32, 14), (46, 22), (46, 48), (32, 54), (18, 48)], fill=(168, 132, 72, 255))
        draw.line((32, 16, 32, 52), fill=(92, 48, 28, 255), width=2)
        draw.ellipse((28, 28, 38, 38), fill=(180, 40, 36, 255))
    elif kind == "settings":
        draw.ellipse((24, 24, 40, 40), fill=gold)
        draw.ellipse((28, 28, 36, 36), fill=(24, 14, 12, 255))
        for angle in range(0, 360, 45):
            rad = math.radians(angle)
            x0 = 32 + math.cos(rad) * 8
            y0 = 32 + math.sin(rad) * 8
            x1 = 32 + math.cos(rad) * 16
            y1 = 32 + math.sin(rad) * 16
            draw.line((x0, y0, x1, y1), fill=gold, width=3)
    else:
        draw.rounded_rectangle((20, 16, 28, 48), radius=3, fill=gold)
        draw.rounded_rectangle((36, 16, 44, 48), radius=3, fill=gold)
    return image


def _item_backdrop(draw: ImageDraw.ImageDraw) -> None:
    draw.rounded_rectangle((4, 4, 59, 59), radius=6, fill=(22, 16, 18, 210))


def paint_item_cell(kind: str) -> Image.Image:
    image = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    _item_backdrop(draw)
    steel = (214, 220, 230, 255)
    gold = (216, 176, 72, 255)
    leather = (112, 72, 40, 255)
    if kind == "weapon":
        draw.polygon([(34, 6), (42, 8), (28, 44), (22, 42)], fill=steel)
        draw.line((30, 10, 26, 38), fill=(150, 160, 176, 255), width=1)
        draw.rectangle((18, 42, 36, 48), fill=gold)
        draw.rectangle((24, 48, 30, 58), fill=leather)
        draw.ellipse((23, 56, 31, 62), fill=gold)
    elif kind == "offhand":
        draw.polygon([(32, 6), (54, 18), (48, 52), (32, 60), (16, 52), (10, 18)], fill=(64, 108, 168, 255))
        draw.polygon([(32, 14), (44, 22), (40, 46), (32, 52), (24, 46), (20, 22)], fill=(186, 48, 42, 255))
        draw.ellipse((27, 28, 37, 38), fill=gold)
    elif kind == "head":
        draw.pieslice((16, 10, 48, 46), 200, 340, fill=(176, 184, 196, 255))
        draw.rectangle((18, 28, 46, 48), fill=(140, 148, 162, 255))
        draw.rectangle((20, 32, 44, 38), fill=(28, 32, 42, 255))
        draw.polygon([(28, 8), (32, 2), (36, 8)], fill=(180, 40, 36, 255))
    elif kind == "shoulders":
        draw.pieslice((4, 16, 30, 50), 200, 40, fill=(150, 156, 170, 255))
        draw.pieslice((34, 16, 60, 50), 140, 340, fill=(150, 156, 170, 255))
        draw.rectangle((26, 28, 38, 40), fill=gold)
    elif kind == "chest":
        draw.polygon([(32, 8), (52, 18), (48, 56), (16, 56), (12, 18)], fill=(168, 48, 42, 255))
        draw.polygon([(32, 16), (40, 22), (36, 48), (28, 48), (24, 22)], fill=gold)
        draw.rectangle((16, 50, 48, 56), fill=leather)
    elif kind == "hands":
        draw.rounded_rectangle((8, 16, 28, 50), radius=6, fill=(92, 58, 36, 255))
        draw.rounded_rectangle((36, 16, 56, 50), radius=6, fill=(92, 58, 36, 255))
        draw.rectangle((10, 28, 26, 32), fill=gold)
        draw.rectangle((38, 28, 54, 32), fill=gold)
    elif kind == "waist":
        draw.rounded_rectangle((6, 24, 58, 42), radius=4, fill=leather)
        draw.rectangle((28, 20, 36, 46), fill=gold)
        draw.polygon([(30, 46), (34, 46), (36, 56), (28, 56)], fill=gold)
    elif kind == "legs":
        draw.polygon([(16, 8), (30, 8), (28, 56), (12, 56)], fill=(48, 62, 120, 255))
        draw.polygon([(34, 8), (48, 8), (52, 56), (36, 56)], fill=(48, 62, 120, 255))
        draw.rectangle((16, 8, 48, 14), fill=gold)
    elif kind == "feet":
        draw.polygon([(10, 28), (30, 26), (34, 40), (28, 52), (8, 48)], fill=(62, 40, 28, 255))
        draw.polygon([(34, 28), (54, 26), (58, 40), (52, 52), (32, 48)], fill=(62, 40, 28, 255))
        draw.rectangle((8, 44, 30, 50), fill=gold)
        draw.rectangle((34, 44, 56, 50), fill=gold)
    elif kind == "amulet":
        draw.arc((16, 6, 48, 30), 200, 340, fill=gold, width=3)
        draw.polygon([(32, 24), (44, 36), (32, 56), (20, 36)], fill=(64, 168, 190, 255))
        draw.polygon([(32, 32), (38, 38), (32, 48), (26, 38)], fill=(220, 245, 255, 255))
    elif kind == "ring":
        draw.ellipse((16, 18, 48, 50), outline=gold, width=6)
        draw.ellipse((26, 8, 38, 22), fill=(186, 36, 48, 255), outline=gold)
    elif kind == "cloak":
        draw.polygon([(32, 6), (56, 16), (50, 58), (14, 58), (8, 16)], fill=(86, 36, 110, 255))
        draw.polygon([(32, 14), (42, 58), (22, 58)], fill=(140, 72, 170, 255))
        draw.rectangle((28, 6, 36, 14), fill=gold)
    elif kind == "charm":
        draw.rounded_rectangle((22, 8, 42, 56), radius=8, fill=(196, 156, 72, 255))
        draw.ellipse((26, 16, 38, 28), fill=(36, 130, 96, 255))
        draw.polygon([(32, 32), (38, 40), (32, 50), (26, 40)], fill=(220, 60, 48, 255))
    elif kind == "relic":
        draw.polygon([(32, 4), (54, 32), (32, 60), (10, 32)], fill=(70, 190, 140, 255))
        draw.polygon([(32, 16), (44, 32), (32, 48), (20, 32)], fill=(236, 244, 230, 255))
        draw.rectangle((30, 28, 34, 36), fill=(40, 90, 70, 255))
    elif kind == "potion":
        draw.rounded_rectangle((26, 18, 38, 54), radius=6, fill=(190, 32, 48, 255))
        draw.rectangle((28, 8, 36, 20), fill=(230, 200, 130, 255))
        draw.rectangle((29, 24, 33, 46), fill=(255, 170, 160, 200))
    else:
        draw.polygon([(32, 6), (52, 24), (44, 52), (20, 52), (12, 24)], fill=(70, 210, 150, 255))
        draw.polygon([(32, 16), (42, 28), (32, 44), (22, 28)], fill=(230, 255, 240, 255))
    return image


ARCHETYPES = {
    "warrior": {
        "armor": (168, 46, 40, 255),
        "trim": (220, 178, 74, 255),
        "cloth": (86, 18, 24, 255),
        "skin": (228, 186, 148, 255),
        "metal": (206, 214, 224, 255),
        "dark": (36, 22, 24, 255),
        "boot": (48, 30, 26, 255),
        "weapon": "sword",
        "head": "helm",
    },
    "ranger": {
        "armor": (52, 110, 62, 255),
        "trim": (186, 150, 64, 255),
        "cloth": (28, 72, 40, 255),
        "skin": (214, 170, 132, 255),
        "metal": (176, 160, 120, 255),
        "dark": (32, 36, 24, 255),
        "boot": (62, 42, 28, 255),
        "weapon": "bow",
        "head": "hood",
    },
    "mage": {
        "armor": (48, 64, 150, 255),
        "trim": (214, 176, 84, 255),
        "cloth": (28, 24, 78, 255),
        "skin": (236, 206, 176, 255),
        "metal": (180, 200, 230, 255),
        "dark": (20, 16, 40, 255),
        "boot": (24, 20, 36, 255),
        "weapon": "staff",
        "head": "hat",
    },
}


def _draw_weapon(draw: ImageDraw.ImageDraw, cx: float, sign: int, hand_y: float, frame: int, clip: str, pal: dict) -> None:
    reach = 16
    lift = 8
    if clip in ("attack", "attack2"):
        reach = 10 + frame * (6 if clip == "attack2" else 5)
        lift = 18 - frame * 6
    elif clip == "cast":
        reach = 14
        lift = 16 + frame * 2
    elif clip == "walk":
        lift = 6 + (frame % 3)
    tip_x = _sx(cx, sign, 8 + reach)
    tip_y = hand_y - lift
    grip_x = _sx(cx, sign, 8)
    if pal["weapon"] == "sword":
        draw.line((grip_x, hand_y, tip_x, tip_y), fill=pal["metal"], width=3)
        draw.polygon(
            [(tip_x, tip_y - 3), (tip_x + sign * 5, tip_y + 2), (tip_x - sign * 2, tip_y + 4)],
            fill=(236, 240, 246, 255),
        )
        draw.line((_sx(cx, sign, 6), hand_y + 2, _sx(cx, sign, 12), hand_y - 2), fill=pal["trim"], width=2)
    elif pal["weapon"] == "bow":
        draw.arc(_ordered_box(_sx(cx, sign, 4), hand_y - 16, _sx(cx, sign, 22), hand_y + 16), 270, 90, fill=pal["trim"], width=2)
        draw.line((_sx(cx, sign, 18), hand_y - 14, _sx(cx, sign, 18), hand_y + 14), fill=(40, 28, 18, 255), width=1)
        if clip in ("attack", "attack2", "cast"):
            draw.line((_sx(cx, sign, 18), hand_y, _sx(cx, sign, 18 + frame * 4), hand_y - 2), fill=pal["metal"], width=2)
    else:
        draw.line((grip_x, hand_y + 10, grip_x, hand_y - 18), fill=(92, 64, 36, 255), width=3)
        orb_y = hand_y - 22 - (frame if clip == "cast" else 0)
        draw.ellipse((grip_x - 5, orb_y, grip_x + 5, orb_y + 10), fill=(120, 190, 255, 255))
        draw.ellipse((grip_x - 2, orb_y + 3, grip_x + 2, orb_y + 7), fill=(240, 250, 255, 255))


def _draw_head(draw: ImageDraw.ImageDraw, cx: float, sign: int, top: float, view: str, pal: dict) -> None:
    if pal["head"] == "hat":
        draw.polygon(
            [(_sx(cx, sign, -8), top + 8), (cx, top - 8), (_sx(cx, sign, 8), top + 8)],
            fill=pal["armor"],
        )
        draw.rectangle(_ordered_box(_sx(cx, sign, -10), top + 7, _sx(cx, sign, 10), top + 12), fill=pal["trim"])
    elif pal["head"] == "hood":
        draw.polygon(
            [(_sx(cx, sign, -8), top + 10), (cx, top - 2), (_sx(cx, sign, 9), top + 8), (_sx(cx, sign, 6), top + 16), (_sx(cx, sign, -8), top + 16)],
            fill=pal["cloth"],
        )
    else:
        draw.pieslice(_ordered_box(_sx(cx, sign, -8), top - 2, _sx(cx, sign, 8), top + 16), 200, 340, fill=pal["metal"])
        draw.polygon([(cx - 2, top - 2), (cx, top - 8), (cx + 2, top - 2)], fill=(170, 36, 32, 255))
    draw.ellipse(_ordered_box(_sx(cx, sign, -6), top + 4, _sx(cx, sign, 6), top + 16), fill=pal["skin"])
    if view == "back":
        draw.rectangle(_ordered_box(_sx(cx, sign, -6), top + 6, _sx(cx, sign, 6), top + 14), fill=pal["cloth"])
        return
    if view == "profile":
        eye = _sx(cx, sign, 3)
        draw.rectangle(_ordered_box(eye, top + 8, eye + sign * 3, top + 10), fill=(28, 16, 16, 255))
    else:
        draw.rectangle(_ordered_box(_sx(cx, sign, -4), top + 8, _sx(cx, sign, -1), top + 11), fill=(28, 16, 16, 255))
        draw.rectangle(_ordered_box(_sx(cx, sign, 1), top + 8, _sx(cx, sign, 4), top + 11), fill=(28, 16, 16, 255))


def paint_hero_cell(draw: ImageDraw.ImageDraw, frame: int, clip: str, direction: int, archetype: str) -> None:
    pal = ARCHETYPES[archetype]
    sign, view = direction_pose_local(direction)
    bob = 1 if clip == "idle" and frame % 2 == 0 else 0
    sink = frame * 3 if clip == "death" else 0
    recoil = 4 if clip == "hit" else 0
    cx = 32 - sign * recoil
    top = 8 + bob + sink
    if clip == "death" and frame >= 4:
        draw.ellipse((cx - 18, 46, cx + 18, 58), fill=(0, 0, 0, 90))
        draw.rounded_rectangle((cx - 16, 44, cx + 18, 56), radius=4, fill=pal["armor"])
        draw.ellipse((cx + 10, 46, cx + 20, 56), fill=pal["skin"])
        return

    stride = 0
    if clip == "walk":
        stride = (4, 2, 0, -2, -4, -1)[frame % 6]
    draw.ellipse((cx - 14, 52, cx + 14, 62), fill=(0, 0, 0, 80))

    if view != "front":
        draw.polygon(
            [(cx, top + 14), (_sx(cx, sign, -14), top + 18), (_sx(cx, sign, -10), top + 40), (cx, top + 34)],
            fill=pal["cloth"],
        )

    leg_top = top + 30
    draw.rectangle(_box(cx, sign, -7, -2, leg_top, 54 + stride), fill=pal["dark"])
    draw.rectangle(_box(cx, sign, 1, 6, leg_top, 54 - stride), fill=pal["dark"])
    draw.rectangle(_box(cx, sign, -8, -1, 50 + max(stride, 0), 56 + max(stride, 0)), fill=pal["boot"])
    draw.rectangle(_box(cx, sign, 1, 8, 50 - min(stride, 0), 56 - min(stride, 0)), fill=pal["boot"])

    torso = _box(cx, sign, -8, 8, top + 14, top + 32)
    draw.rounded_rectangle(torso, radius=3, fill=pal["armor"])
    draw.rectangle(_box(cx, sign, -2, 2, top + 16, top + 30), fill=pal["trim"])
    draw.rectangle(_box(cx, sign, -8, 8, top + 28, top + 32), fill=pal["trim"])
    draw.ellipse(_box(cx, sign, -12, -4, top + 12, top + 22), fill=pal["metal"])
    draw.ellipse(_box(cx, sign, 4, 12, top + 12, top + 22), fill=pal["metal"])

    hand_y = top + 24
    if clip == "cast":
        hand_y -= 6 + frame
    _draw_head(draw, cx, sign, top, view, pal)
    _draw_weapon(draw, cx, sign, hand_y, frame, clip, pal)
    if clip == "hit":
        draw.line((_sx(cx, sign, -6), top + 8, _sx(cx, sign, 10), top + 28), fill=(255, 240, 220, 220), width=2)


def direction_pose_local(index: int) -> tuple[int, str]:
    if index in (2, 3):
        return (-1, "profile")
    if index in (5, 6):
        return (1, "profile")
    if index == 4:
        return (1, "back")
    if index in (1, 7):
        return (-1 if index == 1 else 1, "front")
    return (1, "front")


def paint_warrior_cell(draw: ImageDraw.ImageDraw, frame: int, clip: str, direction: int) -> None:
    paint_hero_cell(draw, frame, clip, direction, "warrior")


def paint_ranger_cell(draw: ImageDraw.ImageDraw, frame: int, clip: str, direction: int) -> None:
    paint_hero_cell(draw, frame, clip, direction, "ranger")


def paint_mage_cell(draw: ImageDraw.ImageDraw, frame: int, clip: str, direction: int) -> None:
    paint_hero_cell(draw, frame, clip, direction, "mage")


def paint_monster_cell(draw: ImageDraw.ImageDraw, frame: int, clip: str, direction: int) -> None:
    sign, view = direction_pose_local(direction)
    y = 18 + (1 if clip == "idle" and frame % 2 == 0 else 0)
    y += frame * 4 if clip == "death" else 0
    cx = 32 - sign * (3 if clip == "hit" else 0)
    if clip == "death" and frame >= 3:
        draw.ellipse((cx - 16, 46, cx + 18, 58), fill=(28, 78, 36, 255))
        return
    hide = (42, 118, 48, 255)
    belly = (168, 150, 84, 255)
    horn = (196, 176, 72, 255)
    dark = (20, 48, 24, 255)
    stride = (frame % 2) * 4 if clip == "walk" else 0
    draw.ellipse((cx - 12, 50, cx + 14, 60), fill=(0, 0, 0, 70))
    if view == "profile":
        draw.ellipse(_box(cx, sign, -16, 8, y + 8, y + 28), fill=hide)
        draw.ellipse(_box(cx, sign, 4, 18, y + 2, y + 18), fill=hide)
        draw.polygon(
            [(_sx(cx, sign, 16), y + 10), (_sx(cx, sign, 24), y + 12), (_sx(cx, sign, 16), y + 16)],
            fill=belly,
        )
        draw.polygon(
            [(_sx(cx, sign, 8), y + 2), (_sx(cx, sign, 4), y - 8), (_sx(cx, sign, 14), y + 4)],
            fill=horn,
        )
        draw.polygon(
            [(_sx(cx, sign, -14), y + 12), (_sx(cx, sign, -22), y + 6), (_sx(cx, sign, -16), y + 20)],
            fill=hide,
        )
        draw.rectangle(_box(cx, sign, -10, -6, y + 24, y + 36 + stride), fill=dark)
        draw.rectangle(_box(cx, sign, -2, 2, y + 24, y + 36 - stride), fill=dark)
        draw.rectangle(_box(cx, sign, 6, 10, y + 22, y + 34 + stride), fill=dark)
        eye = _sx(cx, sign, 12)
        draw.rectangle(_ordered_box(eye, y + 8, eye + sign * 3, y + 11), fill=(240, 220, 60, 255))
        if clip == "attack":
            draw.polygon(
                [
                    (_sx(cx, sign, 18), y + 14),
                    (_sx(cx, sign, 26 + frame * 2), y + 12),
                    (_sx(cx, sign, 18), y + 18),
                ],
                fill=(160, 36, 32, 255),
            )
    else:
        draw.ellipse((cx - 16, y + 10, cx + 16, y + 36), fill=hide)
        draw.ellipse((cx - 8, y + 18, cx + 8, y + 32), fill=belly)
        draw.polygon([(cx - 10, y + 12), (cx - 16, y - 2), (cx - 2, y + 8)], fill=horn)
        draw.polygon([(cx + 10, y + 12), (cx + 16, y - 2), (cx + 2, y + 8)], fill=horn)
        if view != "back":
            draw.ellipse((cx - 8, y + 16, cx - 3, y + 21), fill=(240, 220, 60, 255))
            draw.ellipse((cx + 3, y + 16, cx + 8, y + 21), fill=(240, 220, 60, 255))
        draw.rectangle((cx - 12, y + 32, cx - 6, y + 44 + stride), fill=dark)
        draw.rectangle((cx + 6, y + 32, cx + 12, y + 44 - stride), fill=dark)


def build_ui_atlas(out_dir: Path) -> None:
    width, height = 1024, 512
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    image.paste(paint_globe_ring(192), (0, 0))
    image.paste(paint_ornate_frame(256, 192, 40), (192, 0))
    image.paste(paint_hotbar_bezel(96), (464, 0))
    image.paste(paint_level_badge(), (576, 8))
    image.paste(paint_xp_track(), (576, 56))
    image.paste(paint_potion_vial(96), (848, 0))
    image.paste(paint_parchment(256), (0, 200))
    image.paste(paint_nebula(256), (256, 200))

    skill_names = (
        "skill_power_strike",
        "skill_whirlwind",
        "skill_heal",
        "skill_dash",
        "skill_cleave",
        "skill_firebolt",
        "skill_shout",
        "skill_slam",
    )
    skill_kinds = (
        "power_strike",
        "whirlwind",
        "heal",
        "dash",
        "cleave",
        "firebolt",
        "shout",
        "slam",
    )
    for index, kind in enumerate(skill_kinds):
        image.paste(paint_skill_icon(kind), (520 + index * CELL, 200))

    menu_names = (
        "menu_abilities",
        "menu_inventory",
        "menu_map",
        "menu_settings",
        "menu_pause",
    )
    menu_kinds = ("abilities", "inventory", "map", "settings", "pause")
    for index, kind in enumerate(menu_kinds):
        image.paste(paint_menu_icon(kind), (520 + index * CELL, 280))

    frames = [
        {"name": "globe_ring", "x": 0, "y": 0, "w": 192, "h": 192},
        {"name": "inventory_panel", "x": 192, "y": 0, "w": 256, "h": 192},
        {"name": "hotbar_frame", "x": 464, "y": 0, "w": 96, "h": 96},
        {"name": "level_badge", "x": 576, "y": 8, "w": 160, "h": 40},
        {"name": "xp_track", "x": 576, "y": 56, "w": 256, "h": 24},
        {"name": "potion_vial", "x": 848, "y": 0, "w": 96, "h": 96},
        {"name": "parchment", "x": 0, "y": 200, "w": 256, "h": 256},
        {"name": "nebula", "x": 256, "y": 200, "w": 256, "h": 256},
    ]
    for index, name in enumerate(skill_names):
        frames.append({"name": name, "x": 520 + index * CELL, "y": 200, "w": CELL, "h": CELL})
    for index, name in enumerate(menu_names):
        frames.append({"name": name, "x": 520 + index * CELL, "y": 280, "w": CELL, "h": CELL})

    image.save(out_dir / "ui_atlas.png")
    import json

    (out_dir / "ui_atlas.json").write_text(
        json.dumps({"imageWidth": width, "imageHeight": height, "frames": frames}, indent=2) + "\n",
        encoding="utf-8",
    )


def build_items(out_dir: Path) -> None:
    names = (
        "weapon",
        "offhand",
        "head",
        "shoulders",
        "chest",
        "hands",
        "waist",
        "legs",
        "feet",
        "amulet",
        "ring",
        "cloak",
        "charm",
        "relic",
        "potion",
        "gem",
    )
    columns = 8
    rows = (len(names) + columns - 1) // columns
    image = Image.new("RGBA", (CELL * columns, CELL * rows), (0, 0, 0, 0))
    for index, name in enumerate(names):
        origin_x = (index % columns) * CELL
        origin_y = (index // columns) * CELL
        image.paste(paint_item_cell(name), (origin_x, origin_y))
    image.save(out_dir / "items_atlas.png")
    import json

    frames = []
    for index, name in enumerate(names):
        frames.append(
            {
                "name": name,
                "x": (index % columns) * CELL,
                "y": (index // columns) * CELL,
                "w": CELL,
                "h": CELL,
            }
        )
    (out_dir / "items_atlas.json").write_text(
        json.dumps(
            {"imageWidth": CELL * columns, "imageHeight": CELL * rows, "frames": frames},
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
