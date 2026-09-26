#!/usr/bin/env python3
"""Generate the static town backdrop and building plates.

The engine loads the PNGs in ``assets/textures/town/``. This script writes those
same paths. It uses the SDXL or Flux LoRA stack from
``scripts/generate_rpg_atlases_sd.py``. Prompts are original. No third-party
game art is copied.

One command on the WSL RTX 5080 (repo at /home/dev/projects/cppGame)::

  cd /home/dev/projects/cppGame
  python3 -m venv .venv-diffusion
  source .venv-diffusion/bin/activate
  python -m pip install -U pip
  python -m pip install -r scripts/requirements-diffusion.txt
  python scripts/generate_town_sd.py --backend sdxl

Flux (after the license click and ``huggingface-cli login``)::

  python scripts/generate_town_sd.py --backend flux --cpu-offload

``--dry-run`` prints model ids and prompts. ``--placeholders`` rewrites the
committed pixel-art stand-ins with Pillow and does not need CUDA. A GPU run
overwrites those files.

SDXL and Flux paint an opaque canvas. ``--cutouts`` (no GPU) punches the flat
sky and gray mats, keeps the largest sprite, and crops it so the plates blend
over the dusk backdrop. A GPU run does that step automatically. Reprocess the
committed plates on any machine with Pillow::

  python scripts/generate_town_sd.py --cutouts
"""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "town"
CACHE = ROOT / "assets" / "textures" / "generated" / "town_src"

# Same public stack as scripts/generate_rpg_atlases_sd.py.
SDXL_MODEL = "stabilityai/stable-diffusion-xl-base-1.0"
SDXL_PIXEL_LORA = "nerijs/pixel-art-xl"
SDXL_PIXEL_FILE = "pixel-art-xl.safetensors"
SDXL_PIXEL_TRIGGER = "pixel"
SDXL_PIXEL_WEIGHT = 0.90
SDXL_FANTASY_LORA = "ntc-ai/SDXL-LoRA-slider.fantasy"
SDXL_FANTASY_FILE = "fantasy.safetensors"
SDXL_FANTASY_TRIGGER = "fantasy"
SDXL_FANTASY_WEIGHT = 1.50

FLUX_MODEL = "black-forest-labs/FLUX.1-dev"
FLUX_LORA = "AIGCDuckBoss/fluxLora_pixelRPG"
FLUX_LORA_FILE = "fluxLora_pixelrpg.safetensors"
FLUX_TRIGGER = "The overall style of the illustration is colorful pixel style"
FLUX_LORA_WEIGHT = 0.85

NEGATIVE = (
    "text, letters, numbers, watermark, logo, signature, caption, ui, minimap, "
    "frame, border, label, collage, photo, blurry, noise, speckles, dither, "
    "jpeg artifacts, modern city, cars, people crowd, face closeup"
)

STYLE = (
    "pixel, fantasy, dark fantasy action RPG location, hand-painted pixel art, "
    "dusk, rich color, clean shapes, no text"
)

# name, prompt subject, width, height, seed, key_background
JOBS = (
    (
        "backdrop",
        "wide dusk meadow outside a ruined fantasy town, indigo sky fading to amber, "
        "large moon, soft hills, cobbled plaza, warm lanterns, no buildings in front, no people",
        1280,
        720,
        5101,
        False,
    ),
    (
        "forge_ruined",
        "one ruined stone blacksmith forge, collapsed roof, cold dark windows, rubble, "
        "isolated building centered, flat solid gray background",
        768,
        1024,
        5102,
        True,
    ),
    (
        "forge_repaired",
        "one restored fantasy blacksmith forge, intact timber roof, blazing orange forge mouth, "
        "warm windows, chimney smoke, isolated building centered, flat solid gray background",
        768,
        1024,
        5103,
        True,
    ),
    (
        "chapel_ruined",
        "one ruined stone chapel, broken steeple, dark empty windows, rubble at the door, "
        "isolated building centered, flat solid gray background",
        768,
        1024,
        5104,
        True,
    ),
    (
        "chapel_repaired",
        "one restored stone chapel at dusk, tall steeple, warm rose window, gold lantern light, "
        "isolated building centered, flat solid gray background",
        768,
        1024,
        5105,
        True,
    ),
    (
        "tavern_ruined",
        "one ruined timber tavern, sagging roof, dark boarded windows, broken hanging sign, "
        "isolated building centered, flat solid gray background",
        768,
        1024,
        5106,
        True,
    ),
    (
        "tavern_repaired",
        "one cozy restored fantasy tavern, red roof, glowing windows, hanging lantern, "
        "isolated building centered, flat solid gray background",
        768,
        1024,
        5107,
        True,
    ),
    (
        "road",
        "a low stone town gate and cobbled road leading away at dusk, two lanterns, "
        "wide banner composition, no text, flat solid gray background",
        1280,
        320,
        5108,
        True,
    ),
)


def prompt_for(backend: str, subject: str) -> str:
    if backend == "flux":
        return f"{FLUX_TRIGGER}. {subject}. {STYLE}."
    return f"{SDXL_PIXEL_TRIGGER}, {SDXL_FANTASY_TRIGGER}, {subject}, {STYLE}"


def require_pillow():
    try:
        from PIL import Image, ImageDraw
    except ImportError as exc:  # pragma: no cover
        raise SystemExit("Pillow is required: python -m pip install pillow") from exc
    return Image, ImageDraw


def _block(draw, x: int, y: int, w: int, h: int, color, scale: int) -> None:
    draw.rectangle([x * scale, y * scale, (x + w) * scale - 1, (y + h) * scale - 1], fill=color)


def _poly(draw, points, color, scale: int) -> None:
    draw.polygon([(x * scale, y * scale) for x, y in points], fill=color)


def paint_backdrop(Image, ImageDraw):
    scale = 3
    w, h = 320, 180
    image = Image.new("RGBA", (w * scale, h * scale), (18, 16, 36, 255))
    draw = ImageDraw.Draw(image)
    sky_top = (28, 24, 62)
    sky_mid = (92, 48, 86)
    sky_low = (214, 112, 68)
    for y in range(118):
        t = y / 118
        if t < 0.55:
            u = t / 0.55
            color = tuple(int(sky_top[i] + (sky_mid[i] - sky_top[i]) * u) for i in range(3))
        else:
            u = (t - 0.55) / 0.45
            color = tuple(int(sky_mid[i] + (sky_low[i] - sky_mid[i]) * u) for i in range(3))
        _block(draw, 0, y, w, 1, color + (255,), scale)
    stars = (
        (18, 12), (40, 28), (66, 10), (90, 36), (120, 16), (148, 8), (170, 30),
        (210, 14), (236, 40), (40, 48), (200, 22), (280, 18), (300, 34), (110, 44),
    )
    for x, y in stars:
        _block(draw, x, y, 1, 1, (236, 228, 196, 255), scale)
    _block(draw, 248, 18, 22, 22, (244, 226, 186, 255), scale)
    _block(draw, 254, 22, 6, 6, (220, 196, 140, 255), scale)
    _poly(draw, [(0, 108), (40, 96), (90, 104), (150, 92), (210, 102), (270, 94), (320, 106), (320, 130), (0, 130)], (42, 36, 64, 255), scale)
    _poly(draw, [(0, 122), (70, 110), (140, 118), (220, 108), (320, 120), (320, 140), (0, 140)], (32, 40, 36, 255), scale)
    for y in range(128, 180):
        grass = (46, 68, 40, 255) if y < 150 else (34, 50, 32, 255)
        _block(draw, 0, y, w, 1, grass, scale)
    for x in range(28, 292, 18):
        for y in range(136, 168, 12):
            stone = (112, 78, 48, 255) if ((x // 18 + y // 12) % 2) == 0 else (86, 60, 38, 255)
            _block(draw, x, y, 16, 10, stone, scale)
    for lamp_x in (70, 160, 250):
        _block(draw, lamp_x, 128, 2, 22, (48, 36, 28, 255), scale)
        _block(draw, lamp_x - 2, 124, 6, 6, (255, 186, 84, 255), scale)
    return image


def _house(draw, scale, restored: bool, body, roof, accent, steeple: bool, wide_sign: bool) -> None:
    _poly(draw, [(48, 112), (22, 118), (74, 118)], (16, 12, 10, 140), scale)
    _poly(draw, [(20, 58), (48, 28), (76, 58)], roof, scale)
    _block(draw, 24, 56, 48, 52, body, scale)
    _block(draw, 22, 56, 52, 3, accent, scale)
    if steeple:
        _block(draw, 45, 8, 6, 24, accent, scale)
        _block(draw, 40, 12, 16, 3, accent, scale)
    if not restored:
        _block(draw, 30, 30, 10, 8, (0, 0, 0, 0), scale)
        _block(draw, 58, 86, 14, 16, (42, 36, 34, 255), scale)
    window = (255, 168, 64, 255) if restored else (18, 14, 20, 255)
    _block(draw, 30, 66, 10, 12, (28, 22, 18, 255), scale)
    _block(draw, 32, 68, 6, 8, window, scale)
    _block(draw, 56, 66, 10, 12, (28, 22, 18, 255), scale)
    _block(draw, 58, 68, 6, 8, window, scale)
    door = (92, 52, 32, 255) if restored else (36, 26, 22, 255)
    _block(draw, 42, 82, 12, 26, door, scale)
    if wide_sign:
        _block(draw, 34, 48, 28, 8, (72, 46, 28, 255), scale)
        if restored:
            _block(draw, 46, 50, 4, 4, (180, 42, 46, 255), scale)
    if restored:
        _block(draw, 62, 18, 4, 14, (70, 64, 60, 255), scale)
        _block(draw, 66, 14, 5, 4, (210, 206, 198, 180), scale)
        _block(draw, 70, 10, 4, 3, (210, 206, 198, 120), scale)


def paint_forge(Image, ImageDraw, restored: bool):
    image = Image.new("RGBA", (96 * 4, 128 * 4), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    body = (150, 108, 82, 255) if restored else (108, 102, 98, 255)
    roof = (96, 42, 34, 255) if restored else (72, 64, 60, 255)
    _house(draw, 4, restored, body, roof, (62, 48, 40, 255), False, False)
    if restored:
        _block(draw, 44, 90, 8, 10, (255, 120, 36, 255), 4)
    else:
        _block(draw, 28, 100, 16, 6, (62, 56, 52, 255), 4)
    return image


def paint_chapel(Image, ImageDraw, restored: bool):
    image = Image.new("RGBA", (96 * 4, 128 * 4), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    body = (186, 170, 142, 255) if restored else (118, 112, 118, 255)
    roof = (92, 58, 86, 255) if restored else (64, 56, 68, 255)
    _house(draw, 4, restored, body, roof, (72, 58, 48, 255), True, False)
    if restored:
        _block(draw, 44, 62, 8, 14, (255, 214, 140, 255), 4)
    return image


def paint_tavern(Image, ImageDraw, restored: bool):
    image = Image.new("RGBA", (96 * 4, 128 * 4), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    body = (132, 64, 54, 255) if restored else (96, 72, 68, 255)
    roof = (132, 36, 40, 255) if restored else (70, 44, 46, 255)
    _house(draw, 4, restored, body, roof, (48, 28, 26, 255), False, True)
    if restored:
        _block(draw, 18, 60, 6, 6, (255, 186, 84, 255), 4)
    return image


def paint_road(Image, ImageDraw):
    scale = 4
    w, h = 240, 56
    image = Image.new("RGBA", (w * scale, h * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    for y in range(18, 56, 8):
        for x in range(0, 240, 12):
            stone = (124, 96, 68, 255) if ((x // 12 + y // 8) % 2) == 0 else (86, 62, 42, 255)
            _block(draw, x, y, 11, 7, stone, scale)
    _block(draw, 36, 8, 8, 48, (58, 42, 32, 255), scale)
    _block(draw, 196, 8, 8, 48, (58, 42, 32, 255), scale)
    _block(draw, 36, 8, 168, 8, (112, 74, 42, 255), scale)
    _block(draw, 34, 4, 12, 8, (255, 186, 84, 255), scale)
    _block(draw, 194, 4, 12, 8, (255, 186, 84, 255), scale)
    _block(draw, 96, 28, 36, 3, (236, 206, 120, 255), scale)
    _poly(draw, [(132, 22), (148, 29), (132, 36)], (236, 206, 120, 255), scale)
    return image


def write_placeholders() -> None:
    Image, ImageDraw = require_pillow()
    OUT.mkdir(parents=True, exist_ok=True)
    files = {
        "backdrop.png": paint_backdrop(Image, ImageDraw),
        "forge_ruined.png": paint_forge(Image, ImageDraw, False),
        "forge_repaired.png": paint_forge(Image, ImageDraw, True),
        "chapel_ruined.png": paint_chapel(Image, ImageDraw, False),
        "chapel_repaired.png": paint_chapel(Image, ImageDraw, True),
        "tavern_ruined.png": paint_tavern(Image, ImageDraw, False),
        "tavern_repaired.png": paint_tavern(Image, ImageDraw, True),
        "road.png": paint_road(Image, ImageDraw),
    }
    for name, image in files.items():
        path = OUT / name
        image.save(path)
        print(f"wrote {path} {image.size[0]}x{image.size[1]}")


def key_gray(image, tolerance: int = 42):
    image = image.convert("RGBA")
    pixels = image.load()
    width, height = image.size
    corners = [pixels[1, 1], pixels[width - 2, 1], pixels[1, height - 2], pixels[width - 2, height - 2]]
    key = tuple(sum(pixel[channel] for pixel in corners) // 4 for channel in range(3))
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if abs(red - key[0]) + abs(green - key[1]) + abs(blue - key[2]) <= tolerance:
                pixels[x, y] = (0, 0, 0, 0)
    return image


def _is_matte(red: int, green: int, blue: int, road: bool) -> bool:
    """Flat sky, paper-white, and (for the road plate) the gray studio slab."""
    lum = (red + green + blue) / 3.0
    sat = max(red, green, blue) - min(red, green, blue)
    if lum >= 145 and sat <= 34:
        return True
    if blue >= 165 and blue >= red + 12 and lum >= 150 and sat <= 90:
        return True
    if road and lum >= 108 and sat <= 20:
        return True
    return False


def isolate_plate(image, road: bool = False, pad: int = 6, crop: bool = False):
    """Punch studio backgrounds and keep the largest connected sprite.

    Diffusion plates ship as opaque rectangles. Corner chroma-key misses the
    light-blue sky boxed inside the building, and extra doors stay as floating
    cutouts. This clears those mats everywhere and drops the smaller pieces.
    The canvas size stays put so the engine can keep stretching each plate
    into its hotspot. ``crop`` is only for previews.
    """
    image = image.convert("RGBA")
    width, height = image.size
    raw = bytearray(image.tobytes())
    count = width * height

    for pixel in range(count):
        index = pixel * 4
        alpha = raw[index + 3]
        if alpha < 20 or _is_matte(raw[index], raw[index + 1], raw[index + 2], road):
            raw[index : index + 4] = b"\x00\x00\x00\x00"

    seen = bytearray(count)
    best_pixels: list[int] = []
    for start in range(count):
        if seen[start] or raw[start * 4 + 3] < 48:
            seen[start] = 1
            continue
        stack = [start]
        seen[start] = 1
        component: list[int] = []
        min_x = width
        min_y = height
        max_x = 0
        max_y = 0
        while stack:
            pixel = stack.pop()
            component.append(pixel)
            x = pixel % width
            y = pixel // width
            if x < min_x:
                min_x = x
            if y < min_y:
                min_y = y
            if x > max_x:
                max_x = x
            if y > max_y:
                max_y = y
            for neighbor in (pixel - 1, pixel + 1, pixel - width, pixel + width):
                if neighbor < 0 or neighbor >= count or seen[neighbor]:
                    continue
                if neighbor == pixel - 1 and x == 0:
                    continue
                if neighbor == pixel + 1 and x == width - 1:
                    continue
                if raw[neighbor * 4 + 3] < 48:
                    seen[neighbor] = 1
                    continue
                seen[neighbor] = 1
                stack.append(neighbor)
        if len(component) > len(best_pixels):
            best_pixels = component
            best_box = (min_x, min_y, max_x, max_y)

    Image, _draw = require_pillow()
    if len(best_pixels) < 64:
        return Image.frombytes("RGBA", (width, height), bytes(raw))

    keep = bytearray(count)
    for pixel in best_pixels:
        keep[pixel] = 1
        x = pixel % width
        y = pixel // width
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height and raw[(ny * width + nx) * 4 + 3] > 0:
                keep[ny * width + nx] = 1
    for pixel in range(count):
        if not keep[pixel]:
            raw[pixel * 4 : pixel * 4 + 4] = b"\x00\x00\x00\x00"

    plate = Image.frombytes("RGBA", (width, height), bytes(raw))
    if not crop:
        return plate
    min_x, min_y, max_x, max_y = best_box
    left = max(0, min_x - pad)
    top = max(0, min_y - pad)
    right = min(width, max_x + pad + 1)
    bottom = min(height, max_y + pad + 1)
    return plate.crop((left, top, right, bottom))


def write_cutouts() -> None:
    Image, _draw = require_pillow()
    for name, _subject, _width, _height, _seed, keyed in JOBS:
        if not keyed:
            continue
        path = OUT / f"{name}.png"
        if not path.exists():
            raise SystemExit(f"missing {path}; generate or restore the town plates first")
        image = isolate_plate(Image.open(path), road=(name == "road"))
        image.save(path)
        opaque = sum(1 for pixel in image.get_flattened_data() if pixel[3] >= 48)
        print(f"cutout {path.name} {image.size[0]}x{image.size[1]} opaque {opaque}")


def run_generation(backend: str, steps: int, cpu_offload: bool) -> None:
    try:
        import torch
        from diffusers import FluxPipeline, StableDiffusionXLPipeline
    except ImportError as exc:
        raise SystemExit(
            "torch and diffusers are not installed. On the RTX 5080:\n"
            "  cd /home/dev/projects/cppGame\n"
            "  python3 -m venv .venv-diffusion && source .venv-diffusion/bin/activate\n"
            "  python -m pip install -r scripts/requirements-diffusion.txt\n"
            "  python scripts/generate_town_sd.py --backend sdxl\n"
            f"Import error: {exc}"
        ) from exc
    if not torch.cuda.is_available():
        raise SystemExit(
            "CUDA is not available. Run this on the RTX 5080:\n"
            "  cd /home/dev/projects/cppGame\n"
            "  source .venv-diffusion/bin/activate\n"
            "  python scripts/generate_town_sd.py --backend sdxl\n"
            "The committed PNGs stay in assets/textures/town until that run overwrites them."
        )
    Image, _draw = require_pillow()
    dtype = torch.float16 if backend == "sdxl" else torch.bfloat16
    if backend == "flux":
        print(f"Loading {FLUX_MODEL}")
        pipe = FluxPipeline.from_pretrained(FLUX_MODEL, torch_dtype=dtype)
        pipe.load_lora_weights(FLUX_LORA, weight_name=FLUX_LORA_FILE)
        try:
            pipe.fuse_lora(lora_scale=FLUX_LORA_WEIGHT)
        except TypeError:
            pipe.fuse_lora()
    else:
        print(f"Loading {SDXL_MODEL}")
        pipe = StableDiffusionXLPipeline.from_pretrained(
            SDXL_MODEL, torch_dtype=dtype, variant="fp16", use_safetensors=True
        )
        pipe.load_lora_weights(SDXL_PIXEL_LORA, weight_name=SDXL_PIXEL_FILE, adapter_name="pixel")
        pipe.load_lora_weights(SDXL_FANTASY_LORA, weight_name=SDXL_FANTASY_FILE, adapter_name="fantasy")
        pipe.set_adapters(["pixel", "fantasy"], adapter_weights=[SDXL_PIXEL_WEIGHT, SDXL_FANTASY_WEIGHT])
    if cpu_offload:
        pipe.enable_model_cpu_offload()
    else:
        pipe.to("cuda")
    OUT.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    for name, subject, width, height, seed, keyed in JOBS:
        cache_path = CACHE / f"{backend}_{name}.png"
        if cache_path.exists():
            image = Image.open(cache_path).convert("RGBA")
            print(f"cache {name}")
        else:
            prompt = prompt_for(backend, subject)
            print(f"gen {name} {width}x{height} seed {seed}")
            generator = torch.Generator(device="cuda").manual_seed(seed)
            kwargs = {
                "prompt": prompt,
                "num_inference_steps": steps,
                "generator": generator,
                "width": width,
                "height": height,
            }
            if backend == "sdxl":
                kwargs["guidance_scale"] = 6.0
                kwargs["negative_prompt"] = NEGATIVE
            else:
                kwargs["guidance_scale"] = 3.5
            image = pipe(**kwargs).images[0].convert("RGBA")
            image.save(cache_path)
        if keyed:
            image = isolate_plate(key_gray(image), road=(name == "road"))
        path = OUT / f"{name}.png"
        image.save(path)
        print(f"wrote {path}")


def print_plan(backend: str, steps: int) -> None:
    print(f"backend {backend}")
    if backend == "flux":
        print(f"base {FLUX_MODEL}")
        print(f"lora {FLUX_LORA} file {FLUX_LORA_FILE} weight {FLUX_LORA_WEIGHT}")
    else:
        print(f"base {SDXL_MODEL}")
        print(f"lora {SDXL_PIXEL_LORA} file {SDXL_PIXEL_FILE} trigger {SDXL_PIXEL_TRIGGER} weight {SDXL_PIXEL_WEIGHT}")
        print(f"lora {SDXL_FANTASY_LORA} file {SDXL_FANTASY_FILE} trigger {SDXL_FANTASY_TRIGGER} weight {SDXL_FANTASY_WEIGHT}")
    print(f"out {OUT}")
    print(f"steps {steps}")
    for name, subject, width, height, seed, _keyed in JOBS:
        print(f"{seed:5d}  {name}  {width}x{height}")
        print(f"       {prompt_for(backend, subject)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--backend", choices=("sdxl", "flux"), default="sdxl")
    parser.add_argument("--steps", type=int, default=0, help="0 picks 30 for SDXL and 28 for Flux.")
    parser.add_argument("--cpu-offload", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument(
        "--placeholders",
        action="store_true",
        help="Write the pixel-art stand-ins with Pillow. Does not call a GPU.",
    )
    parser.add_argument(
        "--cutouts",
        action="store_true",
        help="Punch transparent backgrounds on the town plates already in assets/textures/town. No GPU.",
    )
    args = parser.parse_args()
    steps = args.steps or (28 if args.backend == "flux" else 30)
    if args.dry_run:
        print_plan(args.backend, steps)
        return
    if args.placeholders:
        write_placeholders()
        return
    if args.cutouts:
        write_cutouts()
        return
    if args.backend == "flux" and not args.cpu_offload:
        print("Flux on 16 GB: pass --cpu-offload if the 5080 is the 16 GB card.")
    run_generation(args.backend, steps, args.cpu_offload)


if __name__ == "__main__":
    main()
