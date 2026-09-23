# What a support bundle may not carry: the passwords the launcher prints.
#
# The bundle's logs go through Protect-M2LogContent, whose rules knew English
# "password=" and a heading in English, while the launcher prints the panel
# password it makes in Polish - and pattsito's bundle carried his onto a public
# channel on 23 September. Pinned here in every shape the launcher prints one:
# the line from start-server.ps1, the heading with the password below it (with
# the blank lines of a raw action log and with the timestamps of the GUI's
# session log), the password button, and a COOP friend's account line - and
# that the lines around them keep their words.
#
# Run with Windows PowerShell 5.1, the launcher's engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\launcher_log_redaction_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root 'launcher\Metin2Launcher.psm1') -Force
$module = Get-Module Metin2Launcher

$script:pass = 0
$script:fail = 0
function Check {
    param([string]$What, [bool]$Ok, [string]$Detail = '')
    if ($Ok) {
        Write-Host ("  OK   " + $What) -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host ("  FAIL " + $What + $(if ($Detail) { ": " + $Detail } else { '' })) -ForegroundColor Red
        $script:fail++
    }
}
function Protect { param([string]$Text) return (& $module { param($t) Protect-M2LogContent -Text $t } $Text) }

$nl = "`r`n"
$cases = @(
    @{ Name = 'start-server line, GUI log'
       Text = "2026-09-23 08:42:36  Nie bylo pliku .env (instalator nie byl uruchamiany) - utworzono go z nowymi haslami.$nl" +
              "2026-09-23 08:42:36  Haslo do panelu administracyjnego: Xq7kQ2mNp9RsT4vWz1Ab$nl" +
              "2026-09-23 08:42:36  Zapisz je. Jest tez w pliku linux-port\docker\.env (M2_PANEL_PASSWORD).$nl"
       Secret = 'Xq7kQ2mNp9RsT4vWz1Ab'; Keep = @('Zapisz je. Jest tez', 'utworzono go z nowymi haslami') },
    @{ Name = 'heading with the password below it, raw action log'
       Text = "=============================================================$nl" +
              "  HASLO DO PANELU WWW (wygenerowane, bo w .env go nie bylo)$nl$nl" +
              "      Mn4bV8cX2zL6kJ9h$nl$nl" +
              "  Jest tez w linux-port\docker\.env (M2_PANEL_PASSWORD).$nl"
       Secret = 'Mn4bV8cX2zL6kJ9h'; Keep = @('Jest tez w linux-port') },
    @{ Name = 'heading with the password below it, GUI log'
       Text = "2026-09-23 08:42:36    HASLO DO PANELU WWW (wygenerowane, bo w .env go nie bylo)$nl" +
              "2026-09-23 08:42:36        Mn4bV8cX2zL6kJ9h$nl" +
              "2026-09-23 08:42:36    Jest tez w linux-port\docker\.env (M2_PANEL_PASSWORD).$nl"
       Secret = 'Mn4bV8cX2zL6kJ9h'; Keep = @('Jest tez w linux-port') },
    @{ Name = 'the password button'
       Text = "Haslo do panelu WWW (z pliku linux-port\docker\.env):$nl  Ab3dEf6hIj9kLm2n$nl$nl" +
              "Jesli panel go nie przyjmuje, znaczy to, ze zapamietal starsze haslo.$nl"
       Secret = 'Ab3dEf6hIj9kLm2n'; Keep = @('Jesli panel go nie przyjmuje') },
    @{ Name = 'a COOP friend''s account line, with the Polish letter'
       Text = "Konto dla Adam: login m2f_adam, hasło Qw8eRt5yUi, kod usuwania postaci 1234567$nl"
       Secret = 'Qw8eRt5yUi'; Keep = @('login m2f_adam', 'kod usuwania postaci') },
    @{ Name = 'LF line endings'
       Text = "Haslo do panelu WWW (z pliku linux-port\docker\.env):`n  Ab3dEf6hIj9kLm2n`nJesli panel`n"
       Secret = 'Ab3dEf6hIj9kLm2n'; Keep = @('Jesli panel') }
)
foreach ($case in $cases) {
    Write-Host ("== " + $case.Name + " ==")
    $safe = Protect -Text $case.Text
    Check 'the password is gone' ($safe.IndexOf($case.Secret) -lt 0) $safe
    foreach ($keep in $case.Keep) {
        Check ("kept: " + $keep) ($safe.IndexOf($keep) -ge 0) $safe
    }
}

Write-Host '== ordinary lines are left alone =='
$plain = "2026-09-23 08:41:16  Zainstalowany serwer: 2.0.71${nl}2026-09-23 08:41:16  Dostepny serwer:     2.0.97${nl}"
Check 'unchanged' ((Protect -Text $plain) -eq $plain) (Protect -Text $plain)

Write-Host ''
Write-Host ("{0} passed, {1} failed" -f $script:pass, $script:fail)
if ($script:fail -gt 0) { exit 1 }
