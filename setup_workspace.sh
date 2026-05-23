#!/usr/bin/env bash
# Verifies WSL graphics dependencies and prepares workspace asset directories.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/engine_env.sh
source "${SCRIPT_DIR}/scripts/lib/engine_env.sh"

echo "==> cppGame workspace setup"
echo "Root: ${ENGINE_ROOT}"

if ! engine_require_commands; then
    cat >&2 <<'EOF'
Install build tooling:
  sudo apt update
  sudo apt install -y build-essential cmake ninja-build pkg-config \
    libglfw3-dev libglew-dev libglm-dev
EOF
    exit 1
fi

if ! engine_require_pkg_config_modules; then
    cat >&2 <<'EOF'
Install graphics dependencies:
  sudo apt install -y libglfw3-dev libglew-dev libglm-dev
EOF
    exit 1
fi

engine_ensure_asset_dirs
engine_set_workspace_permissions

if [[ -e /dev/dri/renderD128 ]]; then
    echo "GPU render node detected: /dev/dri/renderD128"
else
    echo "Warning: /dev/dri/renderD128 not found. OpenGL runtime may be unavailable in WSLg."
fi

if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "MinGW x86_64 Windows cross-compiler detected."
    echo "Build Windows exe: ./scripts/build-windows-x86_64.sh"
else
    echo "Optional Windows cross-compile:"
    echo "  sudo apt install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 mingw-w64-x86-64-dev"
    echo "  ./scripts/build-windows-x86_64.sh"
fi

echo "Workspace setup complete."
