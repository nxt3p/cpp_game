#!/usr/bin/env bash
# Shared environment helpers for cppGame automation scripts.
# user-invocable: false

set -euo pipefail

engine_root_dir() {
    local script_path="${BASH_SOURCE[1]:-${BASH_SOURCE[0]}}"
    while [[ -L "${script_path}" ]]; do
        script_path="$(readlink "${script_path}")"
    done
    local script_dir
    script_dir="$(cd "$(dirname "${script_path}")" && pwd)"

    if [[ "${script_dir}" == */scripts/lib ]]; then
        (cd "${script_dir}/../.." && pwd)
    else
        (cd "${script_dir}/.." && pwd)
    fi
}

ENGINE_ROOT="$(engine_root_dir)"
ENGINE_BUILD_DIR="${ENGINE_ROOT}/build"
ENGINE_BUILD_TYPE="${ENGINE_BUILD_TYPE:-Debug}"

REQUIRED_PKGS=(glfw3 glew glm)
REQUIRED_BINS=(cmake ninja pkg-config)

engine_require_commands() {
    local missing=0
    for cmd in "${REQUIRED_BINS[@]}"; do
        if ! command -v "${cmd}" >/dev/null 2>&1; then
            echo "[engine_env] Missing command: ${cmd}" >&2
            missing=1
        fi
    done
    return "${missing}"
}

engine_require_pkg_config_modules() {
    local missing=0
    for module in "${REQUIRED_PKGS[@]}"; do
        if ! pkg-config --exists "${module}"; then
            echo "[engine_env] Missing pkg-config module: ${module}" >&2
            missing=1
        fi
    done
    return "${missing}"
}

engine_ensure_asset_dirs() {
    mkdir -p \
        "${ENGINE_ROOT}/assets/shaders" \
        "${ENGINE_ROOT}/assets/textures" \
        "${ENGINE_ROOT}/assets/models"
}

engine_set_workspace_permissions() {
    chmod +x "${ENGINE_ROOT}/setup_workspace.sh" "${ENGINE_ROOT}/validate.sh" 2>/dev/null || true
    chmod +x "${ENGINE_ROOT}/scripts/build-windows-x86_64.sh" 2>/dev/null || true
    find "${ENGINE_ROOT}/scripts" -type f -name '*.sh' -exec chmod +x {} + 2>/dev/null || true
}
