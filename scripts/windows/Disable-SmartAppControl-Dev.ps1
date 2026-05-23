#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Disable Smart App Control for local development (reversible).

  disable-model-invocation: true
#>
$ErrorActionPreference = 'Stop'

$policyPath = 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy'
if (-not (Test-Path $policyPath)) {
    New-Item -Path $policyPath -Force | Out-Null
}

# 0 = off, 1 = evaluation, 2 = on (enforced)
Set-ItemProperty -Path $policyPath -Name 'VerifiedAndReputablePolicyState' -Type DWord -Value 0
Write-Host 'Smart App Control policy set to Off (VerifiedAndReputablePolicyState=0).'
Write-Host 'Restart Windows for the change to take full effect.'
