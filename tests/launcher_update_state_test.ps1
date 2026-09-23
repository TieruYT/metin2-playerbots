# The launcher's record of what is installed, around a build that failed.
#
# pattsito (23 September) clicked the update five times against a Docker disk
# gone read-only: each click downloaded and applied the same package again,
# because a failed build masks the installed server version as "unknown" -
# and Save-State, reading through that mask, wrote "unknown" over the client
# version a client update had just recorded. Pinned here: a pending build of
# the version on offer is finished without a download, the mask covers the
# server alone, and a state file that does not parse stops nothing.
#
# Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\launcher_update_state_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $root 'Metin2-Launcher.ps1'

$script:pass = 0
$script:fail = 0
function Check {
    param([string]$What, $Expected, $Actual)
    if ([string]$Expected -eq [string]$Actual) {
        Write-Host ("  OK   " + $What) -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host ("  FAIL " + $What + ": expected [" + $Expected + "], got [" + $Actual + "]") -ForegroundColor Red
        $script:fail++
    }
}

# The functions live in the launcher script, which cannot be dot-sourced (it
# would run a menu), so they are lifted out of its syntax tree by name.
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($launcher, [ref]$null, [ref]$errors)
if ($errors -and @($errors).Count -gt 0) { throw "Metin2-Launcher.ps1 does not parse" }
foreach ($name in @('Test-RebuildPending', 'Read-RecordedState', 'Read-State', 'Save-State',
                    'Test-InstalledVersion', 'Get-ManifestComponent', 'Update-Server')) {
    $fn = $ast.Find({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name }, $true)
    if (-not $fn) { throw "no function $name in the launcher" }
    . ([scriptblock]::Create($fn.Extent.Text))
}

# What Update-Server calls out to, replaced by something the test can count.
$script:calls = [Collections.Generic.List[string]]::new()
function Write-Phase { param([string]$Name) }
function Confirm-Operation { param([string]$Question) return $true }
function Assert-DockerDiskWritable { param([switch]$KeepRebuildPending, [string]$Before) $script:calls.Add('disk') }
function Rebuild-Server { $script:calls.Add('build') }
function Invoke-M2PackageUpdate {
    param($Component, [string]$TargetRoot, [string]$BackupRoot)
    $script:calls.Add('download')
    Set-Content -LiteralPath (Join-Path $TargetRoot 'VERSION') -Value $Component.version -Encoding ASCII
    return [pscustomobject]@{ Files = 7042; Backup = (Join-Path $BackupRoot 'update-test'); Version = $Component.version }
}

$serverRoot = Join-Path ([IO.Path]::GetTempPath()) ('m2state-' + [Guid]::NewGuid().ToString('N'))
$statePath = Join-Path $serverRoot '.m2launcher-state.json'
$rebuildMarkerPath = Join-Path $serverRoot '.m2launcher-rebuild-pending'
function Set-Fixture {
    param([string]$Version, [string]$ShippedClient, $State, [switch]$Pending)
    Get-ChildItem -LiteralPath $serverRoot -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
    Set-Content -LiteralPath (Join-Path $serverRoot 'VERSION') -Value $Version -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $serverRoot 'CLIENT_VERSION') -Value $ShippedClient -Encoding ASCII
    if ($State -is [string]) { [IO.File]::WriteAllText($statePath, $State) }
    elseif ($State) { $State | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8 }
    if ($Pending) { Set-Content -LiteralPath $rebuildMarkerPath -Value 'test' -Encoding ASCII }
    $script:calls.Clear()
}
$manifest = [pscustomobject]@{
    server = [pscustomobject]@{ version = '2.0.98'; url = 'https://example.invalid/s.zip'; sha256 = 'AA' }
    client = [pscustomobject]@{ version = '2.0.26'; url = 'https://example.invalid/c.zip'; sha256 = 'BB' }
}

New-Item -ItemType Directory -Path $serverRoot -Force | Out-Null
try {
    Write-Host '== a pending build of the version on offer is finished, not downloaded again =='
    Set-Fixture -Version '2.0.98' -ShippedClient '2.0.13' -State @{ schema = 1; server = '2.0.98'; client = 'unknown' } -Pending
    Update-Server -RemoteManifest $manifest
    Check 'nothing downloaded' 'build' ($script:calls -join ',')

    Write-Host '== a pending build of an older version still takes the new package =='
    Set-Fixture -Version '2.0.97' -ShippedClient '2.0.13' -State @{ schema = 1; server = '2.0.97'; client = '2.0.26' } -Pending
    Update-Server -RemoteManifest $manifest
    Check 'disk checked, downloaded, built' 'disk,download,build' ($script:calls -join ',')
    Check 'the new server version is recorded' '2.0.98' (Read-RecordedState).server
    Check 'and the client version survives it' '2.0.26' (Read-RecordedState).client

    Write-Host '== the mask covers the server alone =='
    Set-Fixture -Version '2.0.98' -ShippedClient '2.0.13' -State @{ schema = 1; server = '2.0.98'; client = '2.0.26' } -Pending
    Check 'server reads unknown while the build is pending' 'unknown' (Read-State).server
    Check 'client reads what was recorded' '2.0.26' (Read-State).client
    Save-State -ServerVersion '2.0.98' -ClientVersion ''
    Check 'saving the server keeps the client' '2.0.26' (Read-RecordedState).client
    Save-State -ServerVersion '' -ClientVersion '2.0.27'
    Check 'saving the client keeps the server' '2.0.98' (Read-RecordedState).server

    Write-Host '== an up-to-date server is left alone =='
    Set-Fixture -Version '2.0.98' -ShippedClient '2.0.13' -State @{ schema = 1; server = '2.0.98'; client = '2.0.26' }
    Update-Server -RemoteManifest $manifest
    Check 'nothing called' '' ($script:calls -join ',')

    Write-Host '== a recorded "unknown" is no record =='
    Set-Fixture -Version '2.0.98' -ShippedClient '2.0.13' -State @{ schema = 1; server = 'unknown'; client = '2.0.26' }
    Check 'server falls back to VERSION' '2.0.98' (Read-State).server

    Write-Host '== a state file a crash left behind stops nothing =='
    Set-Fixture -Version '2.0.98' -ShippedClient '2.0.13' -State ([string]([char]0) * 64)
    Check 'server from VERSION' '2.0.98' (Read-State).server
    Check 'client from CLIENT_VERSION' '2.0.13' (Read-State).client
}
finally {
    Remove-Item -LiteralPath $serverRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ''
Write-Host ("{0} passed, {1} failed" -f $script:pass, $script:fail)
if ($script:fail -gt 0) { exit 1 }
