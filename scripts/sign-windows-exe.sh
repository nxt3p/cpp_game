#!/usr/bin/env bash
# disable-model-invocation: true
# Authenticode-sign GameEngine.exe (requires osslsigncode + .codesign/cppgame-dev.pfx).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

ENGINE_WIN_BUILD_DIR="${ENGINE_ROOT}/build-win-x86_64"
EXE_PATH="${ENGINE_WIN_BUILD_DIR}/GameEngine.exe"
PFX_PATH="${ENGINE_SIGN_PFX:-${ENGINE_ROOT}/.codesign/cppgame-dev.pfx}"
PFX_PASS="${ENGINE_SIGN_PFX_PASS:-}"

if [[ ! -f "${EXE_PATH}" ]]; then
    echo "Missing ${EXE_PATH}. Run ./scripts/build-windows-x86_64.sh first." >&2
    exit 1
fi

if [[ ! -f "${PFX_PATH}" ]]; then
    echo "Missing signing certificate: ${PFX_PATH}" >&2
    echo "Run: ./scripts/setup-windows-codesign-cert.sh" >&2
    exit 1
fi

if ! command -v osslsigncode >/dev/null 2>&1; then
    echo "osslsigncode is required. Install: sudo apt install osslsigncode" >&2
    exit 1
fi

SIGNED_PATH="${EXE_PATH}.signed"
rm -f "${SIGNED_PATH}"

echo "==> Signing ${EXE_PATH}"
SIGN_ARGS=(
    sign
    -pkcs12 "${PFX_PATH}"
    -n "cppGame Engine"
    -i "http://localhost/cppgame"
    -t "http://timestamp.digicert.com"
    -in "${EXE_PATH}"
    -out "${SIGNED_PATH}"
)
if [[ -n "${PFX_PASS}" ]]; then
    SIGN_ARGS+=(-pass "${PFX_PASS}")
else
    SIGN_ARGS+=(-pass "")
fi

osslsigncode "${SIGN_ARGS[@]}"
mv -f "${SIGNED_PATH}" "${EXE_PATH}"

if command -v osslsigncode >/dev/null 2>&1; then
    if osslsigncode verify -in "${EXE_PATH}" >/dev/null 2>&1; then
        echo "Signature verified."
    fi
fi

echo "Signed: ${EXE_PATH}"
echo "Trust the dev certificate on Windows (one-time): scripts/windows/Install-DevCertificate.ps1"
