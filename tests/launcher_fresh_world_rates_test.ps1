# Set-FreshWorldSettings: the rates a world about to be made starts on.
#
# The function writes .env and nothing else, so what is worth pinning is which
# keys it writes and when it refuses: a number out of range must stop the reset
# rather than reach the migrator, "leave it alone" must write nothing at all,
# and the hold switch must come out as 1 or 0 and never as PowerShell's "True".
#
# Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\launcher_fresh_world_rates_test.ps1
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
foreach ($name in @('Test-RatePercent', 'Set-FreshWorldSettings')) {
    $fn = $ast.Find({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name }, $true)
    if (-not $fn) { throw "no function $name in the launcher" }
    . ([scriptblock]::Create($fn.Extent.Text))
}

# What the function calls out to, replaced by something the test can read.
$script:written = @{}
function Set-DotEnvValue { param([string]$Key, [string]$Value) $script:written[$Key] = $Value }
function Confirm-Operation { param([string]$Question) return $script:confirmAnswer }

$script:confirmAnswer = $false
# The starter chest joined these in 2.0.94 and the test did not, so every run
# stopped at "$StarterChest cannot be retrieved" under StrictMode.
$RateExp = -1; $RateDrop = -1; $RateYang = -1; $HoldBots = -1; $StarterChest = -1; $Yes = $false

try {
    Write-Host '== the numbers a caller passes are written, and nothing else is =='
    $script:written = @{}
    $RateExp = 300; $RateDrop = 200; $RateYang = 150; $HoldBots = -1; $Yes = $true
    Set-FreshWorldSettings -Reason 'test'
    Check 'experience' '300' $script:written['M2_RATE_EXP']
    Check 'drops' '200' $script:written['M2_RATE_DROP']
    Check 'yang' '150' $script:written['M2_RATE_YANG']
    Check 'the hold switch is left alone' $false $script:written.ContainsKey('M2_PLAYERBOT_START_HELD')

    Write-Host '== the hold switch is 1 or 0, never True/False =='
    $script:written = @{}
    $RateExp = -1; $RateDrop = -1; $RateYang = -1; $HoldBots = 1; $Yes = $true
    Set-FreshWorldSettings -Reason 'test'
    Check 'held' '1' $script:written['M2_PLAYERBOT_START_HELD']
    Check 'and no rate was written' $false $script:written.ContainsKey('M2_RATE_EXP')
    $script:written = @{}
    $HoldBots = 0
    Set-FreshWorldSettings -Reason 'test'
    Check 'not held' '0' $script:written['M2_PLAYERBOT_START_HELD']

    Write-Host '== the starter chest is 1 or 0 and nothing else is written =='
    $script:written = @{}
    $RateExp = -1; $RateDrop = -1; $RateYang = -1; $HoldBots = -1; $StarterChest = 0; $Yes = $true
    Set-FreshWorldSettings -Reason 'test'
    Check 'no chest' '0' $script:written['M2_STARTER_CHEST']
    Check 'and one key' 1 $script:written.Keys.Count
    $script:written = @{}
    $StarterChest = 1
    Set-FreshWorldSettings -Reason 'test'
    Check 'the chest' '1' $script:written['M2_STARTER_CHEST']
    $StarterChest = -1

    Write-Host '== -Yes with nothing to say writes nothing =='
    $script:written = @{}
    $RateExp = -1; $RateDrop = -1; $RateYang = -1; $HoldBots = -1; $Yes = $true
    Set-FreshWorldSettings -Reason 'test'
    Check 'no keys touched' 0 $script:written.Keys.Count

    Write-Host '== a number out of range stops the reset =='
    foreach ($bad in @(0, 10001)) {
        $script:written = @{}
        $RateExp = $bad; $RateDrop = -1; $RateYang = -1; $HoldBots = -1; $Yes = $true
        $threw = $false
        try { Set-FreshWorldSettings -Reason 'test' } catch { $threw = $true }
        Check ("refused $bad") $true $threw
        Check ("and wrote nothing for $bad") 0 $script:written.Keys.Count
    }

    Write-Host '== the bounds themselves are allowed =='
    foreach ($ok in @(1, 10000)) {
        $script:written = @{}
        $RateExp = $ok; $RateDrop = -1; $RateYang = -1; $HoldBots = -1; $Yes = $true
        Set-FreshWorldSettings -Reason 'test'
        Check ("accepted $ok") "$ok" $script:written['M2_RATE_EXP']
    }

    Write-Host '== Test-RatePercent on its own =='
    Check '100 is a rate' $true (Test-RatePercent -Value 100)
    Check '0 is not' $false (Test-RatePercent -Value 0)
    Check '-5 is not' $false (Test-RatePercent -Value -5)
}
finally {
    Write-Host ''
    Write-Host ("passed " + $script:pass + ", failed " + $script:fail) -ForegroundColor $(if ($script:fail) { 'Red' } else { 'Green' })
}
if ($script:fail -gt 0) { exit 1 }
