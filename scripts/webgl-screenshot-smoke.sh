#!/usr/bin/env bash
# disable-model-invocation: true
# Capture a headless browser screenshot and verify the main menu rendered.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

BUILD_DIR="${ENGINE_ROOT}/build-webgl"
PORT="${WEBGL_SCREENSHOT_PORT:-8788}"
SHOT="/mnt/c/Users/Public/cppgame-webgl-smoke.png"
WIN_SHOT="C:/Users/Public/cppgame-webgl-smoke.png"
CHROME=""
for candidate in \
    "/mnt/c/Program Files/Google/Chrome/Application/chrome.exe" \
    "/mnt/c/Program Files (x86)/Google/Chrome/Application/chrome.exe" \
    "/usr/bin/google-chrome" \
    "/usr/bin/chromium-browser" \
    "/usr/bin/chromium"; do
    if [[ -x "${candidate}" || -f "${candidate}" ]]; then
        CHROME="${candidate}"
        break
    fi
done

if [[ -z "${CHROME}" ]]; then
    echo "Screenshot smoke test skipped (no Chrome/Chromium found)."
    exit 0
fi

server_pid=""
cleanup() {
    if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
        kill "${server_pid}" 2>/dev/null || true
        wait "${server_pid}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

(
    cd "${BUILD_DIR}"
    python3 -m http.server "${PORT}" --bind 127.0.0.1 >/dev/null 2>&1
) &
server_pid=$!

for _ in $(seq 1 30); do
    if curl -sf "http://127.0.0.1:${PORT}/index.html" >/dev/null; then
        break
    fi
    sleep 0.2
done

rm -f "${SHOT}" 2>/dev/null || true
"${CHROME}" \
    --headless=new \
    --disable-gpu \
    --no-sandbox \
    --window-size=1280,720 \
    --virtual-time-budget=20000 \
    --screenshot="${WIN_SHOT}" \
    "http://127.0.0.1:${PORT}/index.html" >/dev/null 2>&1

python3 - <<'PY'
from pathlib import Path
from PIL import Image

shot = Path("/mnt/c/Users/Public/cppgame-webgl-smoke.png")
if not shot.exists():
    raise SystemExit("Screenshot smoke test failed: screenshot file was not created.")

image = Image.open(shot)
bright = 0
gold_menu = 0
for x in range(0, image.width, 8):
    for y in range(0, image.height, 8):
        pixel = image.getpixel((x, y))
        if isinstance(pixel, int):
            value = pixel
            r = g = b = value
        else:
            r, g, b = pixel[:3]
        if max(r, g, b) > 30:
            bright += 1
        # Gothic menu trim is gold, not the old green Start button.
        if r > 140 and g > 90 and r > b + 30 and g > b:
            gold_menu += 1

if bright < 200:
    raise SystemExit(f"Screenshot smoke test failed: scene too dark (bright={bright}).")
if gold_menu < 30:
    raise SystemExit(f"Screenshot smoke test failed: main menu chrome not detected (gold={gold_menu}).")

print(f"Screenshot smoke test passed (bright={bright}, gold={gold_menu}).")
PY
