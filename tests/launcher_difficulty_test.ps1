# Set-DifficultyAction: what the world's difficulty writes to .env.
#
# The presets carry the skill books' waits since the books joined the
# difficulty (drip9660, 23 September): hard is the package's 21 hours for the
# players and the bots, medium a third of it, easy none. Custom takes the four
# hour counts it is given; a caller that passes no book hours (an older GUI)
# keeps what .env already says rather than writing a zero over it.
#
# Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\launcher_difficulty_test.ps1
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

# The launcher cannot be dot-sourced (it would run a menu); the functions and
# the preset table are lifted out of its syntax tree.
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($launcher, [ref]$null, [ref]$errors)
if ($errors -and @($errors).Count -gt 0) { throw "Metin2-Launcher.ps1 does not parse" }
foreach ($name in @('Test-DifficultyHours', 'Set-DifficultyAction')) {
    $fn = $ast.Find({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name }, $true)
    if (-not $fn) { throw "no function $name in the launcher" }
    . ([scriptblock]::Create($fn.Extent.Text))
}
$presets = $ast.Find({ param($n) $n -is [System.Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$script:DifficultyPresets' }, $true)
if (-not $presets) { throw 'no $script:DifficultyPresets in the launcher' }
. ([scriptblock]::Create($presets.Extent.Text))

# .env as a table the test can read and seed.
$script:env = @{}
function Get-DotEnvValue { param([string]$Key, [string]$Default = '') if ($script:env.ContainsKey($Key)) { return $script:env[$Key] } return $Default }
function Set-DotEnvValue { param([string]$Key, [string]$Value) $script:env[$Key] = $Value }
function Start-Server { $script:started = $true }
function Confirm-Operation { param([string]$Question) return $false }

$Yes = $false
$Difficulty = ''; $BiologistHours = ''; $HorseHours = ''; $BookHours = ''; $BotBookHours = ''

Write-Host '== hard is the package: 21 hours between books, players and bots =='
$script:env = @{}
$Difficulty = 'hard'
Set-DifficultyAction
Check 'level' 'hard' $script:env['M2_DIFFICULTY']
Check 'Biologist' '24' $script:env['M2_BIOLOGIST_WAIT_HOURS']
Check 'players books' '21' $script:env['M2_BOOK_WAIT_HOURS']
Check 'bots books' '21' $script:env['M2_BOT_BOOK_WAIT_HOURS']

Write-Host '== medium is a third, easy is none =='
$Difficulty = 'medium'
Set-DifficultyAction
Check 'medium players books' '7' $script:env['M2_BOOK_WAIT_HOURS']
Check 'medium bots books' '7' $script:env['M2_BOT_BOOK_WAIT_HOURS']
$Difficulty = 'easy'
Set-DifficultyAction
Check 'easy players books' '0' $script:env['M2_BOOK_WAIT_HOURS']
Check 'easy bots books' '0' $script:env['M2_BOT_BOOK_WAIT_HOURS']

Write-Host '== custom writes the numbers it is given, commas read as points =='
$Difficulty = 'custom'; $BiologistHours = '1'; $HorseHours = '2'; $BookHours = '12,5'; $BotBookHours = '0'
Set-DifficultyAction
Check 'players books' '12.5' $script:env['M2_BOOK_WAIT_HOURS']
Check 'bots books' '0' $script:env['M2_BOT_BOOK_WAIT_HOURS']
Check 'horse' '2' $script:env['M2_HORSE_WAIT_HOURS']

Write-Host '== an older GUI with no book hours keeps what .env says =='
$script:env['M2_BOOK_WAIT_HOURS'] = '5'
$script:env['M2_BOT_BOOK_WAIT_HOURS'] = '3'
$Difficulty = 'custom'; $BiologistHours = '1'; $HorseHours = '2'; $BookHours = ''; $BotBookHours = ''
Set-DifficultyAction
Check 'players books kept' '5' $script:env['M2_BOOK_WAIT_HOURS']
Check 'bots books kept' '3' $script:env['M2_BOT_BOOK_WAIT_HOURS']

Write-Host '== a number out of range stops it =='
$Difficulty = 'custom'; $BookHours = '721'; $BotBookHours = '0'
$threw = $false
try { Set-DifficultyAction } catch { $threw = $true }
Check '721 hours refused' $true $threw

Write-Host ''
Write-Host ("passed {0}, failed {1}" -f $script:pass, $script:fail)
if ($script:fail -gt 0) { exit 1 }
