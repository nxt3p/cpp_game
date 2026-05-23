<#
.SYNOPSIS
  Sign GameEngine.exe on Windows using signtool (Visual Studio Build Tools) or a local PFX.

  disable-model-invocation: true
#>
$ErrorActionPreference = 'Stop'

$buildDir = if (Test-Path '.\GameEngine.exe') { Get-Location } else {
    Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'build-win-x86_64'
}
$exePath = Join-Path $buildDir 'GameEngine.exe'
$pfxPath = Join-Path $buildDir 'cppgame-dev.pfx'
if (-not (Test-Path $pfxPath)) {
    $pfxPath = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) '.codesign\cppgame-dev.pfx'
}

if (-not (Test-Path $exePath)) {
    Write-Error "GameEngine.exe not found in $buildDir"
}

$signtool = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe",
    "${env:ProgramFiles}\Windows Kits\10\bin\*\x64\signtool.exe"
) | Get-Item -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1

if (-not $signtool) {
    Write-Error "signtool.exe not found. Install 'Windows SDK' or Visual Studio Build Tools."
}

if (-not (Test-Path $pfxPath)) {
    Write-Error "PFX not found: $pfxPath. Run setup-windows-codesign-cert.sh on WSL first."
}

& $signtool.FullName sign /fd SHA256 /f $pfxPath /p "" /tr http://timestamp.digicert.com /td SHA256 $exePath
Write-Host "Signed: $exePath"
Write-Host "Run Install-DevCertificate.ps1 as Administrator if the publisher is still untrusted."
