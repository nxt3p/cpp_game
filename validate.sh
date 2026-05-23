#!/usr/bin/env bash
# disable-model-invocation: true
# Deterministic build + test validation pipeline (clears build cache).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/engine_env.sh
source "${SCRIPT_DIR}/scripts/lib/engine_env.sh"

echo "==> cppGame validation pipeline"
echo "Root: ${ENGINE_ROOT}"
echo "Build type: ${ENGINE_BUILD_TYPE}"

"${ENGINE_ROOT}/setup_workspace.sh"

if ! engine_require_commands || ! engine_require_pkg_config_modules; then
    echo "Dependency check failed." >&2
    exit 1
fi

if [[ -d "${ENGINE_BUILD_DIR}" ]]; then
    echo "Clearing previous build cache: ${ENGINE_BUILD_DIR}"
    chmod -R u+w "${ENGINE_BUILD_DIR}" 2>/dev/null || true
    rm -rf "${ENGINE_BUILD_DIR}"
fi

mkdir -p "${ENGINE_BUILD_DIR}"

CORES="$(nproc 2>/dev/null || echo 4)"

cmake -S "${ENGINE_ROOT}" -B "${ENGINE_BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${ENGINE_BUILD_TYPE}"

cmake --build "${ENGINE_BUILD_DIR}" --parallel "${CORES}"

run_test_suite() {
    local label="$1"
  echo "Running test suite: ${label}"
    ctest --test-dir "${ENGINE_BUILD_DIR}" --output-on-failure -R "^${label}$"
}

run_test_suite "engine_math_suite"
run_test_suite "engine_camera_suite"
run_test_suite "engine_zone_suite"
run_test_suite "engine_loot_suite"
run_test_suite "engine_inventory_suite"
run_test_suite "engine_trade_suite"
run_test_suite "engine_minimap_suite"
run_test_suite "engine_combat_suite"
run_test_suite "engine_items_suite"
run_test_suite "engine_mobs_suite"
run_test_suite "engine_boot_suite"
run_test_suite "engine_playthrough_suite"

if [[ ! -x "${ENGINE_BUILD_DIR}/GameEngine" ]]; then
    echo "GameEngine binary missing after build." >&2
    exit 1
fi

if [[ ! -x "${ENGINE_BUILD_DIR}/EngineTests" ]]; then
    echo "EngineTests binary missing after build." >&2
    exit 1
fi

echo "Validation succeeded."
