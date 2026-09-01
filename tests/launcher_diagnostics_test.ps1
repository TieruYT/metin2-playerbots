[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$modulePath = Join-Path $root 'launcher\Metin2Launcher.Diagnostics.psm1'

$parserErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($modulePath, [ref]$null, [ref]$parserErrors)
if (@($parserErrors).Count -ne 0) {
    throw "Moduł diagnostyczny zawiera błędy składni: $($parserErrors | Out-String)"
}

Import-Module $modulePath -Force

$cases = @(
    [pscustomobject]@{
        Name = 'Docker port collision'
        Text = 'driver failed programming external connectivity: Bind for 127.0.0.1:7788 failed: port is already allocated'
        Expected = 'PORT_IN_USE'
    },
    [pscustomobject]@{
        Name = 'Virtualization disabled'
        Text = 'Virtualization support not detected'
        Expected = 'VIRTUALIZATION_DISABLED'
    },
    [pscustomobject]@{
        Name = 'Broken WSL'
        Text = 'There was a problem with WSL. wsl.exe exit status 1'
        Expected = 'WSL_BROKEN'
    },
    [pscustomobject]@{
        Name = 'Docker Engine timeout'
        Text = 'Docker Engine did not become ready within 180 seconds.'
        Expected = 'DOCKER_NOT_READY'
    },
    [pscustomobject]@{
        Name = 'Legacy installer destination'
        Text = 'docker.exe : cannot overwrite non-directory artifacts.json with directory C:\Users\tester\Metin2Server'
        Expected = 'LEGACY_INSTALLER_DESTINATION'
    },
    [pscustomobject]@{
        Name = 'Unpublished update channel'
        Text = 'Serwer zdalny zwrócił błąd: (404) Nie znaleziono.'
        Expected = 'UPDATE_CHANNEL_UNPUBLISHED'
    }
)

foreach ($case in $cases) {
    $actual = Get-M2LauncherErrorGuidance -Text $case.Text
    if ($actual.Code -ne $case.Expected) {
        throw "$($case.Name): oczekiwano $($case.Expected), otrzymano $($actual.Code)."
    }
    if (-not $actual.Title -or -not $actual.Message -or -not $actual.Remedy) {
        throw "$($case.Name): komunikat dla użytkownika jest niekompletny."
    }
}

$unknown = Get-M2LauncherErrorGuidance -Text 'unexpected test failure'
if ($unknown.Code -ne 'UNKNOWN') {
    throw "Nieznany błąd powinien używać kodu UNKNOWN, otrzymano $($unknown.Code)."
}

[pscustomobject]@{
    Result = 'OK'
    ParserErrors = @($parserErrors).Count
    ClassifiedCases = $cases.Count
    UnknownFallback = $unknown.Code
} | ConvertTo-Json
