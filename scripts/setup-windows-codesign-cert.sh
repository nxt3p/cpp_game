#!/usr/bin/env bash
# disable-model-invocation: true
# Create a local development Authenticode certificate (PFX + CER) for signing GameEngine.exe.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/engine_env.sh
source "${SCRIPT_DIR}/lib/engine_env.sh"

CERT_DIR="${ENGINE_ROOT}/.codesign"
PFX_PATH="${CERT_DIR}/cppgame-dev.pfx"
CER_PATH="${CERT_DIR}/cppgame-dev.cer"
KEY_PATH="${CERT_DIR}/cppgame-dev.key"
PEM_PATH="${CERT_DIR}/cppgame-dev.pem"

mkdir -p "${CERT_DIR}"
chmod 700 "${CERT_DIR}"

if [[ -f "${PFX_PATH}" && -f "${CER_PATH}" ]]; then
    echo "Development certificate already exists:"
    echo "  ${PFX_PATH}"
    echo "  ${CER_PATH}"
    exit 0
fi

if ! command -v openssl >/dev/null 2>&1; then
    echo "openssl is required. Install: sudo apt install openssl" >&2
    exit 1
fi

echo "==> Creating cppGame development code-signing certificate"
openssl req -x509 -newkey rsa -4096 -sha256 -days 3650 -nodes \
    -keyout "${KEY_PATH}" \
    -out "${PEM_PATH}" \
    -subj "/CN=cppGame Development/O=cppGame/C=US" \
    -addext "extendedKeyUsage=codeSigning" \
    -addext "keyUsage=digitalSignature"

openssl x509 -in "${PEM_PATH}" -outform DER -out "${CER_PATH}"
openssl pkcs12 -export -out "${PFX_PATH}" -inkey "${KEY_PATH}" -in "${PEM_PATH}" -passout pass:

chmod 600 "${KEY_PATH}" "${PFX_PATH}"
chmod 644 "${CER_PATH}"

echo ""
echo "Certificate created."
echo "  PFX (signing): ${PFX_PATH}"
echo "  CER (trust):   ${CER_PATH}"
echo ""
echo "On Windows 11 (once per machine, as Administrator), run:"
echo "  powershell -ExecutionPolicy Bypass -File scripts/windows/Install-DevCertificate.ps1"
echo ""
echo "Then rebuild and sign:"
echo "  ./scripts/build-windows-x86_64.sh"
