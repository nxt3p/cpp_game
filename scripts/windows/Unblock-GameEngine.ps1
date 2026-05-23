<#
.SYNOPSIS
  Remove Mark-of-the-Web from GameEngine.exe (helps with basic SmartScreen downloads blocks).

  disable-model-invocation: true
#>
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$exePath = Join-Path $repoRoot 'build-win-x86_64\GameEngine.exe'

if (-not (Test-Path $exePath)) {
    Write-Error "Not found: $exePath"
}

Unblock-File -LiteralPath $exePath
Write-Host "Unblocked: $exePath"
Write-Host "Note: Smart App Control may still require a trusted signature. Run Install-DevCertificate.ps1 as Admin."
