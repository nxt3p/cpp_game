#!/usr/bin/env bash
# disable-model-invocation: true
# Cross-compile 32-bit x86 GameEngine.exe for Windows 11 (WOW64).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

ENGINE_WIN_BUILD_DIR="${ENGINE_ROOT}/build-win-i686"
ENGINE_BUILD_TYPE="${ENGINE_BUILD_TYPE:-Release}"

echo "==> Windows i686 (32-bit x86) cross-build"
echo "Root: ${ENGINE_ROOT}"
echo "Output: ${ENGINE_WIN_BUILD_DIR}"

if ! command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then
    cat >&2 <<'EOF'
Install 32-bit MinGW toolchain:
  sudo apt install -y gcc-mingw-w64-i686 g++-mingw-w64-i686 mingw-w64-i686-dev
EOF
    exit 1
fi

CORES="$(nproc 2>/dev/null || echo 4)"

cmake -S "${ENGINE_ROOT}" -B "${ENGINE_WIN_BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${ENGINE_ROOT}/cmake/toolchains/mingw-i686-w64.cmake" \
    -DCMAKE_BUILD_TYPE="${ENGINE_BUILD_TYPE}"

cmake --build "${ENGINE_WIN_BUILD_DIR}" --parallel "${CORES}" --target GameEngine

EXE_PATH="${ENGINE_WIN_BUILD_DIR}/GameEngine.exe"
file "${EXE_PATH}"
echo "Built: ${EXE_PATH}"
