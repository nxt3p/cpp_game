#!/usr/bin/env python3
"""Slice assets/textures/assets.png into categorized world prop sprites.

The LPC environment sheet (1056x384, black background) is organized roughly as:
  - Left (x<220): flowering bushes, grass tufts
  - Top-center (y<140): pine + round trees
  - Center: mushroom grid, rock clusters, chest, stumps
  - Right (x>400): timber house + roof-color cottages

Outputs PNG slices and world_props.csv under assets/textures/world/.
Re-run after updating the source tilesheet:

    python3 scripts/slice_world_assets.py
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Requires Pillow: pip install Pillow", file=sys.stderr)
    sys.exit(1)

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = REPO_ROOT / "assets" / "textures" / "assets.png"
DEFAULT_OUTPUT = REPO_ROOT / "assets" / "textures" / "world"
PAD = 2
MIN_AREA = 250
BLACK_KEY_THRESHOLD = 24


def load_rgba_sheet(path: Path) -> Image.Image:
    image = Image.open(path)
    if image.mode == "RGBA":
        return image
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            red, green, blue, _alpha = pixels[x, y]
            if red <= BLACK_KEY_THRESHOLD and green <= BLACK_KEY_THRESHOLD and blue <= BLACK_KEY_THRESHOLD:
                pixels[x, y] = (0, 0, 0, 0)
    return rgba


def extract_blobs(image: Image.Image) -> list[dict]:
    width, height = image.size
    pixels = image.load()
    visited = [[False] * width for _ in range(height)]
    blobs: list[dict] = []

    def flood(sx: int, sy: int) -> dict | None:
        stack = [(sx, sy)]
        min_x = max_x = sx
        min_y = max_y = sy
        area = 0
        while stack:
            x, y = stack.pop()
            if x < 0 or y < 0 or x >= width or y >= height or visited[y][x]:
                continue
            if pixels[x, y][3] < 16:
                continue
            visited[y][x] = True
            area += 1
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)
            stack.extend([(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)])

        if area < MIN_AREA:
            return None
        return {
            "x": min_x,
            "y": min_y,
            "w": max_x - min_x + 1,
            "h": max_y - min_y + 1,
            "area": area,
        }

    for y in range(height):
        for x in range(width):
            if not visited[y][x] and pixels[x, y][3] >= 16:
                blob = flood(x, y)
                if blob is not None:
                    blobs.append(blob)

    blobs.sort(key=lambda item: (item["y"], item["x"]))
    return blobs


def classify(blob: dict) -> str | None:
    w, h, area, x, y = blob["w"], blob["h"], blob["area"], blob["x"], blob["y"]
    aspect = w / max(h, 1)

    # Houses: timber frame (tall) or wide cottages (blue/red/green roofs)
    if area > 12_000 or (w > 100 and h > 100):
        return "house"

    # Trees: pine + round canopies in the upper band
    if h >= 80 and area > 2_000 and y < 140:
        return "tree"

    # Mushroom color grid (small caps)
    if area < 450 and h <= 36 and w <= 42:
        return "mushroom"

    # Treasure chest (front-facing wooden box)
    if (
        30 <= w <= 50
        and 35 <= h <= 55
        and 700 < area < 2_000
        and 200 < y < 340
        and 350 < x < 470
        and 0.65 <= aspect <= 0.95
    ):
        return "chest"

    # Flowering bushes + grass tufts on the left
    if 28 <= w <= 65 and 40 <= h <= 56 and x < 220:
        return "bush"

    if area < 420 and y < 200 and x < 220 and h <= 32:
        return "bush"

    # Rocks, pebbles, stumps (skip signs/logs/planks by size band)
    if 450 <= area <= 3_500 and w <= 70 and h <= 70:
        return "rock"

    return None


def semantic_slug(category: str, blob: dict, used: set[str]) -> str:
    w, h, area, x, y = blob["w"], blob["h"], blob["area"], blob["x"], blob["y"]

    if category == "tree":
        if h >= 120:
            base = "tree_round_large"
        elif w < 58:
            base = "tree_pine"
        else:
            base = "tree_round_medium"
    elif category == "house":
        if h >= 180:
            base = "house_timber_tall"
        elif y < 100:
            base = "house_cottage_blue" if x < 760 else "house_cottage_red"
        else:
            base = "house_cottage_green"
    elif category == "bush":
        if y < 70 and x < 95:
            flower = ["blue", "red", "pink", "yellow", "plain"]
            index = min(max(y // 55, 0), len(flower) - 1)
            base = f"bush_flower_{flower[index]}"
        elif h <= 34:
            base = "grass_tuft"
        else:
            base = "bush_round"
    elif category == "rock":
        if h >= 45:
            base = "rock_large"
        elif area < 700:
            base = "rock_pebble"
        else:
            base = "rock_medium"
    elif category == "mushroom":
        palette = ["orange", "blue", "brown", "red"]
        index = min(x // 55, len(palette) - 1)
        base = f"mushroom_{palette[index]}"
    elif category == "chest":
        base = "chest"
    else:
        base = category

    slug = base
    suffix = 2
    while slug in used:
        slug = f"{base}_{suffix}"
        suffix += 1
    used.add(slug)
    return slug


def default_world_height(category: str, pixel_height: int) -> float:
    scale = pixel_height / 64.0
    specs: dict[str, tuple[float, float, float]] = {
        "tree": (3.1, 2.6, 6.5),
        "bush": (1.15, 0.95, 1.75),
        "rock": (1.0, 0.65, 2.4),
        "house": (5.5, 4.5, 17.0),
        "chest": (0.95, 0.85, 1.35),
        "mushroom": (0.55, 0.45, 0.85),
    }
    base, minimum, maximum = specs[category]
    return round(max(minimum, min(maximum, base * scale)), 2)


def default_pick_radius(category: str, world_height: float) -> float:
    radii = {
        "tree": 1.4,
        "bush": 0.9,
        "rock": 1.1,
        "house": 2.8,
        "chest": 0.9,
        "mushroom": 0.45,
    }
    return round(max(radii[category], world_height * 0.28), 2)


def slice_assets(source: Path, output_dir: Path, dry_run: bool) -> int:
    if not source.is_file():
        print(f"Missing source image: {source}", file=sys.stderr)
        return 1

    image = load_rgba_sheet(source)
    blobs = extract_blobs(image)

    if not dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)
        for existing in output_dir.glob("*.png"):
            existing.unlink()
        csv_path = output_dir / "world_props.csv"
        if csv_path.exists():
            csv_path.unlink()

    counts: dict[str, int] = {}
    rows: list[dict[str, str]] = []
    used_slugs: set[str] = set()

    for blob in blobs:
        category = classify(blob)
        if category is None:
            continue

        counts[category] = counts.get(category, 0) + 1
        slug = semantic_slug(category, blob, used_slugs)
        file_name = f"{slug}.png"
        world_height = default_world_height(category, blob["h"])
        pick_radius = default_pick_radius(category, world_height)

        left = max(blob["x"] - PAD, 0)
        top = max(blob["y"] - PAD, 0)
        right = min(blob["x"] + blob["w"] + PAD, image.width)
        bottom = min(blob["y"] + blob["h"] + PAD, image.height)

        if not dry_run:
            crop = image.crop((left, top, right, bottom))
            crop.save(output_dir / file_name)

        rows.append(
            {
                "category": category,
                "slug": slug,
                "file": file_name,
                "world_height": f"{world_height:.2f}",
                "pick_radius": f"{pick_radius:.2f}",
                "pixel_width": str(right - left),
                "pixel_height": str(bottom - top),
                "sheet_x": str(blob["x"]),
                "sheet_y": str(blob["y"]),
            }
        )

    if not dry_run:
        with (output_dir / "world_props.csv").open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(
                handle,
                fieldnames=[
                    "category",
                    "slug",
                    "file",
                    "world_height",
                    "pick_radius",
                    "pixel_width",
                    "pixel_height",
                    "sheet_x",
                    "sheet_y",
                ],
            )
            writer.writeheader()
            writer.writerows(rows)

    print(f"Sliced {len(rows)} props from {source.name} ({image.width}x{image.height}):")
    for category in sorted(counts):
        print(f"  {category}: {counts[category]}")
    print(f"Output: {output_dir}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    return slice_assets(args.source, args.output, args.dry_run)


if __name__ == "__main__":
    raise SystemExit(main())
