#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Trust the cppGame development code-signing certificate on this Windows machine.

  disable-model-invocation: true
#>
$ErrorActionPreference = 'Stop'

function Find-DevCertificate {
    $candidates = @(
        (Join-Path (Get-Location) 'cppgame-dev.cer'),
        (Join-Path (Split-Path $PSScriptRoot -Parent) '..\build-win-x86_64\cppgame-dev.cer'),
        (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) '.codesign\cppgame-dev.cer')
    )
    foreach ($path in $candidates) {
        $resolved = [System.IO.Path]::GetFullPath($path)
        if (Test-Path $resolved) {
            return $resolved
        }
    }
    return $null
}

$cerPath = Find-DevCertificate
if (-not $cerPath) {
    Write-Error @"
Certificate not found (cppgame-dev.cer).

On WSL/Linux:
  ./scripts/setup-windows-codesign-cert.sh
  ./scripts/build-windows-x86_64.sh

Copy build-win-x86_64\ (includes cppgame-dev.cer) to Windows, cd into that folder, then run this script again.
"@
}

foreach ($storePath in @('Cert:\LocalMachine\Root', 'Cert:\LocalMachine\TrustedPublisher')) {
    $existing = Get-ChildItem $storePath | Where-Object { $_.Subject -like '*cppGame Development*' }
    if ($existing) {
        Write-Host "Certificate already present in $storePath"
        continue
    }
    Import-Certificate -FilePath $cerPath -CertStoreLocation $storePath | Out-Null
    Write-Host "Installed certificate into $storePath"
}

Write-Host ""
Write-Host "Trusted certificate from: $cerPath"
Write-Host "Run GameEngine.exe from the folder that contains assets\"
Write-Host ""
Write-Host "If Smart App Control still blocks the app, run Disable-SmartAppControl-Dev.ps1 (dev only) and restart Windows."
