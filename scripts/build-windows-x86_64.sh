#!/usr/bin/env bash
# disable-model-invocation: true
# Cross-compile GameEngine.exe (x86_64 / PE64) for Windows 11 using MinGW-w64.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

ENGINE_WIN_BUILD_DIR="${ENGINE_ROOT}/build-win-x86_64"
ENGINE_BUILD_TYPE="${ENGINE_BUILD_TYPE:-Release}"
MINGW_PACKAGES=(
    cmake
    ninja-build
    git
    pkg-config
    gcc-mingw-w64-x86-64
    g++-mingw-w64-x86-64
    mingw-w64-x86-64-dev
    osslsigncode
    openssl
)

echo "==> Windows x86_64 cross-build (MinGW-w64)"
echo "Root: ${ENGINE_ROOT}"
echo "Build type: ${ENGINE_BUILD_TYPE}"
echo "Output: ${ENGINE_WIN_BUILD_DIR}"

require_mingw_toolchain() {
    if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
        cat >&2 <<'EOF'
MinGW-w64 toolchain not found. Install on Ubuntu/WSL:
  sudo apt update
  sudo apt install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 mingw-w64-x86-64-dev
EOF
        exit 1
    fi
}

require_mingw_toolchain

CORES="$(nproc 2>/dev/null || echo 4)"

cmake -S "${ENGINE_ROOT}" -B "${ENGINE_WIN_BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${ENGINE_ROOT}/cmake/toolchains/mingw-x86_64-w64.cmake" \
    -DCMAKE_BUILD_TYPE="${ENGINE_BUILD_TYPE}"

cmake --build "${ENGINE_WIN_BUILD_DIR}" --parallel "${CORES}" --target GameEngine

EXE_PATH="${ENGINE_WIN_BUILD_DIR}/GameEngine.exe"
if [[ ! -f "${EXE_PATH}" ]]; then
    echo "Expected Windows binary missing: ${EXE_PATH}" >&2
    exit 1
fi

file "${EXE_PATH}"

CERT_DIR="${ENGINE_ROOT}/.codesign"
PFX_PATH="${ENGINE_SIGN_PFX:-${CERT_DIR}/cppgame-dev.pfx}"
if [[ ! -f "${PFX_PATH}" ]]; then
    echo ""
    echo "==> No code-signing certificate found; creating development cert"
    "${SCRIPT_DIR}/setup-windows-codesign-cert.sh"
fi

if command -v osslsigncode >/dev/null 2>&1; then
    echo ""
    echo "==> Authenticode signing GameEngine.exe"
    "${SCRIPT_DIR}/sign-windows-exe.sh"
else
    echo ""
    echo "Warning: osslsigncode not installed; GameEngine.exe is UNSIGNED."
    echo "  Install: sudo apt install osslsigncode"
    echo "  Then run: ./scripts/sign-windows-exe.sh"
fi

# Export CER beside the build for easy copy to Windows.
if [[ -f "${CERT_DIR}/cppgame-dev.cer" ]]; then
    cp -f "${CERT_DIR}/cppgame-dev.cer" "${ENGINE_WIN_BUILD_DIR}/cppgame-dev.cer"
fi
if [[ -f "${PFX_PATH}" ]]; then
    cp -f "${PFX_PATH}" "${ENGINE_WIN_BUILD_DIR}/cppgame-dev.pfx"
fi
cp -f "${SCRIPT_DIR}/windows/Install-DevCertificate.ps1" "${ENGINE_WIN_BUILD_DIR}/"
cp -f "${SCRIPT_DIR}/windows/Disable-SmartAppControl-Dev.ps1" "${ENGINE_WIN_BUILD_DIR}/"
cp -f "${SCRIPT_DIR}/windows/Unblock-GameEngine.ps1" "${ENGINE_WIN_BUILD_DIR}/"
cp -f "${SCRIPT_DIR}/windows/Sign-GameEngine.ps1" "${ENGINE_WIN_BUILD_DIR}/"

echo ""
echo "Windows build complete."
echo "  Binary: ${EXE_PATH}"
echo "  Assets: ${ENGINE_WIN_BUILD_DIR}/assets"
echo "  Dev cert: ${ENGINE_WIN_BUILD_DIR}/cppgame-dev.cer (if signing enabled)"
echo ""
echo "On Windows 11 (first time only, Administrator PowerShell):"
echo "  cd build-win-x86_64"
echo "  powershell -ExecutionPolicy Bypass -File ..\\scripts\\windows\\Install-DevCertificate.ps1"
echo ""
echo "If Smart App Control still blocks the game (dev machine):"
echo "  powershell -ExecutionPolicy Bypass -File ..\\scripts\\windows\\Disable-SmartAppControl-Dev.ps1"
echo "  (restart Windows, then run GameEngine.exe)"
