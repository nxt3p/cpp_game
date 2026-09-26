#!/usr/bin/env python3
"""Generate RPG atlas art with SDXL or FLUX.1-dev plus public Hugging Face LoRAs.

The procedural sheets from ``generate_assets.py`` stay the fallback. This script
overwrites pixels inside the existing atlas PNGs and leaves the JSON clip and
frame layout alone, so the C++ loaders do not change.

Style cue: dark rounded inventory plates and one distinct silhouette per slot,
in the spirit of ``items_atlas.png``. Prompts are original. No third-party game
art is copied into the sheets.

Models (exact repo ids)
-----------------------
SDXL stack (default). Fits a 16 GB RTX 5080 without offload.

  Base   stabilityai/stable-diffusion-xl-base-1.0
         public diffusers SDXL 1.0 checkpoint (not gated).
  LoRA   nerijs/pixel-art-xl
         weight file pixel-art-xl.safetensors
         trigger token: pixel
         adapter weight: 0.90
         Game-icon / sprite LoRA. The model card uses the token ``pixel`` and
         also says a keyword is optional. This script always sends ``pixel``.
  LoRA   ntc-ai/SDXL-LoRA-slider.fantasy
         weight file fantasy.safetensors
         trigger token: fantasy
         adapter weight: 1.50
         RPG / fantasy style slider (MIT), trained on SDXL base.

Flux stack (optional, license-gated base).

  Base   black-forest-labs/FLUX.1-dev
         gated: accept the FluxDev non-commercial license on the model page,
         then ``huggingface-cli login``.
  LoRA   AIGCDuckBoss/fluxLora_pixelRPG
         weight file fluxLora_pixelrpg.safetensors
         trigger: The overall style of the illustration is colorful pixel style
         adapter weight: 0.85
         Public pixel-RPG LoRA trained on FLUX.1-dev (Apache-2.0).

One command on the WSL RTX 5080 (repo at /home/dev/projects/cppGame)::

  cd /home/dev/projects/cppGame
  python3 -m venv .venv-diffusion
  source .venv-diffusion/bin/activate
  python -m pip install -U pip
  python -m pip install -r scripts/requirements-diffusion.txt
  python scripts/generate_rpg_atlases_sd.py --backend sdxl

Flux (after the license click and login)::

  python scripts/generate_rpg_atlases_sd.py --backend flux --cpu-offload

``--dry-run`` and ``--check`` need only Pillow. They do not download weights.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFilter
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Pillow is required: pip install pillow") from exc


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "generated"
CACHE = OUT / "rpg_src"

# SDXL
SDXL_MODEL = "stabilityai/stable-diffusion-xl-base-1.0"
SDXL_PIXEL_LORA = "nerijs/pixel-art-xl"
SDXL_PIXEL_FILE = "pixel-art-xl.safetensors"
SDXL_PIXEL_TRIGGER = "pixel"
SDXL_PIXEL_WEIGHT = 0.90
SDXL_FANTASY_LORA = "ntc-ai/SDXL-LoRA-slider.fantasy"
SDXL_FANTASY_FILE = "fantasy.safetensors"
SDXL_FANTASY_TRIGGER = "fantasy"
SDXL_FANTASY_WEIGHT = 1.50

# FLUX.1-dev
FLUX_MODEL = "black-forest-labs/FLUX.1-dev"
FLUX_LORA = "AIGCDuckBoss/fluxLora_pixelRPG"
FLUX_LORA_FILE = "fluxLora_pixelrpg.safetensors"
FLUX_TRIGGER = "The overall style of the illustration is colorful pixel style"
FLUX_LORA_WEIGHT = 0.85

NEGATIVE = (
    "text, letters, numbers, watermark, logo, signature, caption, ui mockup, "
    "inventory page, sprite sheet, tileset, collage, many objects, repeated objects, "
    "person, face, hands holding, scenery, landscape, room, photo, blurry, "
    "3d scene, frame, border, label"
)

ICON_TAIL = (
    "dark fantasy action RPG inventory icon, one object only, centered, large, "
    "plain light gray background, no text"
)
SPRITE_TAIL = (
    "one full-body dark fantasy action RPG character sprite, isolated, "
    "plain light gray background, no scenery, no text, feet visible"
)

# Subjects follow the 16-slot items_atlas grid (weapon through gem).
ITEM_SUBJECTS = {
    "weapon": "one steel longsword standing upright, crossguard and leather grip",
    "offhand": "one round steel shield with a gold boss and a small red emblem",
    "head": "one closed steel greathelm, empty visor, no face inside",
    "shoulders": "a pair of curved steel pauldrons only, no body",
    "chest": "one empty steel breastplate, front view, no person",
    "hands": "exactly one pair of brown leather gauntlets",
    "waist": "one brown leather belt with a single gold buckle, laid flat",
    "legs": "exactly one pair of steel leg greaves standing side by side",
    "feet": "exactly one pair of brown leather boots",
    "amulet": "one gold necklace with a single blue gem pendant",
    "ring": "one gold ring with a single red gem",
    "cloak": "one purple hooded cloak, front view, empty hood, no person",
    "charm": "one round ornate gold medallion with a red center gem",
    "relic": "one tall faceted green crystal relic",
    "potion": "exactly one corked glass flask of red potion",
    "gem": "exactly one large faceted emerald",
}

UI_SUBJECTS = {
    "skill_power_strike": "a single sword slash of orange light, skill icon",
    "skill_whirlwind": "a spinning cyan blade whirlwind, skill icon",
    "skill_heal": "a glowing green cross of light, healing skill icon",
    "skill_dash": "a blue boot with motion streaks, dash skill icon",
    "skill_cleave": "a wide orange axe arc, skill icon",
    "skill_firebolt": "one compact fireball, skill icon",
    "skill_shout": "a golden war horn with sound rings, skill icon",
    "skill_slam": "a hammer hitting the ground with a shock ring, skill icon",
    "menu_abilities": "a small open spellbook with a blue gem, round game button icon",
    "menu_inventory": "a leather satchel, round game button icon",
    "menu_map": "a folded parchment map with a red pin, round game button icon",
    "menu_settings": "a brass gear, round game button icon",
    "menu_pause": "two vertical gold pause bars, round game button icon",
    "potion_vial": "one corked glass flask of red potion, game UI icon",
    "globe_ring": (
        "an ornate circular dark-metal game UI bezel with gold rivets, "
        "empty black center, ring only"
    ),
}

SPRITE_SUBJECTS = {
    "warrior": "a knight in red plate armor, steel helm with a red plume, cape, longsword",
    "ranger": "a ranger in a green hood and leather armor, shortbow, quiver",
    "mage": "a mage in a blue robe and tall pointed hat, wooden staff with a blue orb",
    "monster": "a horned green wolf-beast, quadruped, yellow eyes, no rider",
}

SPRITE_POSES = {
    "front": "facing the camera, standing idle",
    "profile": "side view facing right, standing idle",
    "back": "seen from behind, standing idle",
    "attack": "side view facing right, weapon swung forward",
    "death": "fallen on the ground, side view facing right",
}

ITEM_SEEDS = {name: 4100 + index for index, name in enumerate(ITEM_SUBJECTS)}
UI_SEEDS = {name: 5200 + index for index, name in enumerate(UI_SUBJECTS)}
SPRITE_SEEDS = {
    f"{who}_{pose}": 6300 + who_index * 10 + pose_index
    for who_index, who in enumerate(SPRITE_SUBJECTS)
    for pose_index, pose in enumerate(SPRITE_POSES)
}


def direction_view(index: int) -> tuple[int, str]:
    """Match presentation_paint.direction_pose_local / the 8-direction sheets."""
    if index in (2, 3):
        return (-1, "profile")
    if index in (5, 6):
        return (1, "profile")
    if index == 4:
        return (1, "back")
    return (1, "front")


def sdxl_prompt(subject: str) -> str:
    return f"{SDXL_PIXEL_TRIGGER}, {SDXL_FANTASY_TRIGGER}, {subject}"


def flux_prompt(subject: str) -> str:
    return f"{subject}. {FLUX_TRIGGER}"


def prompt_for(backend: str, subject: str) -> str:
    return flux_prompt(subject) if backend == "flux" else sdxl_prompt(subject)


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def coverage_errors() -> list[str]:
    errors: list[str] = []
    items = load_json(OUT / "items_atlas.json")
    for frame in items["frames"]:
        if frame["name"] not in ITEM_SUBJECTS:
            errors.append(f"items_atlas frame {frame['name']} has no prompt")
    ui = load_json(OUT / "ui_atlas.json")
    ui_names = {frame["name"] for frame in ui["frames"]}
    for name in UI_SUBJECTS:
        if name not in ui_names:
            errors.append(f"ui prompt {name} is not a frame in ui_atlas.json")
    for sheet in ("warrior_atlas.json", "ranger_atlas.json", "mage_atlas.json", "monster_atlas.json"):
        meta = load_json(OUT / sheet)
        if meta.get("frameWidth") != 64 or meta.get("columns") != 6:
            errors.append(f"{sheet} is not the 64px 6-column layout this packer expects")
        if "idle" not in {clip["name"] for clip in meta["clips"]}:
            errors.append(f"{sheet} has no idle clip")
    return errors


def _border_color(image: Image.Image) -> tuple[int, int, int]:
    rgb = image.convert("RGB")
    width, height = rgb.size
    samples = []
    pixels = rgb.load()
    for x in range(width):
        samples.append(pixels[x, 0])
        samples.append(pixels[x, height - 1])
    for y in range(height):
        samples.append(pixels[0, y])
        samples.append(pixels[width - 1, y])
    count = max(1, len(samples))
    red = sum(pixel[0] for pixel in samples) // count
    green = sum(pixel[1] for pixel in samples) // count
    blue = sum(pixel[2] for pixel in samples) // count
    return red, green, blue


def cut_backdrop(image: Image.Image, tolerance: int = 36) -> Image.Image:
    """Knock out a flat backdrop sampled from the border. Keep the centered object."""
    rgba = image.convert("RGBA")
    red, green, blue = _border_color(rgba)
    pixels = rgba.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            pr, pg, pb, pa = pixels[x, y]
            if abs(pr - red) + abs(pg - green) + abs(pb - blue) <= tolerance:
                pixels[x, y] = (pr, pg, pb, 0)
    return rgba


def _fit(icon: Image.Image, size: int, margin: int) -> Image.Image:
    icon = icon.convert("RGBA")
    bbox = icon.getbbox()
    if bbox is None:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))
    cropped = icon.crop(bbox)
    avail = max(1, size - margin * 2)
    scale = min(avail / cropped.width, avail / cropped.height)
    resized = cropped.resize(
        (max(1, int(cropped.width * scale)), max(1, int(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(resized, ((size - resized.width) // 2, (size - resized.height) // 2), resized)
    return canvas


def on_dark_plate(icon: Image.Image, size: int) -> Image.Image:
    """Dark rounded plate, matching the inventory-icon layout cue."""
    fitted = _fit(icon, size, margin=max(4, size // 10))
    plate = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(plate)
    radius = max(4, size // 8)
    draw.rounded_rectangle((1, 1, size - 2, size - 2), radius=radius, fill=(22, 16, 18, 235))
    draw.rounded_rectangle((1, 1, size - 2, size - 2), radius=radius, outline=(70, 54, 32, 255))
    plate.alpha_composite(fitted)
    return plate


def fit_sprite(icon: Image.Image, size: int = 64) -> Image.Image:
    """Feet sit near the bottom of the cell so every clip shares a ground line."""
    icon = icon.convert("RGBA")
    bbox = icon.getbbox()
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    if bbox is None:
        return canvas
    cropped = icon.crop(bbox)
    avail_h = size - 8
    avail_w = size - 6
    scale = min(avail_w / cropped.width, avail_h / cropped.height)
    resized = cropped.resize(
        (max(1, int(cropped.width * scale)), max(1, int(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    x = (size - resized.width) // 2
    y = size - 4 - resized.height
    canvas.paste(resized, (x, y), resized)
    return canvas


def punch_globe_ring(icon: Image.Image, size: int = 192) -> Image.Image:
    """Keep a metal ring. The liquid globe is drawn by the game through the hole."""
    fitted = icon.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    center = size * 0.5
    outer = size * 0.48
    inner = size * 0.32  # diameter fraction 0.64, same hole the HUD liquid uses
    pixels = fitted.load()
    for y in range(size):
        for x in range(size):
            dist = math.hypot(x - center, y - center)
            red, green, blue, alpha = pixels[x, y]
            if dist > outer or dist < inner:
                pixels[x, y] = (red, green, blue, 0)
    return fitted.filter(ImageFilter.SMOOTH)


def pose_for_clip(clip: str) -> str:
    if clip in ("attack", "attack2", "cast"):
        return "attack"
    if clip == "death":
        return "death"
    return "idle"


def sprite_for_cell(keys: dict[str, Image.Image], clip: str, direction: int, frame: int) -> Image.Image:
    sign, view = direction_view(direction)
    pose = pose_for_clip(clip)
    if pose == "attack":
        sprite = keys.get("attack") or keys.get("profile") or keys["front"]
    elif pose == "death":
        sprite = keys.get("death") or keys.get("profile") or keys["front"]
    elif view == "back":
        sprite = keys.get("back") or keys["front"]
    elif view == "profile":
        sprite = keys.get("profile") or keys["front"]
    else:
        sprite = keys["front"]
    if sign < 0 and view == "profile":
        sprite = sprite.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    cell = sprite.copy()
    shift_y = 0
    shift_x = 0
    if clip == "idle":
        shift_y = -1 if frame % 2 == 0 else 0
    elif clip == "walk":
        shift_y = (1, 0, -1, 0, 1, -1)[frame % 6]
        shift_x = sign * ((frame % 3) - 1)
    elif clip == "hit":
        shift_x = -sign * 3
    elif clip == "death":
        shift_y = min(10, frame * 2)
    if shift_x or shift_y:
        moved = Image.new("RGBA", cell.size, (0, 0, 0, 0))
        moved.paste(cell, (shift_x, shift_y), cell)
        cell = moved
    return cell


def paste_cell(sheet: Image.Image, cell: Image.Image, x: int, y: int) -> None:
    blank = Image.new("RGBA", cell.size, (0, 0, 0, 0))
    sheet.paste(blank, (x, y))
    sheet.paste(cell, (x, y), cell)


def pack_named_atlas(png_name: str, cells: dict[str, Image.Image]) -> int:
    meta = load_json(OUT / png_name.replace(".png", ".json"))
    sheet = Image.open(OUT / png_name).convert("RGBA")
    written = 0
    for frame in meta["frames"]:
        cell = cells.get(frame["name"])
        if cell is None:
            continue
        fitted = cell.resize((int(frame["w"]), int(frame["h"])), Image.Resampling.LANCZOS)
        paste_cell(sheet, fitted, int(frame["x"]), int(frame["y"]))
        written += 1
    sheet.save(OUT / png_name)
    return written


def pack_directional(png_name: str, keys: dict[str, Image.Image]) -> int:
    meta = load_json(OUT / png_name.replace(".png", ".json"))
    sheet = Image.open(OUT / png_name).convert("RGBA")
    width = int(meta["frameWidth"])
    height = int(meta["frameHeight"])
    written = 0
    for clip in meta["clips"]:
        for direction in range(len(meta["directions"])):
            for frame in range(int(clip["frames"])):
                cell = sprite_for_cell(keys, clip["name"], direction, frame)
                if cell.size != (width, height):
                    cell = cell.resize((width, height), Image.Resampling.LANCZOS)
                x = frame * width
                y = (int(clip["baseRow"]) + direction) * height
                paste_cell(sheet, cell, x, y)
                written += 1
    sheet.save(OUT / png_name)
    return written


class Generator:
    def __init__(self, backend: str, size: int, steps: int, cpu_offload: bool) -> None:
        self.backend = backend
        self.size = size
        self.steps = steps
        self.cpu_offload = cpu_offload
        self.pipe = None
        self.torch = None

    def load(self) -> None:
        try:
            import torch
            from diffusers import FluxPipeline, StableDiffusionXLPipeline
        except ImportError as exc:
            raise SystemExit(
                "torch and diffusers are not installed. On the RTX 5080:\n"
                "  cd /home/dev/projects/cppGame\n"
                "  python3 -m venv .venv-diffusion && source .venv-diffusion/bin/activate\n"
                "  python -m pip install -r scripts/requirements-diffusion.txt\n"
                "  python scripts/generate_rpg_atlases_sd.py --backend "
                f"{self.backend}\n"
                f"Import error: {exc}"
            ) from exc

        self.torch = torch
        if not torch.cuda.is_available():
            raise SystemExit(
                "CUDA is not available. Run this on the RTX 5080:\n"
                "  cd /home/dev/projects/cppGame\n"
                "  source .venv-diffusion/bin/activate\n"
                "  python scripts/generate_rpg_atlases_sd.py --backend "
                f"{self.backend}"
            )
        dtype = torch.float16 if self.backend == "sdxl" else torch.bfloat16
        if self.backend == "flux":
            print(f"Loading {FLUX_MODEL} ({dtype})")
            pipe = FluxPipeline.from_pretrained(FLUX_MODEL, torch_dtype=dtype)
            pipe.load_lora_weights(FLUX_LORA, weight_name=FLUX_LORA_FILE)
            try:
                pipe.fuse_lora(lora_scale=FLUX_LORA_WEIGHT)
            except TypeError:
                pipe.fuse_lora()
            print(f"Flux LoRA {FLUX_LORA} weight {FLUX_LORA_WEIGHT}")
            print(f"Trigger: {FLUX_TRIGGER}")
        else:
            print(f"Loading {SDXL_MODEL} ({dtype})")
            pipe = StableDiffusionXLPipeline.from_pretrained(
                SDXL_MODEL,
                torch_dtype=dtype,
                variant="fp16",
                use_safetensors=True,
            )
            pipe.load_lora_weights(SDXL_PIXEL_LORA, weight_name=SDXL_PIXEL_FILE, adapter_name="pixel")
            pipe.load_lora_weights(
                SDXL_FANTASY_LORA, weight_name=SDXL_FANTASY_FILE, adapter_name="fantasy"
            )
            pipe.set_adapters(["pixel", "fantasy"], adapter_weights=[SDXL_PIXEL_WEIGHT, SDXL_FANTASY_WEIGHT])
            print(
                f"SDXL LoRAs: {SDXL_PIXEL_LORA} '{SDXL_PIXEL_TRIGGER}' {SDXL_PIXEL_WEIGHT}; "
                f"{SDXL_FANTASY_LORA} '{SDXL_FANTASY_TRIGGER}' {SDXL_FANTASY_WEIGHT}"
            )
        if self.cpu_offload:
            pipe.enable_model_cpu_offload()
        else:
            pipe.to("cuda")
        self.pipe = pipe

    def generate(self, name: str, subject: str, seed: int) -> Image.Image:
        cache_path = CACHE / f"{self.backend}_{name}.png"
        if cache_path.exists():
            print(f"cache {name}")
            return Image.open(cache_path).convert("RGBA")
        assert self.pipe is not None and self.torch is not None
        prompt = prompt_for(self.backend, subject)
        print(f"gen {name} seed {seed}")
        generator = self.torch.Generator(device="cuda").manual_seed(seed)
        kwargs = {
            "prompt": prompt,
            "num_inference_steps": self.steps,
            "generator": generator,
            "width": self.size,
            "height": self.size,
        }
        if self.backend == "sdxl":
            kwargs["guidance_scale"] = 6.0
            kwargs["negative_prompt"] = NEGATIVE
        else:
            kwargs["guidance_scale"] = 3.5
        image = self.pipe(**kwargs).images[0]
        cut = cut_backdrop(image)
        CACHE.mkdir(parents=True, exist_ok=True)
        cut.save(cache_path)
        return cut


def build_icon(raw: Image.Image, name: str, size: int) -> Image.Image:
    if name == "globe_ring":
        return punch_globe_ring(raw, 192)
    return on_dark_plate(raw, size)


def planned_jobs(groups: set[str]) -> list[tuple[str, str, int, str]]:
    """Return (cache name, subject, seed, kind). kind is icon or sprite."""
    jobs: list[tuple[str, str, int, str]] = []
    if "items" in groups:
        for name, subject in ITEM_SUBJECTS.items():
            jobs.append((f"item_{name}", f"{subject}, {ICON_TAIL}", ITEM_SEEDS[name], "icon"))
    if "ui" in groups:
        for name, subject in UI_SUBJECTS.items():
            tail = ICON_TAIL if name != "globe_ring" else "plain black background, no text"
            jobs.append((f"ui_{name}", f"{subject}, {tail}", UI_SEEDS[name], "icon"))
    if "heroes" in groups:
        for who, subject in SPRITE_SUBJECTS.items():
            for pose, pose_text in SPRITE_POSES.items():
                jobs.append(
                    (
                        f"sprite_{who}_{pose}",
                        f"{subject}, {pose_text}, {SPRITE_TAIL}",
                        SPRITE_SEEDS[f"{who}_{pose}"],
                        "sprite",
                    )
                )
    return jobs


def print_plan(backend: str, groups: set[str]) -> None:
    print(f"backend {backend}")
    if backend == "flux":
        print(f"base {FLUX_MODEL}")
        print(f"lora {FLUX_LORA} file {FLUX_LORA_FILE} weight {FLUX_LORA_WEIGHT}")
        print(f"trigger {FLUX_TRIGGER}")
    else:
        print(f"base {SDXL_MODEL}")
        print(f"lora {SDXL_PIXEL_LORA} file {SDXL_PIXEL_FILE} trigger {SDXL_PIXEL_TRIGGER} weight {SDXL_PIXEL_WEIGHT}")
        print(
            f"lora {SDXL_FANTASY_LORA} file {SDXL_FANTASY_FILE} "
            f"trigger {SDXL_FANTASY_TRIGGER} weight {SDXL_FANTASY_WEIGHT}"
        )
    for name, subject, seed, _kind in planned_jobs(groups):
        print(f"{seed:5d}  {name}")
        print(f"       {prompt_for(backend, subject)}")


def run_generation(backend: str, groups: set[str], size: int, steps: int, cpu_offload: bool) -> None:
    generator = Generator(backend, size, steps, cpu_offload)
    generator.load()
    icons: dict[str, Image.Image] = {}
    ui_icons: dict[str, Image.Image] = {}
    sprites: dict[str, dict[str, Image.Image]] = {who: {} for who in SPRITE_SUBJECTS}
    for name, subject, seed, kind in planned_jobs(groups):
        raw = generator.generate(name, subject, seed)
        if kind == "sprite":
            _prefix, who, pose = name.split("_", 2)
            sprites[who][pose] = fit_sprite(raw, 64)
        elif name.startswith("item_"):
            slot = name.removeprefix("item_")
            icons[slot] = build_icon(raw, slot, 64)
        else:
            slot = name.removeprefix("ui_")
            target = 192 if slot == "globe_ring" else 96 if slot == "potion_vial" else 64
            ui_icons[slot] = build_icon(raw, slot, target)

    if icons:
        count = pack_named_atlas("items_atlas.png", icons)
        print(f"packed {count} item cells into items_atlas.png")
    if ui_icons:
        count = pack_named_atlas("ui_atlas.png", ui_icons)
        print(f"packed {count} ui cells into ui_atlas.png")
    if "heroes" in groups:
        for who, keys in sprites.items():
            if "front" not in keys:
                continue
            sheet = "monster_atlas.png" if who == "monster" else f"{who}_atlas.png"
            count = pack_directional(sheet, keys)
            print(f"packed {count} frames into {sheet}")


def parse_groups(text: str) -> set[str]:
    if text.strip() in ("", "all"):
        return {"items", "ui", "heroes"}
    alias = {"hero": "heroes", "character": "heroes", "characters": "heroes", "skills": "ui", "icons": "items"}
    groups = set()
    for part in text.split(","):
        name = alias.get(part.strip(), part.strip())
        if name not in {"items", "ui", "heroes"}:
            raise SystemExit(f"Unknown group {part!r}. Use items, ui, heroes, or all.")
        groups.add(name)
    return groups


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--backend", choices=("sdxl", "flux"), default="sdxl")
    parser.add_argument("--groups", default="all", help="items, ui, heroes, or all")
    parser.add_argument("--size", type=int, default=1024, help="Square generation size. SDXL native is 1024.")
    parser.add_argument("--steps", type=int, default=0, help="0 picks 30 for SDXL and 28 for Flux.")
    parser.add_argument(
        "--cpu-offload",
        action="store_true",
        help="Required for FLUX.1-dev on a 16 GB card. SDXL does not need it.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print model ids, triggers, and prompts.")
    parser.add_argument("--check", action="store_true", help="Fail if prompts drift from the atlas JSON.")
    args = parser.parse_args()

    errors = coverage_errors()
    if errors:
        for error in errors:
            print(error)
        raise SystemExit(1)
    print("Atlas prompt coverage matches items, ui, and directional JSON.")
    if args.check and not args.dry_run:
        return

    groups = parse_groups(args.groups)
    steps = args.steps or (28 if args.backend == "flux" else 30)
    if args.dry_run:
        print_plan(args.backend, groups)
        print(f"jobs {len(planned_jobs(groups))} size {args.size} steps {steps}")
        return
    if args.backend == "flux" and not args.cpu_offload:
        print("Flux on 16 GB: pass --cpu-offload if the 5080 is the 16 GB card.")
    run_generation(args.backend, groups, args.size, steps, args.cpu_offload)


if __name__ == "__main__":
    main()
