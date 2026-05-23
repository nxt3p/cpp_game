#!/usr/bin/env bash
# disable-model-invocation: true
# Remove all local CMake/Ninja build trees and stale compile_commands.json copies.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

BUILD_DIRS=(
    "${ENGINE_ROOT}/build"
    "${ENGINE_ROOT}/build-webgl"
    "${ENGINE_ROOT}/build-win-x86_64"
    "${ENGINE_ROOT}/build-native-check"
)

removed_any=false
for dir in "${BUILD_DIRS[@]}"; do
    if [[ -d "${dir}" ]]; then
        rm -rf "${dir}"
        echo "Removed ${dir}"
        removed_any=true
    fi
done

if [[ -f "${ENGINE_ROOT}/compile_commands.json" ]]; then
    rm -f "${ENGINE_ROOT}/compile_commands.json"
    echo "Removed ${ENGINE_ROOT}/compile_commands.json"
    removed_any=true
fi

if [[ "${removed_any}" == false ]]; then
    echo "No build directories found; nothing to clean."
else
    echo "Build cleanup complete."
fi
