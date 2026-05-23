#!/usr/bin/env python3
"""Generate procedural 2D loot icon PNGs for the RPG inventory UI.

Outputs rarity-tinted icons (weapon/armor/trinket) under assets/textures/loot/.
Deterministic given --seed; safe to re-run in CI or local asset pipelines.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "Pillow is required: pip install pillow"
    ) from exc


RARITIES = ("common", "rare", "legendary")
CATEGORIES = ("weapon", "armor", "trinket")

RARITY_COLORS = {
    "common": (180, 180, 190),
    "rare": (90, 140, 255),
    "legendary": (255, 190, 70),
}

CATEGORY_SHAPES = {
    "weapon": "blade",
    "armor": "shield",
    "trinket": "gem",
}


def lcg(seed: int) -> int:
    return (seed * 1664525 + 1013904223) & 0xFFFFFFFF


def seeded_noise(seed: int, x: int, y: int) -> float:
    payload = struct.pack("<III", seed, x, y)
    digest = hashlib.sha256(payload).digest()
    value = int.from_bytes(digest[:4], "little")
    return value / 0xFFFFFFFF


def draw_shape(draw: ImageDraw.ImageDraw, category: str, size: int, fill: tuple[int, int, int]) -> None:
    pad = size // 6
    if category == "weapon":
        draw.polygon(
            [
                (size // 2, pad),
                (size - pad, size // 2),
                (size // 2, size - pad),
                (pad, size // 2),
            ],
            fill=fill,
        )
    elif category == "armor":
        draw.rounded_rectangle(
            (pad, pad + 2, size - pad, size - pad),
            radius=pad,
            fill=fill,
        )
    else:
        draw.polygon(
            [
                (size // 2, pad),
                (size - pad, size // 2),
                (size // 2, size - pad),
                (pad, size // 2),
            ],
            fill=fill,
        )


def generate_icon(size: int, rarity: str, category: str, seed: int) -> Image.Image:
    base = RARITY_COLORS[rarity]
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    for y in range(size):
        for x in range(size):
            n = seeded_noise(seed, x, y)
            tint = tuple(min(255, int(c * (0.75 + n * 0.35))) for c in base)
            if (x - size // 2) ** 2 + (y - size // 2) ** 2 < (size // 2 - 2) ** 2:
                image.putpixel((x, y), (*tint, 255))

    accent = tuple(min(255, c + 40) for c in base)
    draw_shape(draw, category, size, accent)
    draw.ellipse((2, 2, size - 2, size - 2), outline=(*base, 255), width=2)
    return image


def write_manifest(output_dir: Path) -> None:
    lines = ["filename,category,rarity"]
    for rarity in RARITIES:
        for category in CATEGORIES:
            lines.append(f"loot_{category}_{rarity}.png,{category},{rarity}")
    (output_dir / "loot_manifest.csv").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/textures/loot"),
        help="Directory for generated PNG icons",
    )
    parser.add_argument("--size", type=int, default=64, help="Icon width/height in pixels")
    parser.add_argument("--seed", type=int, default=0xAFF1C0DE, help="Deterministic variation seed")
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    seed = args.seed & 0xFFFFFFFF

    for rarity in RARITIES:
        for category in CATEGORIES:
            seed = lcg(seed)
            icon = generate_icon(args.size, rarity, category, seed)
            filename = f"loot_{category}_{rarity}.png"
            icon.save(args.output / filename)
            print(f"Wrote {args.output / filename}")

    write_manifest(args.output)
    print(f"Wrote {args.output / 'loot_manifest.csv'}")


if __name__ == "__main__":
    main()
