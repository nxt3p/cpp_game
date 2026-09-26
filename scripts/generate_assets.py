#!/usr/bin/env python3
"""Build deterministic 2.5D atlases. Item icons can be refined with SDXL.

Procedural output (Pillow) is the source of truth for the hero sheet and for tests.
A single diffusion still must not be pasted into the looping idle clip: that cell
is a different character, so the warrior pops every idle frame and again on walk.

  python3 scripts/generate_assets.py              # procedural, then try diffusion
  python3 scripts/generate_assets.py --no-diffusion
"""

from __future__ import annotations

import argparse
import json
from collections import deque
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Pillow is required: pip install pillow") from exc


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "generated"

CELL = 64
COLUMNS = 6
DIRECTIONS = (
    "south",
    "southwest",
    "west",
    "northwest",
    "north",
    "northeast",
    "east",
    "southeast",
)
WARRIOR_CLIPS = (
    ("idle", 4, 6.0, True),
    ("walk", 6, 10.0, True),
    ("attack", 4, 12.0, False),
    ("attack2", 4, 12.0, False),
    ("cast", 4, 10.0, False),
    ("hit", 2, 12.0, False),
    ("death", 6, 8.0, False),
)
MONSTER_CLIPS = (
    ("idle", 4, 6.0, True),
    ("walk", 4, 10.0, True),
    ("attack", 4, 12.0, False),
    ("hit", 2, 12.0, False),
    ("death", 4, 8.0, False),
)


def clip_rows() -> dict[str, int]:
    rows: dict[str, int] = {}
    cursor = 0
    for name, _frames, _fps, _loop in WARRIOR_CLIPS:
        rows[name] = cursor
        cursor += len(DIRECTIONS)
    return rows


def direction_pose(index: int) -> tuple[int, str]:
    """Return (x mirror sign, view). view is front, back, or profile."""
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
    sign, view = direction_pose(direction)
    bob = 1 if clip == "idle" and frame % 2 == 0 else 0
    step = (frame % 2) * 3 if clip == "walk" else 0
    sink = frame * 3 if clip == "death" else 0
    recoil = 4 if clip == "hit" else 0
    origin_x = 32 + sign * (2 if view == "front" else 0) + (recoil * sign)
    origin_y = 18 + bob + sink

    armor = (168, 48, 40, 255)
    skin = (214, 176, 138, 255)
    steel = (196, 204, 214, 255)
    cape = (92, 22, 28, 255)
    dark = (28, 18, 20, 255)

    if clip == "death" and frame >= 4:
        draw.ellipse((origin_x - 16, 46, origin_x + 16, 58), fill=armor)
        return

    draw.rounded_rectangle((origin_x - 10, origin_y + 10, origin_x + 10, origin_y + 28), radius=3, fill=cape)
    draw.rectangle((origin_x - 8, origin_y + 12, origin_x + 8, origin_y + 30), fill=armor)
    draw.ellipse((origin_x - 7, origin_y, origin_x + 7, origin_y + 14), fill=skin)
    draw.rectangle((origin_x - 8, origin_y - 2, origin_x + 8, origin_y + 5), fill=steel)

    leg = step if frame % 2 == 0 else -step
    draw.rectangle((origin_x - 7, origin_y + 28, origin_x - 2, origin_y + 40 + leg), fill=dark)
    draw.rectangle((origin_x + 2, origin_y + 28, origin_x + 7, origin_y + 40 - leg), fill=dark)

    arm_y = origin_y + 14
    if clip == "cast":
        arm_y -= 6 + frame
        draw.ellipse((origin_x - 4, arm_y - 10, origin_x + 4, arm_y - 2), fill=(255, 196, 80, 255))
    elif clip in ("attack", "attack2"):
        reach = 10 + frame * (4 if clip == "attack2" else 3)
        sword_x = origin_x + sign * reach
        draw.line((origin_x + sign * 6, arm_y, sword_x, arm_y - 12), fill=steel, width=2)
        draw.polygon(
            [(sword_x, arm_y - 16), (sword_x + sign * 4, arm_y - 8), (sword_x - sign * 2, arm_y - 6)],
            fill=(230, 230, 236, 255),
        )
    else:
        draw.line((origin_x + sign * 6, arm_y, origin_x + sign * 14, arm_y + 8), fill=steel, width=2)

    if view == "back":
        draw.rectangle((origin_x - 6, origin_y + 4, origin_x + 6, origin_y + 12), fill=cape)
    elif view == "profile":
        eye_a = origin_x + sign * 4
        eye_b = origin_x + sign * 8
        draw.ellipse((min(eye_a, eye_b), origin_y + 4, max(eye_a, eye_b), origin_y + 8), fill=(40, 24, 24, 255))


def paint_monster_cell(draw: ImageDraw.ImageDraw, frame: int, clip: str, direction: int) -> None:
    sign, _view = direction_pose(direction)
    y = 16 + (1 if clip == "idle" and frame % 2 == 0 else 0)
    y += frame * 4 if clip == "death" else 0
    x = 32 + sign * 2
    hide = (36, 110, 48, 255)
    horn = (180, 170, 60, 255)
    if clip == "death" and frame >= 3:
        draw.ellipse((x - 14, 46, x + 14, 58), fill=hide)
        return
    draw.ellipse((x - 14, y + 8, x + 14, y + 36), fill=hide)
    draw.polygon([(x - 10, y + 10), (x - 16, y - 2), (x - 4, y + 8)], fill=horn)
    draw.polygon([(x + 10, y + 10), (x + 16, y - 2), (x + 4, y + 8)], fill=horn)
    eye_dx = 4 * sign
    draw.ellipse((x + eye_dx - 2, y + 16, x + eye_dx + 2, y + 20), fill=(240, 230, 80, 255))
    if clip == "attack":
        draw.polygon(
            [(x + sign * 12, y + 22), (x + sign * (22 + frame * 2), y + 18), (x + sign * 12, y + 28)],
            fill=(150, 40, 36, 255),
        )
    if clip == "walk":
        draw.rectangle((x - 8, y + 32, x - 3, y + 42 + (frame % 2) * 3), fill=(20, 50, 24, 255))
        draw.rectangle((x + 3, y + 32, x + 8, y + 42 - (frame % 2) * 3), fill=(20, 50, 24, 255))


def build_sheet(
    clips: tuple[tuple[str, int, float, bool], ...],
    painter,
) -> tuple[Image.Image, list[dict[str, object]]]:
    rows = len(clips) * len(DIRECTIONS)
    image = Image.new("RGBA", (COLUMNS * CELL, rows * CELL), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    metadata: list[dict[str, object]] = []
    for clip_index, (name, frames, fps, loop) in enumerate(clips):
        base_row = clip_index * len(DIRECTIONS)
        metadata.append(
            {
                "name": name,
                "baseRow": base_row,
                "frames": frames,
                "fps": fps,
                "loop": loop,
            }
        )
        for direction in range(len(DIRECTIONS)):
            for frame in range(frames):
                cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
                cell_draw = ImageDraw.Draw(cell)
                painter(cell_draw, frame, name, direction)
                image.paste(cell, ((frame * CELL), (base_row + direction) * CELL))
    return image, metadata


def write_directional(path_png: Path, path_json: Path, image: Image.Image, clips: list[dict[str, object]]) -> None:
    image.save(path_png)
    payload = {
        "frameWidth": CELL,
        "frameHeight": CELL,
        "columns": COLUMNS,
        "directions": list(DIRECTIONS),
        "clips": clips,
    }
    path_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def build_ui_atlas() -> None:
    width, height = 512, 256
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ring = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    mask = Image.new("L", (128, 128), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.ellipse((4, 4, 124, 124), fill=255)
    mask_draw.ellipse((22, 22, 106, 106), fill=0)
    gold = Image.new("RGBA", (128, 128), (196, 154, 64, 255))
    ring = Image.composite(gold, ring, mask)
    highlight = ImageDraw.Draw(ring)
    highlight.arc((10, 10, 118, 118), 200, 340, fill=(240, 214, 140, 255), width=3)
    image.paste(ring, (0, 0))

    panel = Image.new("RGBA", (192, 128), (0, 0, 0, 0))
    panel_draw = ImageDraw.Draw(panel)
    panel_draw.rounded_rectangle((0, 0, 191, 127), radius=8, fill=(28, 22, 26, 235))
    panel_draw.rounded_rectangle((3, 3, 188, 124), radius=6, outline=(176, 138, 58, 255), width=3)
    image.paste(panel, (128, 0))

    slot = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    slot_draw = ImageDraw.Draw(slot)
    slot_draw.rounded_rectangle((1, 1, 62, 62), radius=4, fill=(16, 14, 18, 220))
    slot_draw.rounded_rectangle((2, 2, 61, 61), radius=3, outline=(168, 132, 54, 255), width=2)
    image.paste(slot, (336, 0))

    image.save(OUT / "ui_atlas.png")
    frames = [
        {"name": "globe_ring", "x": 0, "y": 0, "w": 128, "h": 128},
        {"name": "inventory_panel", "x": 128, "y": 0, "w": 192, "h": 128},
        {"name": "hotbar_frame", "x": 336, "y": 0, "w": 64, "h": 64},
    ]
    (OUT / "ui_atlas.json").write_text(
        json.dumps({"imageWidth": width, "imageHeight": height, "frames": frames}, indent=2) + "\n",
        encoding="utf-8",
    )


def _save_cursor(name: str, painter) -> None:
    image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    painter(ImageDraw.Draw(image))
    image.save(OUT / name)


def build_cursor() -> None:
    tip = [(1, 1), (1, 26), (8, 20), (12, 30), (16, 28), (12, 17), (22, 17)]

    def paint_default(draw: ImageDraw.ImageDraw) -> None:
        draw.polygon(tip, fill=(236, 214, 150, 255), outline=(20, 12, 8, 255))

    def paint_enemy(draw: ImageDraw.ImageDraw) -> None:
        draw.polygon(tip, fill=(196, 48, 42, 255), outline=(24, 8, 8, 255))
        draw.line((18, 6, 28, 16), fill=(240, 220, 180, 255), width=2)

    def paint_loot(draw: ImageDraw.ImageDraw) -> None:
        draw.polygon(tip, fill=(232, 186, 64, 255), outline=(48, 28, 8, 255))
        draw.ellipse((16, 4, 28, 16), outline=(255, 236, 170, 255), width=2)

    def paint_slot(draw: ImageDraw.ImageDraw) -> None:
        draw.rectangle((2, 2, 10, 10), outline=(236, 214, 150, 255), width=2)
        draw.rectangle((21, 2, 29, 10), outline=(236, 214, 150, 255), width=2)
        draw.rectangle((2, 21, 10, 29), outline=(236, 214, 150, 255), width=2)
        draw.rectangle((21, 21, 29, 29), outline=(236, 214, 150, 255), width=2)
        draw.line((14, 8, 14, 23), fill=(236, 214, 150, 255), width=1)
        draw.line((8, 14, 23, 14), fill=(236, 214, 150, 255), width=1)

    _save_cursor("cursor.png", paint_default)
    _save_cursor("cursor_enemy.png", paint_enemy)
    _save_cursor("cursor_loot.png", paint_loot)
    _save_cursor("cursor_slot.png", paint_slot)


def build_items() -> None:
    """16 gear icons. Procedural paint is the source of truth if diffusion is skipped."""
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
    draw = ImageDraw.Draw(image)

    def cell_origin(index: int) -> tuple[int, int]:
        return (index % columns) * CELL, (index // columns) * CELL

    def paint_weapon(x: int, y: int) -> None:
        draw.polygon([(x + 34, y + 4), (x + 46, y + 28), (x + 22, y + 28)], fill=(214, 214, 224, 255))
        draw.rectangle((x + 28, y + 28, x + 36, y + 52), fill=(122, 74, 36, 255))
        draw.rectangle((x + 22, y + 48, x + 42, y + 56), fill=(196, 150, 48, 255))

    def paint_offhand(x: int, y: int) -> None:
        draw.ellipse((x + 10, y + 8, x + 54, y + 56), fill=(72, 112, 168, 255))
        draw.ellipse((x + 24, y + 22, x + 40, y + 42), fill=(220, 186, 84, 255))

    def paint_head(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 16, y + 10, x + 48, y + 40), radius=8, fill=(150, 156, 168, 255))
        draw.rectangle((x + 18, y + 36, x + 46, y + 54), fill=(96, 102, 114, 255))
        draw.rectangle((x + 22, y + 22, x + 30, y + 28), fill=(40, 48, 64, 255))
        draw.rectangle((x + 34, y + 22, x + 42, y + 28), fill=(40, 48, 64, 255))

    def paint_shoulders(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 6, y + 16, x + 28, y + 48), radius=6, fill=(120, 84, 48, 255))
        draw.rounded_rectangle((x + 36, y + 16, x + 58, y + 48), radius=6, fill=(120, 84, 48, 255))
        draw.rectangle((x + 24, y + 28, x + 40, y + 40), fill=(186, 148, 64, 255))

    def paint_chest(x: int, y: int) -> None:
        draw.polygon(
            [(x + 32, y + 8), (x + 54, y + 20), (x + 48, y + 56), (x + 16, y + 56), (x + 10, y + 20)],
            fill=(168, 64, 58, 255),
        )
        draw.rectangle((x + 28, y + 18, x + 36, y + 40), fill=(212, 176, 72, 255))

    def paint_hands(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 8, y + 18, x + 28, y + 50), radius=6, fill=(92, 58, 36, 255))
        draw.rounded_rectangle((x + 36, y + 18, x + 56, y + 50), radius=6, fill=(92, 58, 36, 255))

    def paint_waist(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 8, y + 24, x + 56, y + 40), radius=4, fill=(110, 72, 40, 255))
        draw.rectangle((x + 28, y + 22, x + 36, y + 42), fill=(214, 176, 64, 255))

    def paint_legs(x: int, y: int) -> None:
        draw.polygon([(x + 18, y + 8), (x + 30, y + 8), (x + 28, y + 56), (x + 14, y + 56)], fill=(48, 62, 110, 255))
        draw.polygon([(x + 34, y + 8), (x + 46, y + 8), (x + 50, y + 56), (x + 36, y + 56)], fill=(48, 62, 110, 255))

    def paint_feet(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 8, y + 28, x + 30, y + 50), radius=4, fill=(70, 46, 32, 255))
        draw.rounded_rectangle((x + 34, y + 28, x + 56, y + 50), radius=4, fill=(70, 46, 32, 255))

    def paint_amulet(x: int, y: int) -> None:
        draw.arc((x + 18, y + 6, x + 46, y + 28), 200, 340, fill=(212, 176, 72, 255), width=3)
        draw.polygon([(x + 32, y + 26), (x + 44, y + 40), (x + 32, y + 56), (x + 20, y + 40)], fill=(64, 150, 180, 255))

    def paint_ring(x: int, y: int) -> None:
        draw.ellipse((x + 16, y + 16, x + 48, y + 48), outline=(212, 176, 72, 255), width=6)
        draw.ellipse((x + 26, y + 10, x + 38, y + 22), fill=(180, 40, 48, 255))

    def paint_cloak(x: int, y: int) -> None:
        draw.polygon(
            [(x + 32, y + 6), (x + 56, y + 18), (x + 48, y + 58), (x + 16, y + 58), (x + 8, y + 18)],
            fill=(72, 36, 96, 255),
        )
        draw.polygon([(x + 32, y + 16), (x + 40, y + 58), (x + 24, y + 58)], fill=(120, 64, 150, 255))

    def paint_charm(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 20, y + 8, x + 44, y + 56), radius=6, fill=(186, 150, 72, 255))
        draw.ellipse((x + 26, y + 18, x + 38, y + 30), fill=(40, 120, 90, 255))

    def paint_relic(x: int, y: int) -> None:
        draw.polygon([(x + 32, y + 4), (x + 54, y + 32), (x + 32, y + 60), (x + 10, y + 32)], fill=(90, 200, 150, 255))
        draw.polygon([(x + 32, y + 18), (x + 42, y + 32), (x + 32, y + 46), (x + 22, y + 32)], fill=(230, 240, 220, 255))

    def paint_potion(x: int, y: int) -> None:
        draw.rounded_rectangle((x + 22, y + 16, x + 42, y + 54), radius=8, fill=(180, 32, 48, 255))
        draw.rectangle((x + 26, y + 8, x + 38, y + 18), fill=(230, 200, 120, 255))

    def paint_gem(x: int, y: int) -> None:
        draw.polygon([(x + 32, y + 6), (x + 54, y + 32), (x + 32, y + 58), (x + 10, y + 32)], fill=(80, 200, 140, 255))

    painters = (
        paint_weapon,
        paint_offhand,
        paint_head,
        paint_shoulders,
        paint_chest,
        paint_hands,
        paint_waist,
        paint_legs,
        paint_feet,
        paint_amulet,
        paint_ring,
        paint_cloak,
        paint_charm,
        paint_relic,
        paint_potion,
        paint_gem,
    )
    for index, painter in enumerate(painters):
        painter(*cell_origin(index))

    image.save(OUT / "items_atlas.png")
    frames = []
    for index, name in enumerate(names):
        origin_x, origin_y = cell_origin(index)
        frames.append({"name": name, "x": origin_x, "y": origin_y, "w": CELL, "h": CELL})
    (OUT / "items_atlas.json").write_text(
        json.dumps(
            {"imageWidth": CELL * columns, "imageHeight": CELL * rows, "frames": frames},
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"Item atlas written to {OUT / 'items_atlas.png'}")


def paint_menu_background() -> Image.Image:
    """Wide night scene behind the main menu. Diffusion may replace the paint later."""
    width, height = 1280, 720
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for y in range(height):
        sky = y / float(height)
        for x in range(width):
            horizon = 0.62 + 0.04 * ((x / width) - 0.5)
            if sky < horizon:
                blend = sky / horizon
                red = int(28 * (1.0 - blend) + 8 * blend)
                green = int(18 * (1.0 - blend) + 8 * blend)
                blue = int(48 * (1.0 - blend) + 16 * blend)
            else:
                ground = (sky - horizon) / max(1.0 - horizon, 0.01)
                red = int(18 * (1.0 - ground) + 6 * ground)
                green = int(14 * (1.0 - ground) + 5 * ground)
                blue = int(16 * (1.0 - ground) + 6 * ground)
            pixels[x, y] = (red, green, blue)

    draw = ImageDraw.Draw(image)
    draw.ellipse((1010, 64, 1104, 158), fill=(214, 206, 176))
    draw.ellipse((1036, 78, 1118, 160), fill=(22, 16, 32))
    draw.polygon(
        [(0, 470), (140, 360), (260, 450), (420, 300), (610, 460), (820, 280), (1040, 430), (1280, 340), (1280, 720), (0, 720)],
        fill=(10, 8, 14),
    )
    draw.polygon(
        [(430, 430), (470, 250), (500, 250), (500, 210), (530, 210), (530, 250), (560, 250), (600, 430)],
        fill=(16, 12, 18),
    )
    draw.rectangle((492, 168, 538, 214), fill=(18, 14, 20))
    draw.polygon([(492, 168), (515, 132), (538, 168)], fill=(22, 16, 24))
    for window_x, window_y in ((488, 280), (522, 280), (488, 330), (522, 330), (505, 380)):
        draw.rectangle((window_x, window_y, window_x + 10, window_y + 16), fill=(186, 132, 48))
    draw.rectangle((0, 500, width, height), fill=(8, 6, 8))
    image.save(OUT / "menu_background.png")
    print(f"Menu background written to {OUT / 'menu_background.png'}")
    return image


def diffuse_menu_background() -> None:
    """Best-effort local diffusion pass. Keeps the procedural plate if this fails."""
    try:
        import torch
        from diffusers import StableDiffusionPipeline
    except Exception as exc:  # noqa: BLE001
        print(f"Menu background diffusion skipped: {exc}")
        return
    if not torch.cuda.is_available():
        print("Menu background diffusion skipped: CUDA is not available.")
        return

    print(f"Menu background device: {torch.cuda.get_device_name(0)}")
    try:
        pipe = StableDiffusionPipeline.from_pretrained(
            "runwayml/stable-diffusion-v1-5",
            dtype=torch.float16,
            safety_checker=None,
            requires_safety_checker=False,
        )
        pipe = pipe.to("cuda")
        result = pipe(
            "dark fantasy night landscape, one ruined gothic cathedral, exactly one moon, "
            "fog, distant mountains, warm window light, empty foreground, oil painting, full frame, no people, no text",
            negative_prompt="text, watermark, logo, people, person, two moons, daytime, letterbox, border, frame, blurry",
            num_inference_steps=28,
            guidance_scale=7.5,
            width=768,
            height=448,
        )
        frame = result.images[0].convert("RGB")
    except Exception as exc:  # noqa: BLE001
        print(f"Menu background diffusion skipped: {exc}")
        return

    sample = frame.convert("L").resize((8, 8))
    pixels = sample.get_flattened_data() if hasattr(sample, "get_flattened_data") else sample.getdata()
    if sum(pixels) / 64.0 < 8.0:
        print("Menu background diffusion returned a blank frame; procedural plate kept.")
        return

    fitted = frame.resize((1280, 720), Image.Resampling.LANCZOS)
    veil = Image.new("RGB", fitted.size, (0, 0, 0))
    fitted = Image.blend(fitted, veil, 0.28)
    fitted.save(OUT / "menu_background.png")
    print("Diffusion menu background packed into menu_background.png")


# SDXL fp16 fits this 16 GB GPU. FLUX.1-dev bf16 does not, so item icons stay on SDXL.
ITEM_ICON_MODEL = "stabilityai/stable-diffusion-xl-base-1.0"
ITEM_ICON_LORA = "nerijs/pixel-art-xl"
ITEM_ICON_LORA_FILE = "pixel-art-xl.safetensors"
ITEM_ICON_NEGATIVE = (
    "text, letters, numbers, watermark, logo, signature, person, face, human, character, "
    "body, landscape, scenery, forest, room, grid, tiles, pattern, border, frame, "
    "sprite sheet, tileset, item sheet, collage, inventory page, many objects, "
    "repeated objects, multiple copies, photo, blurry, 3d render of a scene"
)
ITEM_ICON_SUBJECTS = {
    "weapon": "one steel longsword standing upright",
    "offhand": "one round steel kite shield with a gold center boss",
    "head": "one closed steel knight helmet, empty visor, no face",
    "shoulders": "two floating steel pauldrons only, curved shoulder armor, no body, no head, no person",
    "chest": "one medieval steel cuirass, front view, empty armor, no person, no wall",
    "hands": "exactly two brown leather gloves and nothing else",
    "waist": "one brown leather belt with one gold buckle, laid flat",
    "legs": "exactly two steel leg greaves standing side by side, not a shield, not a breastplate",
    "feet": "one pair of brown leather boots",
    "amulet": "one gold necklace with a blue gem pendant",
    "ring": "one gold ring with a red gem",
    "cloak": "exactly one purple hooded cloak, front view, empty hood, no copies",
    "charm": "one gold talisman tablet with a small blue gem",
    "relic": "one green crystal relic, a single object",
    "potion": "exactly one corked red potion flask, a single bottle, no other bottles",
    "gem": "exactly one green faceted gemstone and no other gems",
}
ITEM_ICON_SEEDS = {
    "weapon": 1601,
    "offhand": 1602,
    "head": 1203,
    "shoulders": 4204,
    "chest": 2305,
    "hands": 1606,
    "waist": 1607,
    "legs": 2208,
    "feet": 1209,
    "amulet": 1210,
    "ring": 1211,
    "cloak": 1612,
    "charm": 1613,
    "relic": 1614,
    "potion": 2215,
    "gem": 1616,
}


def _erase_border_frames(image: Image.Image) -> None:
    """Drop a rectangular stroke around the backdrop. Keep the object in the middle."""
    width, height = image.size
    pixels = image.load()
    seen = bytearray(width * height)
    band = max(3, int(min(width, height) * 0.035))

    def on_border(x: int, y: int) -> bool:
        return x < band or y < band or x >= width - band or y >= height - band

    for y in range(height):
        for x in range(width):
            index = y * width + x
            if seen[index] or pixels[x, y][3] <= 16:
                continue
            queue: deque[tuple[int, int]] = deque([(x, y)])
            seen[index] = 1
            coords: list[tuple[int, int]] = []
            near = 0
            while queue:
                cx, cy = queue.popleft()
                coords.append((cx, cy))
                if on_border(cx, cy):
                    near += 1
                for nx, ny in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    neighbor = ny * width + nx
                    if seen[neighbor] or pixels[nx, ny][3] <= 16:
                        continue
                    seen[neighbor] = 1
                    queue.append((nx, ny))
            if len(coords) >= 24 and near / len(coords) > 0.82:
                for cx, cy in coords:
                    red, green, blue, _alpha = pixels[cx, cy]
                    pixels[cx, cy] = (red, green, blue, 0)


def _erase_light_scraps(image: Image.Image) -> None:
    """Remove a leftover white floor. Highlights attached to the object stay."""
    width, height = image.size
    pixels = image.load()
    seen = bytearray(width * height)
    groups: list[list[tuple[int, int]]] = []
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if seen[index] or pixels[x, y][3] <= 16:
                continue
            queue: deque[tuple[int, int]] = deque([(x, y)])
            seen[index] = 1
            coords: list[tuple[int, int]] = []
            while queue:
                cx, cy = queue.popleft()
                coords.append((cx, cy))
                for nx, ny in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    neighbor = ny * width + nx
                    if seen[neighbor] or pixels[nx, ny][3] <= 16:
                        continue
                    seen[neighbor] = 1
                    queue.append((nx, ny))
            if len(coords) >= 20:
                groups.append(coords)
    if len(groups) < 2:
        return
    largest = max(len(group) for group in groups)
    for coords in groups:
        if len(coords) > largest * 0.45:
            continue
        sample = coords[:: max(1, len(coords) // 24)]
        luma = sum(sum(pixels[x, y][:3]) / 3 for x, y in sample) / len(sample)
        chroma = sum(
            max(pixels[x, y][:3]) - min(pixels[x, y][:3]) for x, y in sample
        ) / len(sample)
        if luma > 200 and chroma < 28:
            for cx, cy in coords:
                red, green, blue, _alpha = pixels[cx, cy]
                pixels[cx, cy] = (red, green, blue, 0)


def _erase_white_floor(image: Image.Image) -> None:
    """Clear a leftover white strip under the object without eating pale highlights."""
    width, height = image.size
    pixels = image.load()
    for y in range(height - 1, int(height * 0.45), -1):
        opaque = 0
        white = 0
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha <= 16:
                continue
            opaque += 1
            if (red + green + blue) / 3 > 210 and max(red, green, blue) - min(red, green, blue) < 30:
                white += 1
        if opaque < width * 0.12:
            continue
        if opaque == 0 or white / opaque <= 0.65:
            break
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha <= 16:
                continue
            if (red + green + blue) / 3 > 210 and max(red, green, blue) - min(red, green, blue) < 30:
                pixels[x, y] = (red, green, blue, 0)


def _opaque_pieces(image: Image.Image) -> tuple[int, float]:
    """Count separated shapes. Sprite sheets fail; one item, or a pair, passes."""
    probe = image.resize((96, 96), Image.Resampling.NEAREST)
    alpha = probe.getchannel("A")
    width, height = probe.size
    pixels = alpha.load()
    seen = bytearray(width * height)
    sizes: list[int] = []
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if seen[index] or pixels[x, y] <= 24:
                continue
            queue: deque[tuple[int, int]] = deque([(x, y)])
            seen[index] = 1
            size = 0
            while queue:
                cx, cy = queue.popleft()
                size += 1
                for nx, ny in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    neighbor = ny * width + nx
                    if seen[neighbor] or pixels[nx, ny] <= 24:
                        continue
                    seen[neighbor] = 1
                    queue.append((nx, ny))
            if size >= 18:
                sizes.append(size)
    if not sizes:
        return 0, 0.0
    total = sum(sizes)
    return len(sizes), max(sizes) / float(total)


def _backdrop_is_plain(rgb: Image.Image) -> tuple[int, int, int] | None:
    """Return the backdrop color when the inner margin is one light flat color."""
    width, height = rgb.size
    inset_x = max(8, width // 12)
    inset_y = max(8, height // 12)
    samples: list[tuple[int, int, int]] = []
    x_step = max(1, (width - 2 * inset_x) // 10)
    y_step = max(1, (height - 2 * inset_y) // 10)
    for y in range(inset_y, height - inset_y, y_step):
        for x in range(inset_x, width - inset_x, x_step):
            on_margin = (
                x < inset_x * 2
                or x > width - inset_x * 2
                or y < inset_y * 2
                or y > height - inset_y * 2
            )
            if on_margin:
                samples.append(rgb.getpixel((x, y)))
    if len(samples) < 8:
        return None
    color = tuple(sorted(pixel[channel] for pixel in samples)[len(samples) // 2] for channel in range(3))
    if sum(color) / 3 < 140:
        return None
    close = sum(
        1
        for red, green, blue in samples
        if max(abs(red - color[0]), abs(green - color[1]), abs(blue - color[2])) <= 32
    )
    if close / len(samples) < 0.62:
        return None
    return color  # type: ignore[return-value]


def cut_plain_backdrop(image: Image.Image, cell: int) -> Image.Image | None:
    """Keep one centered object. Reject scenes, grids, and full-bleed paintings."""
    rgb = image.convert("RGB")
    backdrop = _backdrop_is_plain(rgb)
    if backdrop is None:
        print("  reject: background is not a plain light color")
        return None
    width, height = rgb.size
    source = rgb.load()
    def is_backdrop(x: int, y: int) -> bool:
        red, green, blue = source[x, y]
        return max(
            abs(red - backdrop[0]),
            abs(green - backdrop[1]),
            abs(blue - backdrop[2]),
        ) <= 24

    rgba = image.convert("RGBA")
    pixels = rgba.load()
    seen = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()
    for x in range(width):
        queue.append((x, 0))
        queue.append((x, height - 1))
    for y in range(height):
        queue.append((0, y))
        queue.append((width - 1, y))
    # Start inside a thin dark frame so a gray studio backdrop still keys out.
    margin = max(4, width // 14)
    for y in range(0, height, 6):
        queue.append((margin, y))
        queue.append((width - 1 - margin, y))
    for x in range(0, width, 6):
        queue.append((x, margin))
        queue.append((x, height - 1 - margin))
    while queue:
        x, y = queue.popleft()
        index = y * width + x
        if seen[index]:
            continue
        seen[index] = 1
        if not is_backdrop(x, y):
            continue
        red, green, blue, _alpha = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        if x > 0:
            queue.append((x - 1, y))
        if x + 1 < width:
            queue.append((x + 1, y))
        if y > 0:
            queue.append((x, y - 1))
        if y + 1 < height:
            queue.append((x, y + 1))

    _erase_border_frames(rgba)
    _erase_light_scraps(rgba)
    _erase_white_floor(rgba)
    alpha = rgba.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        print("  reject: nothing left after removing the background")
        return None
    left, top, right, bottom = bbox
    touches_every_edge = left <= 2 and top <= 2 and right >= width - 2 and bottom >= height - 2
    opaque = sum(1 for value in alpha.getdata() if value > 16)
    ratio = opaque / float(width * height)
    if ratio < 0.035 or ratio > 0.82:
        print(f"  reject: coverage {ratio:.2f}")
        return None
    pieces, largest_share = _opaque_pieces(rgba)
    # A sprite sheet is many separate blobs. A pair of boots or gloves is two.
    if pieces > 3 or largest_share < 0.42:
        print(f"  reject: pieces={pieces} largest={largest_share:.2f}")
        return None
    if touches_every_edge and pieces > 2 and ratio > 0.72:
        print(f"  reject: full-frame pieces={pieces} coverage={ratio:.2f}")
        return None
    cropped = rgba.crop(bbox)
    canvas = Image.new("RGBA", (cell, cell), (0, 0, 0, 0))
    margin = max(3, cell // 16)
    max_side = cell - margin * 2
    scale = min(max_side / cropped.width, max_side / cropped.height)
    fitted = cropped.resize(
        (max(1, int(cropped.width * scale)), max(1, int(cropped.height * scale))),
        Image.Resampling.NEAREST,
    )
    canvas.paste(
        fitted,
        ((cell - fitted.width) // 2, (cell - fitted.height) // 2),
        fitted,
    )
    return canvas


def _write_icon_cell(sheet: Image.Image, frame: dict, cell: Image.Image) -> None:
    sheet_pixels = sheet.load()
    cell_pixels = cell.load()
    for py in range(cell.height):
        for px in range(cell.width):
            sheet_pixels[frame["x"] + px, frame["y"] + py] = cell_pixels[px, py]


def diffuse_item_icons(only_names: str = "") -> None:
    """Paint each gear cell with SDXL plus a game-art LoRA. Keep the painted cell on failure."""
    try:
        import torch
        from diffusers import StableDiffusionXLPipeline
    except Exception as exc:  # noqa: BLE001
        print(f"Item icon diffusion skipped: {exc}")
        return
    if not torch.cuda.is_available():
        print("Item icon diffusion skipped: CUDA is not available.")
        return

    atlas_path = OUT / "items_atlas.png"
    if not atlas_path.exists():
        build_items()
    sheet = Image.open(atlas_path).convert("RGBA")
    print(f"Item icon device: {torch.cuda.get_device_name(0)} model: {ITEM_ICON_MODEL}")
    try:
        pipe = StableDiffusionXLPipeline.from_pretrained(
            ITEM_ICON_MODEL,
            dtype=torch.float16,
            variant="fp16",
            use_safetensors=True,
        )
        pipe = pipe.to("cuda")
    except Exception as exc:  # noqa: BLE001
        print(f"Item icon diffusion skipped: {exc}")
        return

    lora_ready = False
    try:
        pipe.load_lora_weights(
            ITEM_ICON_LORA,
            weight_name=ITEM_ICON_LORA_FILE,
            adapter_name="pixel_art",
        )
        pipe.set_adapters(["pixel_art"], adapter_weights=[0.6])
        lora_ready = True
        print(f"Item icon LoRA loaded: {ITEM_ICON_LORA}")
    except Exception as exc:  # noqa: BLE001
        print(f"Item icon LoRA skipped, SDXL base only: {exc}")

    meta = json.loads((OUT / "items_atlas.json").read_text(encoding="utf-8"))
    selected = {name.strip() for name in only_names.split(",") if name.strip()}
    passes = (
        (
            0.0,
            "one {subject}, dark fantasy game item, centered, isolated, "
            "plain light gray background, no other objects",
            7.0,
        ),
        (
            0.6,
            "a single large pixel-art {subject}, close-up, filling most of the frame, "
            "flat colors, exactly one object, centered, solid light gray background",
            6.0,
        ),
        (
            0.0,
            "macro photo of one {subject}, centered, large, plain light gray background",
            7.5,
        ),
    )
    for frame in meta["frames"]:
        name = frame["name"]
        subject = ITEM_ICON_SUBJECTS.get(name)
        if subject is None or (selected and name not in selected):
            continue
        cell_size = int(frame["w"])
        accepted = None
        for attempt, (lora_scale, template, guidance) in enumerate(passes):
            if lora_ready:
                pipe.set_adapters(["pixel_art"], adapter_weights=[lora_scale])
            prompt = template.format(subject=subject)
            seed = ITEM_ICON_SEEDS[name] + attempt * 80
            try:
                result = pipe(
                    prompt,
                    negative_prompt=ITEM_ICON_NEGATIVE,
                    num_inference_steps=28,
                    guidance_scale=guidance,
                    width=1024,
                    height=1024,
                    generator=torch.Generator(device="cuda").manual_seed(seed),
                )
                icon = result.images[0]
            except Exception as exc:  # noqa: BLE001
                print(f"Item {name} diffusion skipped: {exc}")
                break
            if frame_is_blank(icon):
                print(f"Item {name} attempt {attempt + 1} was blank.")
                continue
            accepted = cut_plain_backdrop(icon, cell_size)
            if accepted is not None:
                break
            print(f"Item {name} attempt {attempt + 1} was not a single icon.")
        if accepted is None:
            print(f"Item {name} kept the procedural icon.")
            continue
        _write_icon_cell(sheet, frame, accepted)
        print(f"Item {name} diffusion packed.")

    sheet.save(atlas_path)
    print(f"Item atlas updated at {atlas_path}")


def build_shadow() -> None:
    image = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((4, 4, 60, 28), fill=(0, 0, 0, 150))
    image.save(OUT / "shadow_blob.png")


def build_procedural() -> Image.Image:
    OUT.mkdir(parents=True, exist_ok=True)
    warrior, warrior_meta = build_sheet(WARRIOR_CLIPS, paint_warrior_cell)
    write_directional(OUT / "warrior_atlas.png", OUT / "warrior_atlas.json", warrior, warrior_meta)
    monster, monster_meta = build_sheet(MONSTER_CLIPS, paint_monster_cell)
    write_directional(OUT / "monster_atlas.png", OUT / "monster_atlas.json", monster, monster_meta)
    build_ui_atlas()
    build_cursor()
    build_items()
    build_shadow()
    paint_menu_background()
    print(f"Procedural atlases written to {OUT}")
    return warrior


def frame_is_blank(frame: Image.Image) -> bool:
    """Safety filters return a solid black image. Keep the procedural cell instead."""
    sample = frame.convert("L").resize((8, 8))
    pixels = sample.get_flattened_data() if hasattr(sample, "get_flattened_data") else sample.getdata()
    return sum(pixels) / 64.0 < 8.0


def overlay_idle_frames(sheet: Image.Image, frames: list[Image.Image]) -> None:
    """Refuse a one-still overlay. The idle loop must stay one character."""
    del sheet, frames
    print(
        "Warrior idle diffusion skipped: one still per direction would pop "
        "against the procedural idle loop."
    )


def try_diffusion(sheet: Image.Image) -> None:
    # A diffusion still in column 0 of a 4-frame idle loop swaps the hero for a
    # different painting, then back. Keep the procedural clip intact.
    overlay_idle_frames(sheet, [])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--no-diffusion",
        action="store_true",
        help="Write procedural atlases only. Skip torch and model download.",
    )
    parser.add_argument(
        "--diffusion-worker",
        action="store_true",
        help="Kept for older callers. Does not overwrite the procedural warrior sheet.",
    )
    parser.add_argument(
        "--menu-background",
        action="store_true",
        help="Write only the main-menu background, then try one diffusion plate.",
    )
    parser.add_argument(
        "--items-only",
        action="store_true",
        help="Write only the item icon atlas. Does not touch the menu background.",
    )
    parser.add_argument(
        "--items-diffuse",
        action="store_true",
        help="With --items-only, replace each item cell using SDXL and a game-art LoRA.",
    )
    parser.add_argument(
        "--item-names",
        default="",
        help="Comma-separated atlas frame names to diffuse. Skips the procedural reset.",
    )
    args = parser.parse_args()
    if args.items_only:
        OUT.mkdir(parents=True, exist_ok=True)
        if not args.item_names:
            build_items()
        if args.items_diffuse or args.item_names:
            diffuse_item_icons(args.item_names)
        return
    if args.menu_background:
        OUT.mkdir(parents=True, exist_ok=True)
        paint_menu_background()
        diffuse_menu_background()
        return
    if args.diffusion_worker:
        sheet = Image.open(OUT / "warrior_atlas.png")
        try_diffusion(sheet)
        return
    sheet = build_procedural()
    if args.no_diffusion:
        print("Diffusion disabled (--no-diffusion).")
        return
    try_diffusion(sheet)
    diffuse_menu_background()


if __name__ == "__main__":
    main()
