#!/usr/bin/env bash
# disable-model-invocation: true
# Build the cppGame engine for WebGL (Emscripten) and smoke-test the artifacts.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

ENGINE_WEBGL_BUILD_DIR="${ENGINE_ROOT}/build-webgl"
ENGINE_BUILD_TYPE="${ENGINE_BUILD_TYPE:-Release}"
EMSDK_ROOT="${EMSDK_ROOT:-${ENGINE_ROOT}/.emsdk}"
CORES="$(nproc 2>/dev/null || echo 4)"

echo "==> WebGL build (Emscripten)"
echo "Root: ${ENGINE_ROOT}"
echo "Build type: ${ENGINE_BUILD_TYPE}"
echo "Output: ${ENGINE_WEBGL_BUILD_DIR}"

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1" >&2
        exit 1
    fi
}

bootstrap_emsdk() {
    if [[ -x "${EMSDK_ROOT}/upstream/emscripten/emcc" ]]; then
        return 0
    fi

    require_command git
    echo "==> Bootstrapping Emscripten SDK at ${EMSDK_ROOT}"
    if [[ ! -d "${EMSDK_ROOT}" ]]; then
        git clone https://github.com/emscripten-core/emsdk.git "${EMSDK_ROOT}"
    fi

    (
        cd "${EMSDK_ROOT}"
        ./emsdk install latest
        ./emsdk activate latest
    )
}

activate_emsdk() {
    bootstrap_emsdk
    # shellcheck disable=SC1091
    source "${EMSDK_ROOT}/emsdk_env.sh"
    require_command emcc
    require_command emcmake
    require_command emmake
    echo "Emscripten: $(emcc --version | head -n 1)"
}

configure_build() {
    emcmake cmake -S "${ENGINE_ROOT}" -B "${ENGINE_WEBGL_BUILD_DIR}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="${ENGINE_BUILD_TYPE}"
}

build_game() {
    emmake cmake --build "${ENGINE_WEBGL_BUILD_DIR}" --parallel "${CORES}" --target GameEngine
}

verify_artifacts() {
    local wasm_file="${ENGINE_WEBGL_BUILD_DIR}/GameEngine.wasm"
    local js_file="${ENGINE_WEBGL_BUILD_DIR}/GameEngine.js"
    local data_file="${ENGINE_WEBGL_BUILD_DIR}/GameEngine.data"
    local shell_file="${ENGINE_WEBGL_BUILD_DIR}/index.html"

    for artifact in "${wasm_file}" "${js_file}" "${data_file}" "${shell_file}"; do
        if [[ ! -f "${artifact}" ]]; then
            echo "Missing WebGL artifact: ${artifact}" >&2
            exit 1
        fi
    done

    local magic
    magic="$(head -c 4 "${wasm_file}" | od -An -tx1 | tr -d ' \n')"
    if [[ "${magic}" != "0061736d" ]]; then
        echo "Invalid wasm magic in ${wasm_file}: ${magic}" >&2
        exit 1
    fi

    echo "Artifacts verified:"
    echo "  ${wasm_file}"
    echo "  ${js_file}"
    echo "  ${data_file}"
    echo "  ${shell_file}"
}

smoke_test_http() {
    require_command python3

    local port=8765
    local server_log
    server_log="$(mktemp)"
    local server_pid=""

    echo "==> HTTP smoke test on port ${port}"
    (
        cd "${ENGINE_WEBGL_BUILD_DIR}"
        python3 -m http.server "${port}" --bind 127.0.0.1 >"${server_log}" 2>&1
    ) &
    server_pid=$!

    for _ in $(seq 1 30); do
        if curl -sf "http://127.0.0.1:${port}/index.html" >/dev/null; then
            break
        fi
        sleep 0.2
    done

    local index_html js_bundle
    index_html="$(curl -sf "http://127.0.0.1:${port}/index.html")"
    js_bundle="$(curl -sf "http://127.0.0.1:${port}/GameEngine.js")"
    grep -qi "WebGL runtime ready" <<< "${index_html}"
    grep -qi "WebAssembly" <<< "${js_bundle}"

    local wasm_bytes
    wasm_bytes="$(curl -sf "http://127.0.0.1:${port}/GameEngine.wasm" | wc -c | tr -d ' ')"
    if [[ "${wasm_bytes}" -lt 100000 ]]; then
        kill "${server_pid}" 2>/dev/null || true
        rm -f "${server_log}"
        echo "WebGL wasm bundle looks too small (${wasm_bytes} bytes)" >&2
        exit 1
    fi

    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
    rm -f "${server_log}"

    echo "HTTP smoke test passed (${wasm_bytes} wasm bytes served)"
}

smoke_test_headless_browser() {
    echo "==> Browser screenshot smoke test"
    "${SCRIPT_DIR}/webgl-screenshot-smoke.sh"
}

activate_emsdk
configure_build
build_game
verify_artifacts
smoke_test_http
smoke_test_headless_browser

echo ""
echo "WebGL build complete."
echo "  Serve:  ${ENGINE_ROOT}/scripts/serve-webgl.sh"
echo "  Open:   http://127.0.0.1:8081/index.html"
