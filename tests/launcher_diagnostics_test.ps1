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
        Name = 'Docker WSL disk read-only'
        Text = 'failed to solve: write /var/lib/desktop-containerd/daemon/io.containerd.metadata.v1.bolt/meta.db: read-only file system'
        Expected = 'DOCKER_DISK_BROKEN'
    },
    [pscustomobject]@{
        # What the first build on a disk gone bad says (pattsito, 23 September).
        Name = 'Docker buildkit I/O error'
        Text = '#16 ERROR: file sync error: sync /var/lib/docker/buildkit/containerd-overlayfs/metadata_v2.db: input/output error'
        Expected = 'DOCKER_DISK_BROKEN'
    },
    [pscustomobject]@{
        # The preflight's write, refused, followed by the remedy it prints:
        # "wsl --shutdown" in the remedy must not read as a broken WSL.
        Name = 'Docker disk probe refused'
        Text = ("BŁĄD: dysk Dockera nie przyjmuje zapisu (Error response from daemon: create m2-disk-probe-0123456789ab: error while creating volume root path '/var/lib/docker/volumes/m2-disk-probe-0123456789ab/_data': mkdir /var/lib/docker/volumes/m2-disk-probe-0123456789ab: read-only file system)." +
                [Environment]::NewLine + (Get-M2DockerDiskRemedy))
        Expected = 'DOCKER_DISK_BROKEN'
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
        # Since 2.0.77 only a 404 said of the manifest itself (see the rule).
        Name = 'Unpublished update channel'
        Text = 'Nie udalo sie pobrac https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/update-manifest-mt2009.json: Serwer zdalny zwrócił błąd: (404) Nie znaleziono.'
        Expected = 'UPDATE_CHANNEL_UNPUBLISHED'
    },
    [pscustomobject]@{
        # ... and a 404 anywhere else in a failed action's output is not one:
        # a panel's missing icon sent archonek to wait for a channel that was
        # there all along (18 September).
        Name = 'A 404 that is not the manifest'
        Text = 'GET /static/icons/50300.png HTTP/1.1" 404 -'
        Expected = 'UNKNOWN'
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

# The remedy alone - which is all the "prepare" dialog shows - must not be
# taken for a broken WSL either, line by line.
if ((Get-M2LauncherErrorGuidance -Text (Get-M2DockerDiskRemedy)).Code -eq 'WSL_BROKEN') {
    throw 'Rada dla dysku Dockera nie może wyglądać jak zepsuty WSL.'
}

# A server folder OneDrive holds (Avalach, 24 September): found by the
# OneDrive variable, named by the guidance only for a build that lost a file,
# and nothing for a folder outside it or a prefix that only looks alike.
$savedOneDrive = [Environment]::GetEnvironmentVariable('OneDrive')
$fakeOneDrive = Join-Path ([IO.Path]::GetTempPath()) 'm2-onedrive-test\OneDrive'
try {
    [Environment]::SetEnvironmentVariable('OneDrive', $fakeOneDrive, 'Process')
    $inside = Join-Path $fakeOneDrive 'Desktop\Metin2 Singleplayer\Serwer'
    $outside = Join-Path ([IO.Path]::GetTempPath()) 'm2-onedrive-test\Metin2 Singleplayer\Serwer'
    $lookalike = $fakeOneDrive + ' - Firma\Serwer'
    if ((Get-M2OneDriveRootFor -Path $inside) -ne $fakeOneDrive) {
        throw 'Folder w OneDrive nie został rozpoznany.'
    }
    if ((Get-M2OneDriveRootFor -Path $outside) -or (Get-M2OneDriveRootFor -Path $lookalike)) {
        throw 'Folder poza OneDrive został wzięty za folder OneDrive.'
    }
    $missingHeader = '#82 2.006 ../../../Extern/include/boost/preprocessor/iteration/detail/iter/forward1.hpp:1343:14: fatal error: boost/preprocessor/iteration/detail/iter/limits/forward1_256.hpp: No such file or directory'
    if ((Get-M2LauncherErrorGuidance -Text $missingHeader -ServerRoot $inside).Code -ne 'ONEDRIVE_BUILD_CONTEXT') {
        throw 'Brakujący plik przy budowie w OneDrive powinien dać ONEDRIVE_BUILD_CONTEXT.'
    }
    if ((Get-M2LauncherErrorGuidance -Text $missingHeader -ServerRoot $outside).Code -eq 'ONEDRIVE_BUILD_CONTEXT') {
        throw 'Folder poza OneDrive nie może dostać rady o OneDrive.'
    }
    if ((Get-M2LauncherErrorGuidance -Text 'unexpected test failure' -ServerRoot $inside).Code -ne 'UNKNOWN') {
        throw 'Rada o OneDrive tylko dla budowy, która zgubiła plik.'
    }
}
finally {
    [Environment]::SetEnvironmentVariable('OneDrive', $savedOneDrive, 'Process')
}

# Where Docker keeps its disk: a folder always, a drive and its free space when
# Windows answers. A bundle's disk report never carries the profile path.
$dockerData = Get-M2DockerDataLocation
if (-not $dockerData -or -not $dockerData.Directory) {
    throw 'Nie ustalono folderu dysku Dockera.'
}
$diskReport = Get-M2DiskSpaceReport -ServerRoot $root
if ($diskReport -notmatch 'Dyski Windows' -or $diskReport -notmatch 'Docker Desktop') {
    throw "Raport miejsca na dyskach jest niepełny: $diskReport"
}
$profilePath = [Environment]::GetFolderPath('UserProfile')
if ($profilePath -and $diskReport.IndexOf($profilePath, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw 'Raport miejsca na dyskach nie może zawierać ścieżki profilu użytkownika.'
}

# A refused write, as the daemon words it on a read-only disk, is a fault named
# by its last line; a write refused for any other reason is not this check's
# to name. The docker call is replaced inside the module for that.
$module = Get-Module Metin2Launcher.Diagnostics
$refusals = @(
    @{ Output = "Error response from daemon: create m2-disk-probe-x: error while creating volume root path '/var/lib/docker/volumes/m2-disk-probe-x/_data': mkdir /var/lib/docker/volumes/m2-disk-probe-x: read-only file system"; Fault = $true },
    @{ Output = "Error response from daemon: write /var/lib/docker/volumes/metadata.db: input/output error"; Fault = $true },
    @{ Output = "Error response from daemon: open /var/lib/docker/volumes/x: no space left on device"; Fault = $true },
    @{ Output = "Error response from daemon: permission denied"; Fault = $false },
    @{ Output = 'Polecenie nie odpowiedziało w wyznaczonym czasie.'; Fault = $false }
)
foreach ($refusal in $refusals) {
    $answer = & $module {
        param($Text)
        $script:probeText = $Text
        function script:Invoke-M2DiagnosticProcess {
            param([string]$FileName, [string]$Arguments, [int]$TimeoutMilliseconds)
            return [pscustomobject]@{ ExitCode = 1; TimedOut = $false; Output = ("Unrelated first line`n" + $script:probeText) }
        }
        try { return (Get-M2DockerDiskFault) }
        finally { Remove-Item -LiteralPath 'function:script:Invoke-M2DiagnosticProcess' -ErrorAction SilentlyContinue }
    } $refusal.Output
    if ($refusal.Fault -and $answer -ne $refusal.Output) {
        throw "Odmowa zapisu powinna zostać nazwana ostatnią linią demona, otrzymano: [$answer]"
    }
    if (-not $refusal.Fault -and $answer) {
        throw "Odmowa z innego powodu nie jest usterką dysku, otrzymano: [$answer]"
    }
}
# The module's own docker call is back after the stub.
Import-Module $modulePath -Force

# On an engine that works the write succeeds and leaves no volume behind.
$previousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& docker info --format '{{.ServerVersion}}' 1>$null 2>$null
$engineUp = $LASTEXITCODE -eq 0
$ErrorActionPreference = $previousPreference
if ($engineUp) {
    $fault = Get-M2DockerDiskFault
    if ($fault) { throw "Zapis na działający dysk Dockera nie powinien zawieść: $fault" }
    $ErrorActionPreference = 'Continue'
    $leftovers = @(& docker volume ls --filter 'label=com.metin2.probe=1' --format '{{.Name}}' 2>$null | Where-Object { $_ })
    $ErrorActionPreference = $previousPreference
    if ($leftovers.Count -ne 0) { throw "Próba zapisu zostawiła wolumeny: $($leftovers -join ', ')" }
}

# The collision that actually stops an update is not the panel's. 7790 is the
# advanced panel, which a second copy of the server publishes too, and the
# remedy has to say why quitting Docker Desktop does not help.
$secondPanel = Get-M2LauncherErrorGuidance -Text (
    'driver failed programming external connectivity on endpoint m2zip-seban-panel: ' +
    'Bind for 127.0.0.1:7790 failed: port is already allocated')
if ($secondPanel.Code -ne 'PORT_IN_USE') {
    throw "Kolizja na porcie 7790 powinna dać PORT_IN_USE, otrzymano $($secondPanel.Code)."
}
if ($secondPanel.Title -notmatch '7790') {
    throw "Komunikat powinien nazywać port 7790, otrzymano: $($secondPanel.Title)"
}
if ($secondPanel.Remedy -notmatch 'unless-stopped') {
    throw 'Rada przy zajętym porcie musi tłumaczyć, dlaczego samo wyłączenie Dockera nie pomaga.'
}

# Every published port comes from the installation's own .env: compose gives up
# on the first one that is taken, so a preflight that knows only 7788 passes and
# the build dies afterwards.
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('m2ports-' + [Guid]::NewGuid().ToString('N'))
$expectedPorts = @(7788, 7790, 7791, 11000, 13000, 13001, 13002, 3306)
try {
    New-Item -ItemType Directory -Path (Join-Path $fixture 'linux-port\docker') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $fixture 'linux-port\docker\.env') -Encoding UTF8 -Value @(
        'M2_PANEL_PUBLIC_PORT=7788',
        'M2_SEBAN_PANEL_PORT=7790',
        'M2_ITEMSHOP_PUBLIC_PORT=7791',
        'M2_AUTH_PORT=11000',
        'M2_GAME_PORT_RANGE=13000-13002',
        'M2_DB_PUBLISH_PORT=3306')
    $stackPorts = @(Get-M2StackHostPorts -ServerRoot $fixture | ForEach-Object { [int]$_.Port })
    foreach ($expected in $expectedPorts) {
        if ($stackPorts -notcontains $expected) {
            throw "Preflight musi sprawdzać port $expected; otrzymano: $($stackPorts -join ', ')."
        }
    }
}
finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }

# Regresja z 2.0.31: Docker drukuje opublikowany zakres jako JEDEN wpis
# ("127.0.0.1:13000-13002->13000-13002/tcp"), a szukanie dosłownego "13001->"
# nie trafiało w żaden kanał gry. Preflight uznawał wtedy własny, działający
# serwer gracza za obcy program i odmawiał startu (sizowski).
$portsColumn = '127.0.0.1:11000->11000/tcp, 127.0.0.1:13000-13002->13000-13002/tcp'
$matchedPorts = @(Get-M2PublishedPortMatches -PortsText $portsColumn -Ports @(7788, 11000, 13000, 13001, 13002))
foreach ($expected in @(11000, 13000, 13001, 13002)) {
    if ($matchedPorts -notcontains $expected) {
        throw "Port $expected z zakresu musi zostać rozpoznany; otrzymano: $($matchedPorts -join ', ')."
    }
}
if ($matchedPorts -contains 7788) {
    throw 'Port spoza opublikowanej listy nie może zostać dopasowany.'
}

# Pojedynczy port, adres IPv6 i wpis bez adresu - wszystkie trzy postacie, w
# jakich Docker podaje stronę hosta.
$singleMatches = @(Get-M2PublishedPortMatches -PortsText '[::]:7788->7788/tcp, 7790->7789/tcp' -Ports @(7788, 7790, 7791))
foreach ($expected in @(7788, 7790)) {
    if ($singleMatches -notcontains $expected) {
        throw "Port $expected musi zostać rozpoznany; otrzymano: $($singleMatches -join ', ')."
    }
}
if ($singleMatches -contains 7791) {
    throw 'Nieopublikowany port nie może zostać dopasowany.'
}

# Port wystawiony tylko wewnątrz sieci (bez "->") nie jest publikowany na hoście.
if (@(Get-M2PublishedPortMatches -PortsText '7789/tcp' -Ports @(7789)).Count -ne 0) {
    throw 'Port bez publikacji na hoście nie może zostać dopasowany.'
}

[pscustomobject]@{
    Result = 'OK'
    ParserErrors = @($parserErrors).Count
    ClassifiedCases = $cases.Count
    UnknownFallback = $unknown.Code
    DockerDiskProbed = $engineUp
} | ConvertTo-Json
