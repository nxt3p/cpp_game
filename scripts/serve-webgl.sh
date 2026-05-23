#!/usr/bin/env bash
# disable-model-invocation: true
# Serve the WebGL build for interactive browser testing (Cursor embedded browser).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

ENGINE_WEBGL_BUILD_DIR="${ENGINE_ROOT}/build-webgl"
WEBGL_SERVE_PORT="${WEBGL_SERVE_PORT:-8081}"
WEBGL_SERVE_BIND="${WEBGL_SERVE_BIND:-127.0.0.1}"

if [[ ! -f "${ENGINE_WEBGL_BUILD_DIR}/index.html" ]]; then
    echo "Missing ${ENGINE_WEBGL_BUILD_DIR}/index.html" >&2
    echo "Build first: ${ENGINE_ROOT}/scripts/build-webgl.sh" >&2
    exit 1
fi

echo "Serving WebGL from ${ENGINE_WEBGL_BUILD_DIR}"
echo "Open: http://${WEBGL_SERVE_BIND}:${WEBGL_SERVE_PORT}/index.html"
echo "Press Ctrl+C to stop."

cd "${ENGINE_WEBGL_BUILD_DIR}"
exec python3 -m http.server "${WEBGL_SERVE_PORT}" --bind "${WEBGL_SERVE_BIND}"
