[CmdletBinding()]
param(
    [ValidateSet('Menu', 'Start', 'Stop', 'StartDocker', 'StopAll', 'Check', 'UpdateServer', 'UpdateClient', 'UpdateAll', 'Diagnose', 'Logs', 'SendLogs', 'Configure', 'SetBots', 'SetDifficulty', 'ImportDb', 'BackupDb', 'RestoreDb', 'ResetWorld', 'RepairDb', 'DbAccess', 'PanelPassword', 'FreePorts', 'CoopCheck', 'CoopSecure', 'CoopAddFriend', 'CoopBlockFriend', 'CoopUnblockFriend', 'CoopInvite', 'CoopHost', 'CoopStop', 'CoopRenew', 'CoopJoin')]
    [string]$Action = 'Menu',
    [string]$Manifest = '',
    [int]$BotCount = -1,
    # The spawn plan beside the count (SetBots): -1 leaves .env as it is.
    [int]$SpawnMinutes = -1,
    [int]$LateJoiners = -1,
    [int]$LateHours = -1,
    # SetBots: the operator's own number per kingdom (1 = on, 0 = off, -1 =
    # leave it) and the three numbers, and the second channel with its share.
    [int]$PerKingdom = -1,
    [int]$ShinsooBots = -1,
    [int]$ChunjoBots = -1,
    [int]$JinnoBots = -1,
    [int]$Channel2 = -1,
    [int]$Channel2Share = -1,
    # SetDifficulty: easy | medium | hard | custom, and the hours custom reads.
    [string]$Difficulty = '',
    [string]$BiologistHours = '',
    [string]$HorseHours = '',
    # And the waits between two skill books, players' and bots' (custom).
    [string]$BookHours = '',
    [string]$BotBookHours = '',
    # The rates a fresh world starts on, asked for when one is about to be
    # made (ResetWorld, and the first start of an install that has no database
    # yet). -1 leaves .env as it is, which is what every other caller wants.
    [int]$RateExp = -1,
    [int]$RateDrop = -1,
    [int]$RateYang = -1,
    # And whether that world comes up with the bots held at the door: 1 = held
    # until the operator lets them in, 0 = they walk in with the world.
    [int]$HoldBots = -1,
    # And whether a player's new character there gets the apprentice chest:
    # 1 = yes, 0 = no, -1 leaves .env as it is.
    [int]$StarterChest = -1,
    [string]$ImportSource = '',
    [string]$RestoreSource = '',
    # COOP (experimental): the friend's name for CoopAddFriend, a friend's
    # login for CoopBlockFriend/CoopInvite, and the code CoopJoin reads.
    [string]$FriendName = '',
    [string]$FriendLogin = '',
    [string]$Invite = '',
    # CoopHost: how the world is offered - auto (the Internet where it can
    # reach this machine, a VPN found here where it cannot), internet, or one
    # VPN by name (vpn = the first one found).
    [ValidateSet('auto', 'internet', 'vpn', 'radmin', 'tailscale', 'zerotier', 'hamachi')]
    [string]$CoopVia = 'auto',
    # ResetWorld only: bring the server up on the fresh world right away, so
    # "wyzeruj swiat i zacznij od nowa" is one click and not a reset followed
    # by GRAJ.
    [switch]$ThenStart,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'

# The GUI runs this script hidden with its stdout redirected into a file and
# reads that file back as UTF-8. Without this the redirect gets the console's
# OEM code page instead, and every Polish letter this script prints reaches the
# log broken - "Serwer dzia?a w wersji", while the GUI's own lines beside them
# are fine. Both ends speak UTF-8 now.
try {
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    $OutputEncoding = [Text.UTF8Encoding]::new($false)
}
catch { }

$serverRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$modulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.psm1'
$diagnosticsModulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.Diagnostics.psm1'
$configPath = Join-Path $serverRoot '.m2launcher.json'
$statePath = Join-Path $serverRoot '.m2launcher-state.json'
# Written when new files are already on disk but Docker did not finish building
# them. Until it is gone the installation is not really on the version its
# VERSION file claims, and starting it would run the previous images.
$rebuildMarkerPath = Join-Path $serverRoot '.m2launcher-rebuild-pending'

foreach ($requiredModule in @($modulePath, $diagnosticsModulePath)) {
    if (-not (Test-Path -LiteralPath $requiredModule -PathType Leaf)) {
        throw "Brakuje modułu launchera: $requiredModule"
    }
}
Import-Module $modulePath -Force

# Where the time goes. Every action prints a "[faza]" line with the seconds
# since the action began at each point that can be slow - Docker checks, the
# download, the file swap, the image build, compose up - so a launcher log
# from a player says which of them took the ten minutes instead of "the
# update is slow". The GUI stamps every line with the clock as well; this is
# for the CLI, and for reading a log without doing the subtraction.
$script:phaseWatch = [Diagnostics.Stopwatch]::StartNew()
function Write-Phase {
    param([Parameter(Mandatory = $true)][string]$Name)
    Write-Host ("[faza] {0} (+{1} s od poczatku akcji)" -f $Name, [int]$script:phaseWatch.Elapsed.TotalSeconds) -ForegroundColor DarkCyan
}
Import-Module $diagnosticsModulePath -Force
# COOP (experimental, the local branch "coop"): an optional module; without
# it the Coop* actions say so and nothing else changes.
$coopModulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.Coop.psm1'
if (Test-Path -LiteralPath $coopModulePath -PathType Leaf) { Import-Module $coopModulePath -Force }

function Write-Header {
    Clear-Host
    Write-Host '========================================================' -ForegroundColor DarkYellow
    Write-Host '  Metin2 Singleplayer - Launcher i aktualizacje' -ForegroundColor Yellow
    Write-Host '========================================================' -ForegroundColor DarkYellow
    Write-Host ''
}

function Get-Config {
    return Get-M2LauncherConfig -ServerRoot $serverRoot -ConfigPath $configPath
}

function Get-ManifestSource {
    param($Config)
    if ($Manifest) { return $Manifest }
    return [string]$Config.manifestUrl
}

function Test-RebuildPending {
    return (Test-Path -LiteralPath $rebuildMarkerPath -PathType Leaf)
}

function Read-RecordedState {
    # What the files on disk are, whatever the images are: the versions the
    # last updates recorded, else VERSION and the client the full package
    # shipped (New-M2DeployTree.ps1 puts CLIENT_VERSION beside VERSION). A
    # recorded "unknown" is no record - older launchers wrote one back (see
    # Read-State) - and a state file that does not parse, which a crash can
    # leave behind, must not stop every action.
    $versionFile = Join-Path $serverRoot 'VERSION'
    $onDisk = if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
        (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    else { 'unknown' }
    $clientMarker = Join-Path $serverRoot 'CLIENT_VERSION'
    $shippedClient = if (Test-Path -LiteralPath $clientMarker -PathType Leaf) {
        (Get-Content -LiteralPath $clientMarker -Raw).Trim()
    }
    else { 'unknown' }
    $server = ''
    $client = ''
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $saved = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($saved -and $saved.PSObject.Properties['server']) { $server = ([string]$saved.server).Trim() }
            if ($saved -and $saved.PSObject.Properties['client']) { $client = ([string]$saved.client).Trim() }
        }
        catch { }
    }
    if (-not $server -or $server -eq 'unknown') { $server = $onDisk }
    if (-not $client -or $client -eq 'unknown') { $client = $shippedClient }
    return [pscustomobject]@{ schema = 1; server = $server; client = $client }
}

function Read-State {
    # An interrupted update leaves the new VERSION file on disk while the running
    # containers are still the old ones. Reporting that version would make the
    # update check answer "already up to date" and never rebuild, which is the
    # state a player cannot get out of on their own. The server alone: nothing
    # of the client is built, and hiding its version as well had two costs -
    # the update offered the client again, and Save-State, which read through
    # here, wrote "unknown" over the version a client update had just recorded
    # (pattsito, 23 September, client 2.0.26 recorded and lost two minutes later).
    $state = Read-RecordedState
    if (Test-RebuildPending) { $state.server = 'unknown' }
    return $state
}

function Save-State {
    param([string]$ServerVersion, [string]$ClientVersion)
    $state = Read-RecordedState
    if ($ServerVersion) { $state.server = $ServerVersion }
    if ($ClientVersion) { $state.client = $ClientVersion }
    $state | Select-Object schema, server, client | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
}

function Get-ManifestComponent {
    param(
        [Parameter(Mandatory = $true)]$RemoteManifest,
        [Parameter(Mandatory = $true)][ValidateSet('server', 'client')][string]$Name
    )
    $property = $RemoteManifest.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) { return $null }
    $component = $property.Value
    if (-not [string]$component.version -or -not [string]$component.url -or -not [string]$component.sha256) {
        return $null
    }
    return $component
}

function Test-InstalledVersion {
    param(
        [AllowEmptyString()][string]$Installed,
        [AllowEmptyString()][string]$Available
    )
    if (-not $Installed -or -not $Available -or $Installed -eq 'unknown') { return $false }
    return $Installed.Trim().Equals($Available.Trim(), [StringComparison]::OrdinalIgnoreCase)
}

function Confirm-Operation {
    param([Parameter(Mandatory = $true)][string]$Question)
    if ($Yes) { return $true }
    $answer = Read-Host "$Question [t/N]"
    return $answer -match '^(t|tak|y|yes)$'
}

function Show-DockerDiagnostics {
    param([switch]$CheckPanelPort)

    $report = Get-M2DockerPreflight -ServerRoot $serverRoot -CheckPanelPort:$CheckPanelPort
    $text = Format-M2DockerPreflightReport -Report $report
    Write-Host $text -ForegroundColor $(if ($report.CanStart) { 'Green' } else { 'Yellow' })
    return $report
}

function Assert-DockerPrerequisites {
    param([switch]$CheckPanelPort)

    $report = Show-DockerDiagnostics -CheckPanelPort:$CheckPanelPort
    if (-not $report.CanStart) {
        throw (@($report.BlockingIssues) -join [Environment]::NewLine)
    }
}

function Assert-DockerDiskWritable {
    # Before a build, and before an update swaps a single file: a Docker disk
    # gone read-only fails every build at its first write, and after the
    # first failure Docker only says "failed to solve: exit code: 255".
    # pattsito (23 September) downloaded and applied the same update five
    # times in forty minutes against such a disk. An engine that is not
    # running cannot be asked; Rebuild-Server starts it and asks again.
    # -KeepRebuildPending: the files are already the new ones, so a later
    # GRAJ must still finish the build.
    param([switch]$KeepRebuildPending, [string]$Before = 'budowanie serwera')
    if (-not (Test-M2DockerRunning)) { return }
    $fault = Get-M2DockerDiskFault
    if (-not $fault) { return }
    if ($KeepRebuildPending) {
        Set-Content -LiteralPath $rebuildMarkerPath -Value ([DateTime]::UtcNow.ToString('o')) -Encoding UTF8
    }
    throw ("Przerywam $Before - dysk Dockera nie przyjmuje zapisu:" + [Environment]::NewLine +
           $fault + [Environment]::NewLine + [Environment]::NewLine + (Get-M2DockerDiskRemedy))
}

function Start-Server {
    # Before the preflight refuses the start: an old installation takes the
    # ports back on every engine start, so a check that only names it leaves the
    # player exactly where they were.
    Clear-PortConflicts -Quiet | Out-Null
    Assert-DockerPrerequisites -CheckPanelPort
    Write-Phase 'Docker sprawdzony'
    # A second-channel wish left in the web panel, before .env is read.
    try { Sync-ChannelWishFromPanel }
    catch { Write-Host "Nie udalo sie odczytac ustawienia kanalow z panelu WWW: $($_.Exception.Message)" -ForegroundColor Yellow }
    # start-server.ps1 brings the stack up from the images that already exist.
    # After an interrupted update those are the old ones, so finish the build
    # first - otherwise the player keeps running the previous server and the
    # website keeps showing the previous panel.
    if (Test-RebuildPending) {
        Write-Host 'Poprzednia aktualizacja nie dokonczyla budowania. Dokancczam je teraz...' -ForegroundColor Yellow
        Rebuild-Server
        Write-Host 'Budowanie zakonczone.' -ForegroundColor Green
    }
    $script = Join-Path $serverRoot 'start-server.ps1'
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) { throw 'Brakuje start-server.ps1.' }
    & $script
    if ($LASTEXITCODE -ne 0) { throw "Uruchamianie serwera zakończyło się kodem $LASTEXITCODE." }
    Write-Phase 'Serwer uruchomiony'
    # COOP: a world hosted before this start is still hosted - .env keeps the
    # address - so the router's four-hour lease is renewed here.
    try { Update-CoopHostingLease }
    catch { Write-Host "COOP: nie udalo sie odnowic przekierowan w routerze: $($_.Exception.Message)" -ForegroundColor Yellow }
}

function Stop-Server {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'SilentlyContinue'
        docker info 1>$null 2>$null
        $dockerAvailable = $LASTEXITCODE -eq 0
    }
    finally { $ErrorActionPreference = $previousPreference }
    if (-not $dockerAvailable) {
        Write-Host 'Docker jest już zatrzymany.' -ForegroundColor Yellow
        return
    }
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    # `docker compose' writes progress to stderr; under $ErrorActionPreference=
    # 'Stop' Windows PowerShell 5.1 turns that into a terminating error and the
    # stop reports failure even when it worked. Decide from the exit code.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        docker compose --project-directory $composeDir -f $composeFile stop
        $stopExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($stopExit -ne 0) { throw "Zatrzymywanie serwera zakończyło się kodem $stopExit." }
}

# Another installation of this same server, sitting on the ports this one
# publishes. Every container ships `restart: unless-stopped`, so Docker Desktop
# starts the old project again on every engine start and it binds the ports
# before this installation can - which is why quitting Docker by hand never
# helped ("nawet jak recznie wylacze calkowicie docker"). `docker stop` is what
# holds, because its manual-stop flag survives an engine restart. Volumes are
# never touched: the collision is containers, and a removed volume is the world.
function Clear-PortConflicts {
    param([switch]$Quiet)

    if (-not (Test-M2DockerRunning)) { return 0 }
    $holders = @(Get-M2ForeignPortHolders -ServerRoot $serverRoot)
    if ($holders.Count -eq 0) {
        if (-not $Quiet) {
            Write-Host 'Zadna inna instalacja nie trzyma portow tego serwera.' -ForegroundColor Green
        }
        return 0
    }
    foreach ($holder in $holders) {
        $where = if ($holder.WorkingDir) { " (folder: $($holder.WorkingDir))" } else { '' }
        Write-Host ("Port {0}: trzyma go kontener {1} z instalacji '{2}'{3}." -f
            ((@($holder.Ports) | ForEach-Object { "$_" }) -join ', '), $holder.Container, $holder.Project, $where) -ForegroundColor Yellow
    }
    $stopped = @(Stop-M2ForeignPortHolders -ServerRoot $serverRoot)
    foreach ($entry in $stopped) {
        Write-Host ("Zatrzymano instalacje '{0}' ({1} kontenerow). Baza, wolumeny i postep sa nietkniete." -f
            $entry.Project, $entry.Containers) -ForegroundColor Green
    }
    return $stopped.Count
}

function Clear-PortConflictsAction {
    $freed = Clear-PortConflicts
    if ($freed -gt 0) {
        Write-Host 'Porty zwolnione. Mozesz kliknac GRAJ albo ponowic aktualizacje.' -ForegroundColor Green
    }
}

function Start-Docker {
    Assert-DockerPrerequisites
    $script = Join-Path $serverRoot 'start-server.ps1'
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) { throw 'Brakuje start-server.ps1.' }
    & $script -DockerOnly
    if ($LASTEXITCODE -ne 0) { throw "Uruchamianie Docker Desktop zakończyło się kodem $LASTEXITCODE." }
}

function Stop-DockerAndServer {
    Stop-Server
    $dockerCli = Join-Path $env:ProgramFiles 'Docker\Docker\DockerCli.exe'
    if (Test-Path -LiteralPath $dockerCli -PathType Leaf) {
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'SilentlyContinue'
            & $dockerCli -Shutdown 1>$null 2>$null
        }
        finally { $ErrorActionPreference = $previousPreference }
    }
    else {
        Get-Process -Name 'Docker Desktop', 'com.docker.backend' -ErrorAction SilentlyContinue |
            Stop-Process -ErrorAction SilentlyContinue
    }
    Write-Host 'Serwer i Docker Desktop zatrzymane. Dane pozostają zapisane w wolumenach.' -ForegroundColor Green
}

function Rebuild-Server {
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    # Stopping the server also stops Docker Desktop (see Stop-DockerAndServer),
    # so the sensible order - stop the server, then update it - always arrived
    # here with a dead engine and failed on a raw npipe error, after the files
    # had already been swapped. Start-Server has the same hole: it finishes a
    # pending build before start-server.ps1 gets a chance to bring the engine
    # up, so "click GRAJ" only ever worked when Docker happened to be running.
    # Both paths go through here, so the engine is ensured here as well.
    if (-not (Test-M2DockerRunning)) {
        Write-Host 'Silnik Dockera jest zatrzymany - uruchamiam go przed budowaniem.' -ForegroundColor Yellow
        Start-Docker
    }
    Assert-DockerDiskWritable -KeepRebuildPending
    # Compose needs the .env before it can build anything - the database
    # passwords are required variables. A copy unpacked by hand has no .env
    # until start-server.ps1 writes one, and that used to run only after this
    # build, so the update failed and "click GRAJ" failed the same way.
    $identityScript = Join-Path $serverRoot 'start-server.ps1'
    if (Test-Path -LiteralPath $identityScript -PathType Leaf) {
        & $identityScript -IdentityOnly
        if ($LASTEXITCODE -ne 0) { throw "Przygotowanie pliku .env zakonczylo sie kodem $LASTEXITCODE." }
    }
    # The overlay is the source of truth; the build context is only a copy of
    # it. Refresh the copy before Docker reads it, or an update that added a
    # source file compiles against the previous one - or, as in 1.23.2, against
    # a header that is not there at all.
    $synced = Sync-M2PlayerbotOverlay -ServerRoot $serverRoot
    if ($synced -gt 0) {
        Write-Host "Zsynchronizowano $synced plik(ow) zrodlowych bota do kontekstu budowania." -ForegroundColor DarkGray
    }
    Write-Phase 'Kontekst budowania przygotowany (.env, nakladka)'
    # The engine patches are part of the overlay too, and until now nothing on a
    # player's machine ever applied them.
    $patched = Invoke-M2EnginePatches -ServerRoot $serverRoot
    if ($patched -gt 0) {
        Write-Host "Nalozono $patched latek silnika." -ForegroundColor DarkGray
    }
    # And the sources the image is actually built from.
    #
    # This check exists in start-server.ps1 too, and that was not enough: this
    # path calls start-server.ps1 with -IdentityOnly, which returns after
    # writing the .env and never reaches it, then builds here. So a player
    # clicking GRAJ went straight to `docker compose --build' with an
    # incomplete context and got fifteen "failed to calculate checksum ... not
    # found" lines. Reported from the Discord twice, the second time against a
    # version that was supposed to have fixed it - because the fix was in the
    # half of the code that click does not run.
    #
    # linux-port/docker/game/src holds the r40250 tree, put there once by
    # fetch-sources.sh during installation. It is the operator's own package and
    # never travels in an update; what an update does put there is
    # src/server/game, because that is where the bot sources belong - which is
    # why a broken install still shows a plausible src/server/game and a build
    # context of about 1.6 MB where a complete one is hundreds of megabytes.
    $gameContext = Join-Path $serverRoot 'linux-port\docker\game\src'
    [void](Restore-M2EmptyGameContextDirs -ServerRoot $serverRoot)
    $requiredContext = @(Get-M2RequiredGameContext -ServerRoot $serverRoot)
    $missingContext = @()
    foreach ($entry in $requiredContext) {
        if (-not (Test-Path -LiteralPath (Join-Path $gameContext $entry))) {
            $missingContext += $entry
        }
    }
    # The dumps, the same way (see start-server.ps1 for why an initialised
    # database is exempt): this is the half of the code that click runs.
    $missingDumps = @(Get-M2MissingSqlDumps -ServerRoot $serverRoot)
    if ($missingDumps.Count -gt 0) {
        $dbVolume = Get-CurrentInstallTargetVolume
        $dbReady = $false
        if ($dbVolume) { $dbReady = Test-M2VolumeInitialized -Volume $dbVolume }
        if (-not $dbReady) {
            throw ("Brakuje zrzutow bazy danych, wiec pierwsza baza powstalaby pusta.`n`n" +
                   "Katalog: " + (Join-Path $serverRoot 'linux-port\docker\mariadb\initdb.d\dumps') + "`n" +
                   "Brakuje: " + ($missingDumps -join ', ') + "`n`n" +
                   "MariaDB wystartowalaby bez schematu gry (i zglosila 'healthy'), a playerbot-migrate " +
                   "czekalby 30 minut na tabele, ktore nigdy nie powstana. Zrzuty pochodza z Twojej " +
                   "paczki serwera r40250 (Server\metin2_mysql_dump.zip) i wystawia je wylacznie " +
                   "instalator - zadna aktualizacja ich nie przywroci.`n`n" +
                   "Uruchom ponownie instalator (installer\install.ps1) ze wskazana paczka " +
                   "(`$env:M2_SRC_ARCHIVE), albo rozpakuj metin2_mysql_dump.zip do tego katalogu " +
                   "i kliknij GRAJ jeszcze raz.")
        }
    }
    if ($missingContext.Count -gt 0) {
        throw ("Brakuje zrodel gry, wiec nie ma z czego zbudowac serwera.`n`n" +
               "Katalog: " + $gameContext + "`n" +
               "Brakuje: " + ($missingContext -join ', ') + "`n`n" +
               "To nie jest blad Dockera, WSL ani tej aktualizacji. Te pliki pochodza " +
               "z Twojej wlasnej paczki serwera r40250 i sa rozpakowywane raz, podczas " +
               "instalacji - zadna aktualizacja ich nie przywroci, bo nie wolno nam ich " +
               "rozpowszechniac.`n`n" +
               "Uruchom ponownie instalator (installer\install.ps1). Pobierze zrodla i " +
               "odtworzy kontekst budowania. Baza, postacie i ustawienia zostaja nietkniete.")
    }

    # The update is where a port collision hurts most: the images build for
    # minutes and compose then cannot bind a port another installation took back
    # while they were building ("Bind for 127.0.0.1:7790 failed"), so the whole
    # update is lost at its last step and the player is told to free a port they
    # cannot find.
    Clear-PortConflicts -Quiet | Out-Null

    # See Stop-Server: compose progress on stderr must not be treated as failure
    # under $ErrorActionPreference='Stop' in Windows PowerShell 5.1.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        # `up --build` on a fresh engine has raced its own pull: the images
        # were built, then "No such image: mariadb:10.11" while creating the
        # database container, and the update was reported as failed although
        # the second click succeeded. Pull what is not built first; a failure
        # here is not final, `up` tries again.
        docker compose --project-directory $composeDir -f $composeFile pull --ignore-buildable 2>&1 | Out-Null
        Write-Phase 'Obrazy bazowe pobrane, zaczynam docker compose up --build'
        Set-M2PlayerbotsVersionEnvironment -ServerRoot $serverRoot
        docker compose --project-directory $composeDir -f $composeFile up -d --build
        $buildExit = $LASTEXITCODE
        Write-Phase "docker compose up --build zakonczone (kod $buildExit)"
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($buildExit -ne 0) {
        Set-Content -LiteralPath $rebuildMarkerPath -Value ([DateTime]::UtcNow.ToString('o')) -Encoding UTF8
        throw 'Nowa wersja plików została zapisana, ale Docker nie zbudował serwera. Kliknij GRAJ — launcher dokończy budowanie. Kopia plików jest w katalogu backups.'
    }
    if (Test-RebuildPending) { Remove-Item -LiteralPath $rebuildMarkerPath -Force -ErrorAction SilentlyContinue }
}

function Show-UpdateStatus {
    param($RemoteManifest)
    $state = Read-State
    $serverComponent = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'server'
    $clientComponent = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'client'
    $messageProperty = $RemoteManifest.PSObject.Properties['statusMessage']
    if ($null -ne $messageProperty -and [string]$messageProperty.Value) {
        Write-Host ([string]$messageProperty.Value) -ForegroundColor Yellow
    }
    Write-Host "Zainstalowany serwer: $($state.server)" -ForegroundColor Gray
    Write-Host "Dostępny serwer:     $(if ($serverComponent) { $serverComponent.version } else { 'brak w tym kanale' })" -ForegroundColor Cyan
    Write-Host "Zainstalowany klient: $($state.client)" -ForegroundColor Gray
    Write-Host "Dostępny klient:      $(if ($clientComponent) { $clientComponent.version } else { 'brak w tym kanale' })" -ForegroundColor Cyan
}

function Update-Server {
    param($RemoteManifest)
    $component = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'server'
    if (-not $component) {
        Write-Host 'Manifest nie zawiera aktualizacji serwera. Pomijam.' -ForegroundColor Yellow
        return
    }
    $state = Read-State
    if (Test-InstalledVersion -Installed ([string]$state.server) -Available ([string]$component.version)) {
        Write-Host "Serwer jest już aktualny (wersja $($component.version))." -ForegroundColor Green
        return
    }
    # A build that failed after the files were swapped leaves them at the new
    # version with only the images missing, and applying the same package
    # again changes nothing but the backups folder: pattsito's five attempts
    # were five downloads of 47.8 MB and five copies of 7042 files, on the
    # drive whose room was the likeliest cause of the failure. Finish the build.
    if ((Test-RebuildPending) -and (Test-InstalledVersion -Installed ([string](Read-RecordedState).server) -Available ([string]$component.version))) {
        Write-Host "Pliki serwera w wersji $($component.version) są już na dysku - dokańczam budowanie bez ponownego pobierania." -ForegroundColor Yellow
        Rebuild-Server
        Write-Host "Serwer działa w wersji $($component.version)." -ForegroundColor Green
        return
    }
    Assert-DockerDiskWritable -Before 'aktualizację (niczego nie pobrano ani nie podmieniono)'
    if (-not (Confirm-Operation 'Zaktualizować pliki serwera i przebudować kontenery? Baza postaci pozostanie bez zmian.')) {
        Write-Host 'Anulowano.' -ForegroundColor Yellow
        return
    }
    $result = Invoke-M2PackageUpdate -Component $component -TargetRoot $serverRoot -BackupRoot (Join-Path $serverRoot 'backups')
    Write-Host "Podmieniono $($result.Files) plików. Kopia: $($result.Backup)" -ForegroundColor Green
    Write-Phase 'Pliki aktualizacji pobrane i podmienione'
    # From here the files on disk are the new version whatever happens to the
    # build, and VERSION on disk already says so. Recording it only after a
    # successful rebuild meant a deferred build left the launcher reporting the
    # previous version for ever - it kept offering the same update and kept
    # re-downloading and re-applying it, one backup directory per attempt. What
    # tracks the build is the rebuild marker, not the version number.
    Save-State -ServerVersion $result.Version -ClientVersion ''
    Rebuild-Server
    Write-Host "Serwer działa w wersji $($result.Version)." -ForegroundColor Green
}

function Assert-ClientNotRunning {
    # The client's exe cannot be replaced while the game runs, and Windows
    # says so only when the file is copied - after the whole download.
    # Ratorex (18 September) tried five times in a quarter of an hour, each
    # time 65 MB and the same "used by another process". Asked first now,
    # and before the server as well, so an "update everything" does not
    # leave a new server beside a client that cannot log in to it.
    param($Config)
    $clientRoot = [string]$Config.clientRoot
    if (-not $clientRoot -or -not (Test-Path -LiteralPath $clientRoot -PathType Container)) { return }
    $running = @(Get-M2FolderProcesses -Root $clientRoot)
    if ($running.Count -gt 0) {
        throw ("Klient gry jest uruchomiony ({0}). Zamknij gre - sprawdz tez Menedzer zadan, czy metin2client.exe nie zostal w tle - i kliknij ZAINSTALUJ AKTUALIZACJE jeszcze raz." -f ($running -join ', '))
    }
}

function Update-Client {
    param($RemoteManifest, $Config)
    $component = Get-ManifestComponent -RemoteManifest $RemoteManifest -Name 'client'
    if (-not $component) {
        Write-Host 'Manifest nie zawiera aktualizacji klienta. Pomijam.' -ForegroundColor Yellow
        return
    }
    $state = Read-State
    if (Test-InstalledVersion -Installed ([string]$state.client) -Available ([string]$component.version)) {
        Write-Host "Klient jest już aktualny (wersja $($component.version))." -ForegroundColor Green
        return
    }
    $clientRoot = [string]$Config.clientRoot
    if (-not $clientRoot) {
        throw 'Nie ustawiono folderu klienta. Uruchom launcher z akcją Configure.'
    }
    if (-not (Test-Path -LiteralPath $clientRoot -PathType Container)) {
        throw "Nie znaleziono folderu klienta: $clientRoot"
    }
    Assert-ClientNotRunning -Config $Config
    if (-not (Confirm-Operation "Zaktualizować klienta w $clientRoot?")) {
        Write-Host 'Anulowano.' -ForegroundColor Yellow
        return
    }
    $result = Invoke-M2PackageUpdate -Component $component -TargetRoot $clientRoot -BackupRoot (Join-Path $serverRoot 'backups\client')
    Save-State -ServerVersion '' -ClientVersion $result.Version
    Write-Host "Klient został zaktualizowany. Plików: $($result.Files), kopia: $($result.Backup)" -ForegroundColor Green
}

function Configure-Launcher {
    $config = Get-Config
    Write-Host 'Pozostaw puste pole, aby zachować dotychczasową wartość.' -ForegroundColor Gray
    $manifestValue = Read-Host "Manifest aktualizacji [$($config.manifestUrl)]"
    if ($manifestValue) { $config.manifestUrl = $manifestValue }
    $clientValue = Read-Host "Folder klienta [$($config.clientRoot)]"
    if ($clientValue) { $config.clientRoot = [IO.Path]::GetFullPath($clientValue) }
    $clientExeValue = Read-Host "Plik EXE klienta [$($config.clientExecutable)]"
    if ($clientExeValue -eq '-') { $config.clientExecutable = '' }
    elseif ($clientExeValue) { $config.clientExecutable = [IO.Path]::GetFullPath($clientExeValue) }
    $supportState = if ($config.supportUploadUrl) { 'ustawiony' } else { 'nieustawiony' }
    $supportValue = Read-Host "Prywatny webhook Discord lub adres HTTPS pomocy [$supportState] (wpisz - aby usunąć)"
    if ($supportValue -eq '-') { $config.supportUploadUrl = '' }
    elseif ($supportValue) { $config.supportUploadUrl = $supportValue }
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    Write-Host "Zapisano konfigurację: $configPath" -ForegroundColor Green
}

function Get-PlayerbotEnvPath {
    return Join-Path $serverRoot 'linux-port\docker\.env'
}

function Get-PlayerbotCount {
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return 350 }
    $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=(\d+)\s*$')
    if ($match.Success) { return [int]$match.Groups[1].Value }
    return 350
}

function Set-PlayerbotCount {
    # Writes PLAYERBOT_AUTOSPAWN_COUNT to .env. The core reads it once at startup
    # and spawns at most this many of the bots it will accept, which is a
    # different and usually smaller number: only characters the canonical seed
    # created are in the registry. A world carrying bots from an older bootstrap
    # keeps them, but they never spawn, so asking for more than the registry
    # holds simply gets the registry. The core says both numbers at startup:
    #   PLAYERBOT_AUTH: loaded <n> registered bot identities
    #   PLAYERBOT: autospawn requested=<x> registered_started=<n>
    #
    # The ceiling is the core's own (2500, input_db.cpp); the seed's canonical
    # cohort holds 1500 a kingdom since 2.2.1, so the number is split equally
    # and never runs short of identities. This clamp is the one that decides -
    # the slider in the GUI only proposes a number, and raising that alone
    # would have written 1500 into .env while showing the player 2500.
    param([Parameter(Mandatory = $true)][int]$Count)
    if ($Count -lt 0) { $Count = 0 }
    if ($Count -gt 2500) { $Count = 2500 }
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        throw "Brak pliku .env: $envPath. Uruchom najpierw serwer (GRAJ), aby go utworzyć."
    }
    $content = [IO.File]::ReadAllText($envPath)
    $pattern = '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=.*$'
    if ([Regex]::IsMatch($content, $pattern)) {
        $content = [Regex]::Replace($content, $pattern, "PLAYERBOT_AUTOSPAWN_COUNT=$Count")
    }
    else {
        if ($content -and -not $content.EndsWith("`n")) { $content += [Environment]::NewLine }
        $content += "PLAYERBOT_AUTOSPAWN_COUNT=$Count" + [Environment]::NewLine
    }
    [IO.File]::WriteAllText($envPath, $content, [Text.UTF8Encoding]::new($false))
    return $Count
}

function Get-SpawnPlanFromEnv {
    # PLAYERBOT_SPAWN_WINDOW_MINUTES / PLAYERBOT_LATE_JOINERS / PLAYERBOT_LATE_JOIN_HOURS
    # as .env has them; 1 / 0 / 24 when the keys are not there yet.
    return @{
        Minutes = Get-DotEnvValue -Key 'PLAYERBOT_SPAWN_WINDOW_MINUTES' -Default '1'
        Late    = Get-DotEnvValue -Key 'PLAYERBOT_LATE_JOINERS' -Default '0'
        Hours   = Get-DotEnvValue -Key 'PLAYERBOT_LATE_JOIN_HOURS' -Default '24'
    }
}

function Set-SpawnPlan {
    # The core reads the three at startup (input_db.cpp): the window the
    # cohort arrives over, the second cohort and its hours. Clamped to what
    # the core accepts, so .env never carries a number it would refuse.
    param([int]$Minutes, [int]$Late, [int]$Hours)
    if ($Minutes -lt 1) { $Minutes = 1 }
    if ($Minutes -gt 180) { $Minutes = 180 }
    if ($Late -lt 0) { $Late = 0 }
    if ($Late -gt 2500) { $Late = 2500 }
    if ($Hours -lt 1) { $Hours = 1 }
    if ($Hours -gt 168) { $Hours = 168 }
    Set-DotEnvValue -Key 'PLAYERBOT_SPAWN_WINDOW_MINUTES' -Value "$Minutes"
    Set-DotEnvValue -Key 'PLAYERBOT_LATE_JOINERS' -Value "$Late"
    Set-DotEnvValue -Key 'PLAYERBOT_LATE_JOIN_HOURS' -Value "$Hours"
    return @{ Minutes = $Minutes; Late = $Late; Hours = $Hours }
}

function Get-KingdomCountsFromEnv {
    # PLAYERBOT_AUTOSPAWN_PER_KINGDOM and the three numbers.
    #
    # A kingdom whose key is not in .env yet defaults to the equal share of
    # PLAYERBOT_AUTOSPAWN_COUNT, which is what the world runs on right now -
    # never to zero. Zero is a real setting that means "this kingdom starts
    # nobody", and offering it as the opening value of a dialog is how a world
    # ends up with bots in one kingdom: the three keys are absent on every
    # install made before 2.0.83, so the box showed 0 for all three, and
    # ticking "Indywidualne wartosci" with one of them filled left the other
    # two empty for good ("nowe postacie tworza sie tylko w Chunjo",
    # NerrVoVy, 19 September - his world had just turned the second channel on
    # and the two were read together).
    $total = 0
    [int]::TryParse((Get-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_COUNT' -Default '0'), [ref]$total) | Out-Null
    $even = [int][Math]::Floor($total / 3)
    $read = {
        param($key)
        $raw = Get-DotEnvValue -Key $key -Default ''
        if ([string]::IsNullOrWhiteSpace([string]$raw)) { return $even }
        $n = 0
        if ([int]::TryParse($raw, [ref]$n)) { return $n }
        return $even
    }
    return @{
        Enabled = (Get-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_PER_KINGDOM' -Default '0') -eq '1'
        Shinsoo = & $read 'PLAYERBOT_AUTOSPAWN_SHINSOO'
        Chunjo  = & $read 'PLAYERBOT_AUTOSPAWN_CHUNJO'
        Jinno   = & $read 'PLAYERBOT_AUTOSPAWN_JINNO'
    }
}

function Set-KingdomCounts {
    # The operator's own number per kingdom (Greess): with it on, each kingdom
    # starts its own count instead of a share of PLAYERBOT_AUTOSPAWN_COUNT, cut
    # by the core to the identities the kingdom has - 1500 since 2.2.1, so a
    # number is clamped there rather than written and quietly cut (kavvaski's
    # 729 Shinsoo came out as the 500 the kingdom held). Read at the next start.
    param([bool]$Enabled, [int]$Shinsoo = 0, [int]$Chunjo = 0, [int]$Jinno = 0)
    $clamp = { param($n) if ($n -lt 0) { 0 } elseif ($n -gt 1500) { 1500 } else { $n } }
    $Shinsoo = & $clamp $Shinsoo
    $Chunjo = & $clamp $Chunjo
    $Jinno = & $clamp $Jinno
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_PER_KINGDOM' -Value $(if ($Enabled) { '1' } else { '0' })
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_SHINSOO' -Value "$Shinsoo"
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_CHUNJO' -Value "$Chunjo"
    Set-DotEnvValue -Key 'PLAYERBOT_AUTOSPAWN_JINNO' -Value "$Jinno"
    return @{ Enabled = $Enabled; Shinsoo = $Shinsoo; Chunjo = $Chunjo; Jinno = $Jinno }
}

function Get-SecondChannelFromEnv {
    $share = 40
    [int]::TryParse((Get-DotEnvValue -Key 'PLAYERBOT_CH2_SHARE' -Default '40'), [ref]$share) | Out-Null
    return @{ Enabled = (Get-DotEnvValue -Key 'M2_PLAYERBOT_CH2' -Default '0') -eq '1'; Share = $share }
}

function Set-SecondChannel {
    # The second channel (M2_PLAYERBOT_CH2): the switch, the share of the bots
    # that play on it, and the two port ranges compose publishes - 13000-13012
    # while it is on (its cores listen on 13010-13012), the first channel's
    # three otherwise. The host side keeps the first port a player may have
    # moved. SetAt is when the choice was made: the game container compares it
    # with the web panel's wish, and the newer of the two wins.
    param([bool]$Enabled, [int]$Share = 40, [long]$SetAt = 0)
    if ($Share -lt 10) { $Share = 10 }
    if ($Share -gt 90) { $Share = 90 }
    if ($SetAt -le 0) { $SetAt = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() }
    $first = 13000
    $range = Get-DotEnvValue -Key 'M2_GAME_PORT_RANGE' -Default '13000-13002'
    if ($range -match '^\s*(\d+)') { $first = [int]$Matches[1] }
    $span = if ($Enabled) { 12 } else { 2 }
    Set-DotEnvValue -Key 'M2_PLAYERBOT_CH2' -Value $(if ($Enabled) { '1' } else { '0' })
    Set-DotEnvValue -Key 'PLAYERBOT_CH2_SHARE' -Value "$Share"
    Set-DotEnvValue -Key 'M2_PLAYERBOT_CH2_SET_AT' -Value "$SetAt"
    Set-DotEnvValue -Key 'M2_GAME_PORT_RANGE' -Value ('{0}-{1}' -f $first, ($first + $span))
    Set-DotEnvValue -Key 'M2_GAME_CONTAINER_PORT_RANGE' -Value ('13000-{0}' -f (13000 + $span))
    return @{ Enabled = $Enabled; Share = $Share }
}

function Sync-ChannelWishFromPanel {
    # The web panel cannot write .env; it leaves its second-channel wish in the
    # spool the game container reads (channels.wanted, with SET_AT). The
    # container honours it for the bots at its next start whatever happens
    # here, but only .env can publish the second channel's ports - so a wish
    # newer than .env's own is copied into .env before the stack comes up.
    # Only while the game container runs: its spool cannot be read otherwise.
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return }
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $text = @(docker compose --project-directory $composeDir -f $composeFile exec -T game cat /opt/m2spool/channels.wanted 2>$null)
        $exit = $LASTEXITCODE
    }
    catch { return }
    finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0 -or $text.Count -eq 0) { return }
    $wish = @{}
    foreach ($line in $text) {
        if ("$line" -match '^\s*([A-Z0-9_]+)=(\d+)\s*$') { $wish[$Matches[1]] = [long]$Matches[2] }
    }
    if (-not $wish.ContainsKey('CH2') -or -not $wish.ContainsKey('SET_AT')) { return }
    $envAt = 0L
    [long]::TryParse((Get-DotEnvValue -Key 'M2_PLAYERBOT_CH2_SET_AT' -Default '0'), [ref]$envAt) | Out-Null
    if ($wish['SET_AT'] -le $envAt) { return }
    $share = if ($wish.ContainsKey('SHARE')) { [int]$wish['SHARE'] } else { 40 }
    $applied = Set-SecondChannel -Enabled ($wish['CH2'] -eq 1) -Share $share -SetAt $wish['SET_AT']
    $what = if ($applied.Enabled) { "wlaczony, $($applied.Share)% botow na CH2" } else { 'wylaczony' }
    Write-Host "Drugi kanal ustawiony w panelu WWW: $what." -ForegroundColor Green
}

function Set-BotCountAction {
    $current = Get-PlayerbotCount
    $plan = Get-SpawnPlanFromEnv
    Write-Host "Aktualnie gra: $current botów (efektywny limit = liczba botów w Twoim świecie; kanoniczna paczka ma 350)." -ForegroundColor Gray
    Write-Host "Wchodzą w ciągu $($plan.Minutes) min od startu; dodatkowych botów dołączających stopniowo: $($plan.Late) w ciągu $($plan.Hours) h." -ForegroundColor Gray

    # -BotCount passed (from the GUI or scripting) is non-interactive: never call
    # Read-Host, because the GUI runs this in a hidden, non-interactive console.
    # Restart only when -Yes is also given. Without -BotCount we are in the text
    # menu and can prompt for the numbers and the restart.
    if ($BotCount -ge 0) {
        $applied = Set-PlayerbotCount -Count $BotCount
        Write-Host "Zapisano: $applied grających botów." -ForegroundColor Green
        if ($SpawnMinutes -ge 0 -or $LateJoiners -ge 0 -or $LateHours -ge 0) {
            $m = if ($SpawnMinutes -ge 0) { $SpawnMinutes } else { [int]$plan.Minutes }
            $l = if ($LateJoiners -ge 0) { $LateJoiners } else { [int]$plan.Late }
            $h = if ($LateHours -ge 0) { $LateHours } else { [int]$plan.Hours }
            $p = Set-SpawnPlan -Minutes $m -Late $l -Hours $h
            Write-Host "Zapisano: wejście w $($p.Minutes) min, $($p.Late) dodatkowych botów w ciągu $($p.Hours) h." -ForegroundColor Green
        }
        if ($PerKingdom -ge 0) {
            $k = Get-KingdomCountsFromEnv
            $s = if ($ShinsooBots -ge 0) { $ShinsooBots } else { $k.Shinsoo }
            $c = if ($ChunjoBots -ge 0) { $ChunjoBots } else { $k.Chunjo }
            $j = if ($JinnoBots -ge 0) { $JinnoBots } else { $k.Jinno }
            $kk = Set-KingdomCounts -Enabled ($PerKingdom -eq 1) -Shinsoo $s -Chunjo $c -Jinno $j
            if ($kk.Enabled) {
                Write-Host "Zapisano: osobno dla królestw - Shinsoo $($kk.Shinsoo), Chunjo $($kk.Chunjo), Jinno $($kk.Jinno)." -ForegroundColor Green
                # A kingdom at zero starts nobody, and nothing in the game says
                # so afterwards - the world simply has no bots there. It is a
                # legitimate setting, so it is said out loud rather than
                # refused.
                $empty = @()
                if ($kk.Shinsoo -le 0) { $empty += 'Shinsoo' }
                if ($kk.Chunjo -le 0) { $empty += 'Chunjo' }
                if ($kk.Jinno -le 0) { $empty += 'Jinno' }
                if (@($empty).Count -gt 0) {
                    Write-Host ("UWAGA: " + ($empty -join ' i ') + " nie wystartuje zadnego bota. Wpisz tam liczbe wieksza od zera albo wylacz indywidualne wartosci.") -ForegroundColor Yellow
                }
            }
            else { Write-Host 'Zapisano: jedna liczba botów dzielona po równo na królestwa.' -ForegroundColor Green }
        }
        if ($Channel2 -ge 0) {
            # Written only when it changes, so the moment of the choice stays the
            # one it was made at and a wish from the web panel made after it is
            # not overwritten by a dialog that only changed the bot count.
            $cur = Get-SecondChannelFromEnv
            $share = if ($Channel2Share -ge 0) { $Channel2Share } else { $cur.Share }
            if (($Channel2 -eq 1) -ne $cur.Enabled -or (($Channel2 -eq 1) -and $share -ne $cur.Share)) {
                $ch = Set-SecondChannel -Enabled ($Channel2 -eq 1) -Share $share
                if ($ch.Enabled) { Write-Host "Zapisano: drugi kanał (CH2) włączony, $($ch.Share)% botów na CH2." -ForegroundColor Green }
                else { Write-Host 'Zapisano: drugi kanał (CH2) wyłączony.' -ForegroundColor Green }
            }
        }
        if ($Yes) {
            Start-Server
            Write-Host "Serwer zrestartowany z liczbą botów: $applied." -ForegroundColor Green
        }
        else {
            Write-Host 'Zmiana zostanie zastosowana przy następnym starcie serwera.' -ForegroundColor Yellow
        }
        return
    }

    $answer = Read-Host 'Ilu botów ma grać (0-2500)'
    if ($answer -notmatch '^\d+$') { Write-Host 'Anulowano: to nie jest liczba.' -ForegroundColor Yellow; return }
    $applied = Set-PlayerbotCount -Count ([int]$answer)
    Write-Host "Zapisano: $applied grających botów." -ForegroundColor Green
    $m = Read-Host "W ciągu ilu minut od startu mają wejść (1-180, Enter = $($plan.Minutes))"
    $l = Read-Host "Ilu dodatkowych botów ma dołączać stopniowo później (0-2500, Enter = $($plan.Late))"
    $h = Read-Host "W ciągu ilu godzin mają dołączać (1-168, Enter = $($plan.Hours))"
    if (-not "$m".Trim()) { $m = $plan.Minutes }
    if (-not "$l".Trim()) { $l = $plan.Late }
    if (-not "$h".Trim()) { $h = $plan.Hours }
    if ("$m" -notmatch '^\d+$' -or "$l" -notmatch '^\d+$' -or "$h" -notmatch '^\d+$') {
        Write-Host 'Plan wejścia bez zmian: to nie są liczby.' -ForegroundColor Yellow
    }
    else {
        $p = Set-SpawnPlan -Minutes ([int]$m) -Late ([int]$l) -Hours ([int]$h)
        Write-Host "Zapisano: wejście w $($p.Minutes) min, $($p.Late) dodatkowych botów w ciągu $($p.Hours) h." -ForegroundColor Green
    }
    $k = Get-KingdomCountsFromEnv
    $kAnswer = Read-Host "Osobna liczba botów dla każdego królestwa? (t/n, Enter = $(if ($k.Enabled) { 't' } else { 'n' }))"
    if ("$kAnswer".Trim() -match '^[tTyY]') {
        $sAnswer = Read-Host "Shinsoo, czerwone (0-1500, Enter = $($k.Shinsoo))"
        $cAnswer = Read-Host "Chunjo, żółte (0-1500, Enter = $($k.Chunjo))"
        $jAnswer = Read-Host "Jinno, niebieskie (0-1500, Enter = $($k.Jinno))"
        $s = if ("$sAnswer".Trim() -match '^\d+$') { [int]$sAnswer } else { $k.Shinsoo }
        $c = if ("$cAnswer".Trim() -match '^\d+$') { [int]$cAnswer } else { $k.Chunjo }
        $j = if ("$jAnswer".Trim() -match '^\d+$') { [int]$jAnswer } else { $k.Jinno }
        $kk = Set-KingdomCounts -Enabled $true -Shinsoo $s -Chunjo $c -Jinno $j
        Write-Host "Zapisano: Shinsoo $($kk.Shinsoo), Chunjo $($kk.Chunjo), Jinno $($kk.Jinno)." -ForegroundColor Green
    }
    elseif ("$kAnswer".Trim() -match '^[nN]') {
        Set-KingdomCounts -Enabled $false -Shinsoo $k.Shinsoo -Chunjo $k.Chunjo -Jinno $k.Jinno | Out-Null
        Write-Host 'Zapisano: jedna liczba botów dzielona po równo na królestwa.' -ForegroundColor Green
    }
    $ch2 = Get-SecondChannelFromEnv
    $chAnswer = Read-Host "Drugi kanał (CH2) dla botów i graczy? Sklepy zostają na CH1 (t/n, Enter = $(if ($ch2.Enabled) { 't' } else { 'n' }))"
    if ("$chAnswer".Trim() -match '^[tTyY]') {
        $shAnswer = Read-Host "Ile procent botów na CH2 (10-90, Enter = $($ch2.Share))"
        $share = if ("$shAnswer".Trim() -match '^\d+$') { [int]$shAnswer } else { $ch2.Share }
        $applied2 = Set-SecondChannel -Enabled $true -Share $share
        Write-Host "Zapisano: drugi kanał włączony, $($applied2.Share)% botów na CH2." -ForegroundColor Green
    }
    elseif ("$chAnswer".Trim() -match '^[nN]' -and $ch2.Enabled) {
        Set-SecondChannel -Enabled $false -Share $ch2.Share | Out-Null
        Write-Host 'Zapisano: drugi kanał wyłączony.' -ForegroundColor Green
    }
    if (Confirm-Operation 'Zrestartować serwer teraz, aby zastosować zmianę? Baza i postęp botów pozostają bez zmian') {
        Start-Server
        Write-Host "Serwer zrestartowany z liczbą botów: $applied." -ForegroundColor Green
    }
    else {
        Write-Host 'Zmiana zostanie zastosowana przy następnym starcie serwera.' -ForegroundColor Yellow
    }
}

# The world's difficulty: how long a player waits at the Biologist between two
# hand-ins and at the stable keeper (the pony, each Horse Book, the medal
# trainings). M2_DIFFICULTY in .env - easy, medium, hard or custom with the two
# hour counts - is turned into event flags by the migrate service at every
# start and read by the quests (linux-port-mt2009/docker/game/quest/
# m2_difficulty.lua), so a change needs a restart. The bots never waited.
# The skill books' waits are the package's twenty-one hours on hard and a
# third of them on medium, for the players and the bots alike; the migrator's
# presets (apply.sh) carry the same numbers in seconds.
$script:DifficultyPresets = @{
    easy   = @{ Biologist = '0';  Horse = '0';  Book = '0';  BotBook = '0' }
    medium = @{ Biologist = '8';  Horse = '4';  Book = '7';  BotBook = '7' }
    hard   = @{ Biologist = '24'; Horse = '12'; Book = '21'; BotBook = '21' }
}

function Get-DotEnvValue {
    param([Parameter(Mandatory = $true)][string]$Key, [string]$Default = '')
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return $Default }
    $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^' + [Regex]::Escape($Key) + '=(.*?)\s*$')
    if ($match.Success) { return $match.Groups[1].Value.Trim() }
    return $Default
}

function Set-DotEnvValue {
    # One key of .env replaced in place or appended; nothing else in the file -
    # the player's own passwords included - is touched. Same shape as
    # Set-PlayerbotCount. The value is a literal: a $ in it must not become a
    # group reference for Regex.Replace.
    param([Parameter(Mandatory = $true)][string]$Key, [Parameter(Mandatory = $true)][string]$Value)
    $envPath = Get-PlayerbotEnvPath
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        throw "Brak pliku .env: $envPath. Uruchom najpierw serwer (GRAJ), aby go utworzyć."
    }
    $content = [IO.File]::ReadAllText($envPath)
    $pattern = '(?m)^' + [Regex]::Escape($Key) + '=.*$'
    $line = "$Key=$Value"
    if ([Regex]::IsMatch($content, $pattern)) {
        $content = [Regex]::Replace($content, $pattern, $line.Replace('$', '$$'))
    }
    else {
        if ($content -and -not $content.EndsWith("`n")) { $content += [Environment]::NewLine }
        $content += $line + [Environment]::NewLine
    }
    [IO.File]::WriteAllText($envPath, $content, [Text.UTF8Encoding]::new($false))
}

function Test-DifficultyHours {
    param([string]$Text)
    $n = 0.0
    $ok = [double]::TryParse("$Text".Trim().Replace(',', '.'), [Globalization.NumberStyles]::Float,
        [Globalization.CultureInfo]::InvariantCulture, [ref]$n)
    return ($ok -and $n -ge 0 -and $n -le 720)
}

function Set-DifficultyAction {
    $current = Get-DotEnvValue -Key 'M2_DIFFICULTY' -Default 'easy'
    $currentBio = Get-DotEnvValue -Key 'M2_BIOLOGIST_WAIT_HOURS' -Default '0'
    $currentHorse = Get-DotEnvValue -Key 'M2_HORSE_WAIT_HOURS' -Default '0'
    $currentBook = Get-DotEnvValue -Key 'M2_BOOK_WAIT_HOURS' -Default '0'
    $currentBotBook = Get-DotEnvValue -Key 'M2_BOT_BOOK_WAIT_HOURS' -Default '0'
    Write-Host "Aktualny poziom trudności: $current (przy 'custom': Biolog $currentBio h, Stajenny $currentHorse h, księgi: gracze $currentBook h, boty $currentBotBook h)." -ForegroundColor Gray

    # -Difficulty passed (from the GUI or scripting) is non-interactive, like
    # -BotCount: never Read-Host, restart only with -Yes.
    $level = "$Difficulty".Trim().ToLowerInvariant()
    $bio = "$BiologistHours"
    $horse = "$HorseHours"
    $book = "$BookHours"
    $botBook = "$BotBookHours"
    $interactive = (-not $level)
    if ($interactive) {
        Write-Host ' 1. easy   - bez czekania u Biologa, u Stajennego i na kolejną księgę (tak jak dotąd)'
        Write-Host ' 2. medium - Biolog 8 h; kucyk i Księgi Konia 4 h; treningi konia 6 h (1-10) i 7 h (11-19); księgi 7 h'
        Write-Host ' 3. hard   - jak w oryginale: Biolog 24 h; kucyk i Księgi 12 h; treningi 18 h i 21 h; księgi 21 h'
        Write-Host ' 4. custom - własne godziny (Biolog, każde czekanie u Stajennego, księgi graczy i księgi botów)'
        $answer = Read-Host 'Wybierz poziom (1-4)'
        $level = switch ($answer) { '1' { 'easy' } '2' { 'medium' } '3' { 'hard' } '4' { 'custom' } default { '' } }
        if (-not $level) { Write-Host 'Anulowano.' -ForegroundColor Yellow; return }
        if ($level -eq 'custom') {
            $bio = Read-Host 'Ile godzin czeka się u Biologa między oddaniami (0 = bez czekania, ułamki dozwolone)'
            $horse = Read-Host 'Ile godzin czeka się u Stajennego na kucyka, Księgę Konia i trening (0 = bez czekania)'
            $book = Read-Host 'Ile godzin gracz czeka między dwiema księgami tej samej umiejętności (0 = od razu)'
            $botBook = Read-Host 'Ile godzin czekają na kolejną księgę boty (0 = od razu)'
        }
    }
    if ($level -notin @('easy', 'medium', 'hard', 'custom')) {
        throw "Nieznany poziom trudności: '$level'. Dozwolone: easy, medium, hard, custom."
    }
    if ($level -ne 'custom') {
        $bio = $script:DifficultyPresets[$level].Biologist
        $horse = $script:DifficultyPresets[$level].Horse
        $book = $script:DifficultyPresets[$level].Book
        $botBook = $script:DifficultyPresets[$level].BotBook
    }
    # An older GUI passes no book hours for custom: what .env already says.
    if ("$book".Trim() -eq '') { $book = $currentBook }
    if ("$botBook".Trim() -eq '') { $botBook = $currentBotBook }
    if (-not (Test-DifficultyHours $bio)) { throw "Godziny u Biologa: podaj liczbę od 0 do 720 (np. 12 albo 0.5), nie '$bio'." }
    if (-not (Test-DifficultyHours $horse)) { throw "Godziny u Stajennego: podaj liczbę od 0 do 720 (np. 12 albo 0.5), nie '$horse'." }
    if (-not (Test-DifficultyHours $book)) { throw "Godziny między księgami graczy: podaj liczbę od 0 do 720 (np. 21 albo 0.5), nie '$book'." }
    if (-not (Test-DifficultyHours $botBook)) { throw "Godziny między księgami botów: podaj liczbę od 0 do 720 (np. 21 albo 0.5), nie '$botBook'." }
    $bio = "$bio".Trim().Replace(',', '.')
    $horse = "$horse".Trim().Replace(',', '.')
    $book = "$book".Trim().Replace(',', '.')
    $botBook = "$botBook".Trim().Replace(',', '.')
    Set-DotEnvValue -Key 'M2_DIFFICULTY' -Value $level
    Set-DotEnvValue -Key 'M2_BIOLOGIST_WAIT_HOURS' -Value $bio
    Set-DotEnvValue -Key 'M2_HORSE_WAIT_HOURS' -Value $horse
    Set-DotEnvValue -Key 'M2_BOOK_WAIT_HOURS' -Value $book
    Set-DotEnvValue -Key 'M2_BOT_BOOK_WAIT_HOURS' -Value $botBook
    Write-Host "Zapisano: poziom trudności $level (Biolog $bio h, Stajenny $horse h, księgi: gracze $book h, boty $botBook h)." -ForegroundColor Green
    if ($Yes) {
        Start-Server
        Write-Host "Serwer zrestartowany z poziomem trudności: $level." -ForegroundColor Green
        return
    }
    if ($interactive -and (Confirm-Operation 'Zrestartować serwer teraz, aby zastosować zmianę? Baza i postęp botów pozostają bez zmian')) {
        Start-Server
        Write-Host "Serwer zrestartowany z poziomem trudności: $level." -ForegroundColor Green
        return
    }
    Write-Host 'Zmiana zostanie zastosowana przy następnym starcie serwera.' -ForegroundColor Yellow
}

function Get-CurrentInstallTargetVolume {
    # The db-data volume of THIS installation (import target).
    $statePath = Join-Path $serverRoot '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $volProp = $state.PSObject.Properties['databaseVolume']
            if ($volProp -and [string]$volProp.Value) { return [string]$volProp.Value }
            if ([string]$state.projectName) { return "$([string]$state.projectName)_db-data" }
        }
        catch { }
    }
    $envPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$')
        if ($match.Success) { return "$($match.Groups[1].Value)_db-data" }
    }
    return $null
}

function Import-DatabaseAction {
    if (-not (Test-M2DockerRunning)) {
        Write-Host 'Silnik Dockera jest zatrzymany, więc nie widać żadnych baz.' -ForegroundColor Yellow
        Write-Host 'Uruchom Docker (akcja StartDocker lub przycisk „URUCHOM DOCKER") i spróbuj ponownie.' -ForegroundColor Yellow
        Write-Host 'Żadne dane nie zginęły — bazy są na dysku, tylko Docker ich teraz nie pokazuje.' -ForegroundColor Gray
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host 'Nie można ustalić bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choć raz, aby utworzyć tożsamość i wolumen.' -ForegroundColor Yellow
        return
    }
    Write-Host "Baza docelowa (ta instalacja): $target" -ForegroundColor Gray
    if (-not (Test-M2VolumeInitialized -Volume $target)) {
        Write-Host 'Ta instalacja nie ma jeszcze gotowej bazy danych.' -ForegroundColor Yellow
        Write-Host 'Najpierw kliknij GRAJ i pozwól serwerowi wystartować choć raz (utworzy bazę ze schematami gry),' -ForegroundColor Yellow
        Write-Host 'a dopiero potem importuj świat. Import na pustą bazę zostawiłby instalację bez schematów.' -ForegroundColor Yellow
        return
    }
    $sources = @(Get-M2DbDataVolumes | Where-Object { $_.Name -ne $target })
    if ($sources.Count -eq 0) {
        Write-Host 'Nie znaleziono innej bazy Docker do importu na tym komputerze.' -ForegroundColor Yellow
        return
    }

    $chosen = $null
    if ($ImportSource) {
        $chosen = $sources | Where-Object { $_.Name -eq $ImportSource -or $_.Project -eq $ImportSource } | Select-Object -First 1
        if (-not $chosen) { Write-Host "Nie znaleziono źródła do importu: $ImportSource" -ForegroundColor Red; return }
    }
    else {
        Write-Host 'Dostępne bazy do importu:' -ForegroundColor Cyan
        for ($i = 0; $i -lt $sources.Count; $i++) {
            $label = $sources[$i].Project
            if ($sources[$i].CreatedAt) { $label = '{0}   (utworzona {1:yyyy-MM-dd HH:mm})' -f $label, $sources[$i].CreatedAt }
            Write-Host ("  [{0}] {1}" -f ($i + 1), $label)
        }
        $pick = Read-Host 'Wybierz numer źródła (Enter = anuluj)'
        if ($pick -notmatch '^\d+$') { Write-Host 'Anulowano.' -ForegroundColor Yellow; return }
        $idx = [int]$pick - 1
        if ($idx -lt 0 -or $idx -ge $sources.Count) { Write-Host 'Nieprawidłowy numer.' -ForegroundColor Yellow; return }
        $chosen = $sources[$idx]
    }

    Write-Host "Sprawdzam świat źródłowy '$($chosen.Project)'..." -ForegroundColor Gray
    $stats = Get-M2VolumeWorldStats -Volume $chosen.Name
    if ($stats.Ok) {
        Write-Host ("Źródło: {0} postaci, najwyższy poziom {1}." -f $stats.Players, $stats.MaxLevel) -ForegroundColor Green
        if ($stats.Created) { Write-Host ("  Baza utworzona: {0}" -f $stats.Created) -ForegroundColor Gray }
        if ($stats.LastPlay -and $stats.LastPlay -ne '0') { Write-Host ("  Ostatnia gra: {0}" -f $stats.LastPlay) -ForegroundColor Gray }
    }
    else {
        Write-Host 'Nie udało się odczytać statystyk źródła (mimo to można spróbować importu).' -ForegroundColor Yellow
    }

    Write-Host ''
    Write-Host "UWAGA: import ZASTĄPI obecny świat tej instalacji światem ze źródła '$($chosen.Project)'." -ForegroundColor Yellow
    Write-Host "Źródło pozostaje nietknięte. Obecny świat trafi do kopii w 'backups' przed nadpisaniem." -ForegroundColor Yellow
    if (-not (Confirm-Operation "Kontynuować import z '$($chosen.Project)'?")) { Write-Host 'Anulowano.' -ForegroundColor Yellow; return }

    # Read this install's game DB user/password so the import can re-apply the
    # user and grants afterwards (guards against the migrator failing to
    # authenticate after a swap).
    $dbUser = 'metin2'; $dbPass = ''
    $importEnvPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $importEnvPath -PathType Leaf) {
        $importEnvText = [IO.File]::ReadAllText($importEnvPath)
        $userMatch = [Regex]::Match($importEnvText, '(?m)^M2_DB_USER=(.+?)\s*$')
        if ($userMatch.Success) { $dbUser = $userMatch.Groups[1].Value }
        $passMatch = [Regex]::Match($importEnvText, '(?m)^M2_DB_PASSWORD=(.+?)\s*$')
        if ($passMatch.Success) { $dbPass = $passMatch.Groups[1].Value }
    }

    Write-Host 'Zatrzymuję serwer, aby zwolnić bazę docelową...' -ForegroundColor Cyan
    Stop-Server

    Write-Host 'Importuję bazę (to może potrwać chwilę)...' -ForegroundColor Cyan
    $result = Invoke-M2DatabaseImport -SourceVolume $chosen.Name -TargetVolume $target -BackupRoot (Join-Path $serverRoot 'backups') -DbUser $dbUser -DbPassword $dbPass
    Write-Host ("Gotowe. Zaimportowany świat: {0} postaci, najwyższy poziom {1}." -f $result.Players, $result.MaxLevel) -ForegroundColor Green
    Write-Host ("Kopia poprzedniego świata: {0}" -f $result.Backup) -ForegroundColor Gray
    Write-Host 'Kliknij GRAJ (lub akcja Start), aby uruchomić serwer z zaimportowanym światem.' -ForegroundColor Green
}

function Backup-DatabaseAction {
    # Everything the import path already did to protect a world, asked for on
    # purpose instead of as a side effect: five SQL dumps, a manifest and a zip.
    if (-not (Test-M2DockerRunning)) {
        Write-Host 'Silnik Dockera jest zatrzymany, wiec nie da sie odczytac bazy.' -ForegroundColor Yellow
        Write-Host 'Uruchom Docker (akcja StartDocker) i sprobuj ponownie. Nic nie zginelo.' -ForegroundColor Gray
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host 'Nie mozna ustalic bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choc raz.' -ForegroundColor Yellow
        return
    }
    if (-not (Test-M2VolumeInitialized -Volume $target)) {
        Write-Host 'Ta instalacja nie ma jeszcze bazy danych - nie ma czego zapisac.' -ForegroundColor Yellow
        return
    }
    Write-Host 'Zatrzymuję serwer, aby baza była spójna w chwili zapisu...' -ForegroundColor Cyan
    Stop-Server
    Write-Host 'Zapisuję kopię (to może potrwać chwilę)...' -ForegroundColor Cyan
    $result = New-M2DatabaseBackup -Volume $target -BackupRoot (Join-Path $serverRoot 'backups')
    Write-Host ("Gotowe. Zapisany świat: {0} postaci, najwyższy poziom {1}." -f $result.Players, $result.MaxLevel) -ForegroundColor Green
    Write-Host ("  Folder: {0}" -f $result.Folder) -ForegroundColor Gray
    Write-Host ("  Plik:   {0}  ({1:N0} MB)" -f $result.Zip, ($result.ZipBytes / 1MB)) -ForegroundColor Gray
    Write-Host 'Ten jeden plik zip wystarczy, aby odtworzyć świat na tym albo na innym komputerze.' -ForegroundColor Gray
    Write-Host 'Kliknij GRAJ, aby uruchomić serwer z powrotem.' -ForegroundColor Green
}

function Restore-DatabaseAction {
    if (-not (Test-M2DockerRunning)) {
        Write-Host 'Silnik Dockera jest zatrzymany. Uruchom Docker i spróbuj ponownie.' -ForegroundColor Yellow
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host 'Nie mozna ustalic bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choc raz.' -ForegroundColor Yellow
        return
    }
    $picked = $RestoreSource
    if (-not $picked) {
        $backupRoot = Join-Path $serverRoot 'backups'
        $found = @()
        if (Test-Path -LiteralPath $backupRoot -PathType Container) {
            $found = @(Get-ChildItem -LiteralPath $backupRoot -Filter 'db-backup-*.zip' -File |
                       Sort-Object LastWriteTime -Descending)
        }
        if ($found.Count -eq 0) {
            Write-Host "Nie znaleziono zadnej kopii w '$backupRoot'." -ForegroundColor Yellow
            Write-Host 'Zrob najpierw kopie (akcja BackupDb), albo podaj sciezke: -RestoreSource "C:\...\db-backup-....zip"' -ForegroundColor Gray
            return
        }
        Write-Host 'Dostępne kopie:' -ForegroundColor Cyan
        for ($i = 0; $i -lt $found.Count; $i++) {
            Write-Host ("  [{0}] {1}   ({2:yyyy-MM-dd HH:mm}, {3:N0} MB)" -f ($i + 1),
                $found[$i].Name, $found[$i].LastWriteTime, ($found[$i].Length / 1MB))
        }
        $pick = Read-Host 'Wybierz numer kopii (Enter = anuluj)'
        if ($pick -notmatch '^\d+$') { Write-Host 'Anulowano.' -ForegroundColor Yellow; return }
        $idx = [int]$pick - 1
        if ($idx -lt 0 -or $idx -ge $found.Count) { Write-Host 'Nieprawidłowy numer.' -ForegroundColor Yellow; return }
        $picked = $found[$idx].FullName
    }
    if (-not (Test-Path -LiteralPath $picked)) {
        Write-Host "Nie znaleziono kopii: $picked" -ForegroundColor Red
        return
    }
    Write-Host ''
    Write-Host "UWAGA: przywrócenie ZASTĄPI obecny świat tej instalacji zawartością kopii." -ForegroundColor Yellow
    Write-Host 'Obecny świat zostanie najpierw zapisany do własnej kopii w folderze backups.' -ForegroundColor Yellow
    if (-not (Confirm-Operation "Przywrócić świat z '$([IO.Path]::GetFileName($picked))'?")) {
        Write-Host 'Anulowano.' -ForegroundColor Yellow; return
    }
    $creds = Get-InstallDbCredentials
    Write-Host 'Zatrzymuję serwer, aby zwolnić bazę...' -ForegroundColor Cyan
    Stop-Server
    Write-Host 'Przywracam kopię (to może potrwać chwilę)...' -ForegroundColor Cyan
    $result = Restore-M2DatabaseBackup -BackupPath $picked -TargetVolume $target `
        -BackupRoot (Join-Path $serverRoot 'backups') -DbUser $creds.User -DbPassword $creds.Password
    Write-Host ("Gotowe. Przywrócony świat: {0} postaci, najwyższy poziom {1}." -f $result.Players, $result.MaxLevel) -ForegroundColor Green
    Write-Host ("  Kopia poprzedniego świata: {0}" -f $result.Safety) -ForegroundColor Gray
    Write-Host 'Kliknij GRAJ, aby uruchomić serwer z przywróconym światem.' -ForegroundColor Green
}

function Test-RatePercent {
    param([int]$Value)
    return ($Value -ge 1 -and $Value -le 10000)
}

function Set-FreshWorldSettings {
    <#
      .SYNOPSIS
        The rates a world about to be made starts on, and whether its bots wait.

      .DESCRIPTION
        Both are read by the migrator before the cores come up, and only for a
        world whose event flags do not exist yet - a world already set from the
        panel is never touched by .env, so this is asked where a fresh world is
        about to be made and nowhere else.

        The panel used to promise 650% experience on a world the game ran at
        100%, and the first press of its button - field untouched - was what
        made the promise real. That is the window NerrVoVy asked us to close
        (20 September): "zanim sie zmieni ustawienia to juz cos sie tam
        podzieje".

        Non-interactive when the numbers come in as parameters or -Yes is set,
        which is how the GUI calls every action; the console path asks.
    #>
    param([string]$Reason = 'nowego świata')

    $exp = $RateExp
    $drop = $RateDrop
    $yang = $RateYang
    $hold = $HoldBots
    $starter = $StarterChest
    $interactive = (-not $Yes) -and $exp -lt 0 -and $drop -lt 0 -and $yang -lt 0 -and $hold -lt 0 -and $starter -lt 0
    if ($interactive) {
        Write-Host ''
        Write-Host "Ustawienia $Reason - wchodzą w życie, zanim pojawi się pierwszy bot:" -ForegroundColor Cyan
        Write-Host ' 1. Normalnie      - 100% doświadczenia, 100% dropu, 100% yang (tak, jak gra została stworzona)'
        Write-Host ' 2. Spokojnie      - 300% / 200% / 200%'
        Write-Host ' 3. Szybko         - 1000% / 500% / 500%'
        Write-Host ' 4. Własne liczby'
        Write-Host ' 5. Nie zmieniaj   - zostaw to, co jest w .env'
        $answer = Read-Host 'Wybierz (1-5)'
        switch ($answer) {
            '1' { $exp = 100;  $drop = 100; $yang = 100 }
            '2' { $exp = 300;  $drop = 200; $yang = 200 }
            '3' { $exp = 1000; $drop = 500; $yang = 500 }
            '4' {
                $exp = [int](Read-Host 'Doświadczenie w procentach (100 = normalnie)')
                $drop = [int](Read-Host 'Drop przedmiotów w procentach')
                $yang = [int](Read-Host 'Yang w procentach')
            }
            default { $exp = -1; $drop = -1; $yang = -1 }
        }
        Write-Host ''
        Write-Host 'Boty mogą poczekać przy drzwiach, żeby dało się spokojnie ustawić resztę:' -ForegroundColor Cyan
        if (Confirm-Operation 'Wstrzymać boty po starcie (wpuścisz je przyciskiem w panelu)?') {
            $hold = 1
        }
        else {
            $hold = 0
        }
        Write-Host ''
        Write-Host 'Skrzynia Ucznia to zestaw skrzyń, który prowadzi postać przez pierwsze wioski (boty też je mają):' -ForegroundColor Cyan
        if (Confirm-Operation 'Czy nowe postacie graczy mają dostawać Skrzynię Ucznia przy pierwszym logowaniu?') {
            $starter = 1
        }
        else {
            $starter = 0
        }
    }

    $written = @()
    foreach ($pair in @(
            @{ Key = 'M2_RATE_EXP';  Value = $exp;  Label = 'doświadczenie' },
            @{ Key = 'M2_RATE_DROP'; Value = $drop; Label = 'drop' },
            @{ Key = 'M2_RATE_YANG'; Value = $yang; Label = 'yang' })) {
        $v = [int]$pair.Value
        if ($v -lt 0) { continue }
        if (-not (Test-RatePercent -Value $v)) {
            throw ("{0}: podaj całe procenty od 1 do 10000, nie '{1}'." -f $pair.Label, $v)
        }
        Set-DotEnvValue -Key $pair.Key -Value "$v"
        $written += ('{0} {1}%' -f $pair.Label, $v)
    }
    if ($hold -ge 0) {
        $heldValue = $(if ($hold -ge 1) { '1' } else { '0' })
        Set-DotEnvValue -Key 'M2_PLAYERBOT_START_HELD' -Value $heldValue
        $written += $(if ($heldValue -eq '1') { 'boty czekają na wpuszczenie' } else { 'boty wchodzą od razu' })
    }
    if ($starter -ge 0) {
        $starterValue = $(if ($starter -ge 1) { '1' } else { '0' })
        Set-DotEnvValue -Key 'M2_STARTER_CHEST' -Value $starterValue
        $written += $(if ($starterValue -eq '1') { 'nowe postacie dostają Skrzynię Ucznia' } else { 'bez Skrzyni Ucznia dla nowych postaci' })
    }
    if ($written.Count -gt 0) {
        Write-Host ('Zapisano: ' + ($written -join ', ') + '.') -ForegroundColor Green
    }
}

function Reset-WorldAction {
    # "Zacznij od zera": the world a fresh install starts with, with the old one
    # kept as a zip. The volume is deleted, because that is the only thing that
    # makes MariaDB import initdb.d again.
    if (-not (Test-M2DockerRunning)) {
        Write-Host 'Silnik Dockera jest zatrzymany. Uruchom Docker i spróbuj ponownie.' -ForegroundColor Yellow
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host 'Nie mozna ustalic bazy tej instalacji.' -ForegroundColor Yellow
        return
    }
    $missing = @(Get-M2MissingSqlDumps -ServerRoot $serverRoot)
    if ($missing.Count -gt 0) {
        Write-Host 'Nie mogę zresetować świata: brakuje zrzutów, z których powstaje nowa baza.' -ForegroundColor Red
        Write-Host ('  Brakuje: ' + ($missing -join ', ')) -ForegroundColor Red
        Write-Host '  Miejsce: linux-port\docker\mariadb\initdb.d\dumps' -ForegroundColor Gray
        Write-Host 'Bez nich skasowanie bazy zostawiłoby instalację bez świata i bez sposobu na nowy.' -ForegroundColor Gray
        return
    }
    Write-Host ''
    Write-Host 'UWAGA: to kasuje CAŁY obecny świat - postacie, poziomy, ekwipunek, boty, konta gry.' -ForegroundColor Yellow
    Write-Host 'Przed skasowaniem świat zostanie zapisany do kopii zip w folderze backups,' -ForegroundColor Yellow
    Write-Host 'więc da się do niego wrócić akcją "Przywróć kopię".' -ForegroundColor Yellow
    Write-Host 'Po resecie pierwszy start potrwa dłużej: baza powstaje od nowa i boty są zasiewane.' -ForegroundColor Gray
    if (-not (Confirm-Operation 'Zresetować świat do stanu świeżej instalacji?')) {
        Write-Host 'Anulowano.' -ForegroundColor Yellow; return
    }
    Set-FreshWorldSettings -Reason 'nowego świata'
    Write-Host 'Zatrzymuję serwer i Dockera po stronie stosu...' -ForegroundColor Cyan
    Stop-Server
    Write-Host 'Zapisuję kopię i kasuję bazę...' -ForegroundColor Cyan
    $result = Reset-M2WorldToFreshInstall -Volume $target -ServerRoot $serverRoot `
        -BackupRoot (Join-Path $serverRoot 'backups')
    if ($result.Backup) {
        Write-Host ("Kopia poprzedniego świata ({0} postaci): {1}" -f $result.Players, $result.Backup) -ForegroundColor Gray
    }
    if ($ThenStart) {
        Write-Host 'Świat skasowany. Uruchamiam serwer z nowym światem - baza powstaje od nowa i boty są zasiewane, to potrwa dłużej niż zwykły start.' -ForegroundColor Green
        Start-Server
        return
    }
    Write-Host 'Świat skasowany. Kliknij GRAJ - serwer zbuduje bazę od nowa i zasieje boty.' -ForegroundColor Green
}

function Reset-PanelPasswordAction {
    # The panel keeps a PBKDF2 hash of its passphrase in m2panel.conf, on a
    # volume of its own, and its entrypoint never regenerates it - regenerating
    # would log every operator out and invalidate every session cookie. Right,
    # except when the passphrase it hashed is one nobody has: the container
    # invented it on a first run and printed it to a log nobody read.
    #
    # Deleting that one file is the whole reset. The entrypoint then rebuilds it
    # from M2_PANEL_PASSWORD, which the launcher now guarantees is in .env.
    $creds = Get-InstallDbCredentials
    $panelPw = ''
    if ($creds.EnvPath) {
        $text = [IO.File]::ReadAllText($creds.EnvPath)
        $match = [Regex]::Match($text, '(?m)^M2_PANEL_PASSWORD=(.+?)\s*$')
        if ($match.Success) { $panelPw = $match.Groups[1].Value }
    }
    if (-not $panelPw) {
        Write-Host 'W pliku .env nie ma hasla do panelu. Uruchom raz GRAJ - launcher je uzupelni i pokaze.' -ForegroundColor Yellow
        return
    }
    Write-Host 'Haslo do panelu WWW (z pliku linux-port\docker\.env):' -ForegroundColor Cyan
    Write-Host "  $panelPw"
    Write-Host ''
    Write-Host 'Jesli panel go nie przyjmuje, znaczy to, ze zapamietal starsze haslo.' -ForegroundColor Gray
    Write-Host 'Reset kasuje jeden plik konfiguracyjny panelu; swiat, postacie i boty' -ForegroundColor Gray
    Write-Host 'sa w bazie i nie sa tym ruszane. Wylogowuje otwarte sesje panelu.' -ForegroundColor Gray
    if (-not (Confirm-Operation 'Zresetowac haslo panelu do tego z .env?')) {
        Write-Host 'Anulowano - haslo wyzej pozostaje aktualne.' -ForegroundColor Yellow
        return
    }
    # The panel's config volume is named after the same project as the database
    # volume, which the launcher already knows how to find.
    $dbVolume = Get-CurrentInstallTargetVolume
    if (-not $dbVolume -or -not $dbVolume.EndsWith('_db-data')) {
        Write-Host 'Nie moge ustalic nazwy projektu tej instalacji. Uruchom raz GRAJ.' -ForegroundColor Yellow
        return
    }
    $volume = $dbVolume.Substring(0, $dbVolume.Length - '_db-data'.Length) + '_panel-conf'

    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    $previousPreference = $ErrorActionPreference
    try {
        # docker compose writes progress to stderr; under 'Stop' that is a
        # terminating error even when the command worked. See Stop-Server.
        $ErrorActionPreference = 'Continue'
        Write-Host 'Zatrzymuje panel...' -ForegroundColor Cyan
        docker compose --project-directory $composeDir -f $composeFile stop panel 2>&1 | Out-Null
        Write-Host 'Kasuje zapamietane haslo...' -ForegroundColor Cyan
        docker run --rm -v "${volume}:/etc/m2panel" alpine:3.20 rm -f /etc/m2panel/m2panel.conf 2>&1 | Out-Null
        $removeExit = $LASTEXITCODE
        if ($removeExit -ne 0) {
            Write-Host "Nie udalo sie skasowac pliku (kod $removeExit). Panel zostaje bez zmian." -ForegroundColor Red
            docker compose --project-directory $composeDir -f $composeFile start panel 2>&1 | Out-Null
            return
        }
        Write-Host 'Uruchamiam panel...' -ForegroundColor Cyan
        docker compose --project-directory $composeDir -f $composeFile up -d --no-deps panel 2>&1 | Out-Null
    }
    finally { $ErrorActionPreference = $previousPreference }

    Write-Host ''
    Write-Host 'Gotowe. Zaloguj sie haslem:' -ForegroundColor Green
    Write-Host "  $panelPw"
}

function Get-InstallDbCredentials {
    # Everything a database client needs, straight from .env: the port the
    # compose file publishes on 127.0.0.1, the game account and root. The root
    # password is what MariaDB was initialised with, and what Repair-DatabaseAction
    # puts back on root@'%' when the two have drifted apart.
    $result = [pscustomobject]@{ User = 'metin2'; Password = ''; RootPassword = ''; Port = '3306'; EnvPath = '' }
    $envPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $result.EnvPath = $envPath
        $text = [IO.File]::ReadAllText($envPath)
        $userMatch = [Regex]::Match($text, '(?m)^M2_DB_USER=(.+?)\s*$')
        if ($userMatch.Success) { $result.User = $userMatch.Groups[1].Value }
        $passMatch = [Regex]::Match($text, '(?m)^M2_DB_PASSWORD=(.+?)\s*$')
        if ($passMatch.Success) { $result.Password = $passMatch.Groups[1].Value }
        $rootMatch = [Regex]::Match($text, '(?m)^M2_DB_ROOT_PASSWORD=(.+?)\s*$')
        if ($rootMatch.Success) { $result.RootPassword = $rootMatch.Groups[1].Value }
        $portMatch = [Regex]::Match($text, '(?m)^M2_DB_PUBLISH_PORT=(\d+)\s*$')
        if ($portMatch.Success) { $result.Port = $portMatch.Groups[1].Value }
    }
    return $result
}

function Show-DatabaseAccessAction {
    # Where a database client (Navicat, HeidiSQL, DBeaver) connects, and with
    # which accounts. The passwords are not printed: this output lands in the
    # launcher log, and the launcher log lands in support bundles that get
    # posted on the Discord. The GUI shows them in a dialog of its own; here
    # the .env is opened in Notepad instead.
    $creds = Get-InstallDbCredentials
    if (-not $creds.EnvPath) {
        Write-Host 'Brak pliku linux-port\docker\.env — uruchom najpierw serwer (GRAJ), launcher go utworzy.' -ForegroundColor Yellow
        return
    }
    Write-Host 'Dane do połączenia z bazą (Navicat, HeidiSQL, DBeaver — typ MySQL/MariaDB):' -ForegroundColor Cyan
    Write-Host '  Host:      127.0.0.1'
    Write-Host "  Port:      $($creds.Port)"
    Write-Host '  Konto 1:   root        — pełny dostęp; hasło: M2_DB_ROOT_PASSWORD w pliku .env'
    Write-Host "  Konto 2:   $($creds.User)      — tylko bazy gry; hasło: M2_DB_PASSWORD w pliku .env"
    Write-Host "  Plik .env: $($creds.EnvPath)"
    Write-Host ''
    Write-Host 'Baza słucha tylko na tym komputerze (127.0.0.1), więc klient musi działać na nim.' -ForegroundColor Gray
    if ((Get-M2ServerEngine -ServerRoot $serverRoot) -ne 'r40250') {
        Write-Host 'Na plikach 2.x przedmioty i potwory (item_proto, mob_proto) są w bazie world; player.item_proto' -ForegroundColor Gray
        Write-Host 'i player.mob_proto to tylko widoki. Zmiany w world zostają po restarcie serwera.' -ForegroundColor Gray
    }
    Write-Host 'Jeśli baza odrzuca hasło z .env („Access denied"), użyj akcji RepairDb (przycisk' -ForegroundColor Gray
    Write-Host '„NAPRAW DOSTĘP DO BAZY"): ustawia konta root i metin2 na hasła z tego pliku.' -ForegroundColor Gray
    Write-Host 'Nie wklejaj haseł z .env na Discordzie ani do paczki z logami.' -ForegroundColor Yellow
    if (-not $Yes) {
        $answer = Read-Host 'Otworzyć plik .env w Notatniku, żeby skopiować hasła? [t/N]'
        if ($answer -match '^[tTyY]') { Start-Process notepad.exe -ArgumentList ('"' + $creds.EnvPath + '"') }
    }
}

function Repair-DatabaseAction {
    if (-not (Test-M2DockerRunning)) {
        Write-Host 'Silnik Dockera jest zatrzymany, więc nie widać żadnych baz.' -ForegroundColor Yellow
        Write-Host 'Uruchom Docker (akcja StartDocker lub przycisk „URUCHOM DOCKER") i spróbuj ponownie.' -ForegroundColor Yellow
        Write-Host 'Żadne dane nie zginęły — bazy są na dysku, tylko Docker ich teraz nie pokazuje.' -ForegroundColor Gray
        return
    }
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host 'Nie można ustalić bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choć raz.' -ForegroundColor Yellow
        return
    }
    $creds = Get-InstallDbCredentials
    if (-not $creds.Password) {
        Write-Host 'Brak M2_DB_PASSWORD w linux-port\docker\.env — nie mam czego przywrócić.' -ForegroundColor Red
        return
    }
    Write-Host "Naprawiam konta bazy dla instalacji: $target" -ForegroundColor Cyan
    Write-Host 'To odtwarza wyłącznie użytkowników i uprawnienia bazy — konto gry i root — z hasłami z pliku .env. Postacie, przedmioty i boty pozostają bez zmian.' -ForegroundColor Gray
    if (-not $creds.RootPassword) {
        Write-Host 'Brak M2_DB_ROOT_PASSWORD w .env — konto root zostanie pominięte.' -ForegroundColor Yellow
    }
    Write-Host 'Zatrzymuję serwer, aby zwolnić bazę...' -ForegroundColor Cyan
    Stop-Server
    if (Repair-M2GameDbUser -Volume $target -DbUser $creds.User -DbPassword $creds.Password -RootPassword $creds.RootPassword) {
        Write-Host 'Gotowe. Konta i uprawnienia bazy odtworzone. Kliknij GRAJ, aby uruchomić serwer.' -ForegroundColor Green
        Write-Host "Do Navicat: host 127.0.0.1, port $($creds.Port), root albo $($creds.User) — hasła z .env (akcja DbAccess pokaże szczegóły)." -ForegroundColor Gray
    }
    else {
        Write-Host 'Naprawa nie powiodła się. Zbierz logi (ZIP) i zgłoś problem.' -ForegroundColor Red
    }
}

function Create-Logs {
    $preflightLog = Join-Path $serverRoot 'launcher-logs\preflight-last.log'
    New-Item -ItemType Directory -Path (Split-Path -Parent $preflightLog) -Force | Out-Null
    $report = Get-M2DockerPreflight -ServerRoot $serverRoot -CheckPanelPort
    [IO.File]::WriteAllText(
        $preflightLog,
        (Format-M2DockerPreflightReport -Report $report),
        [Text.UTF8Encoding]::new($false))
    # Free space and the size of Docker's disk: whether the drive was full is
    # the first question a report of a read-only Docker disk raises, and the
    # bundle could not answer it (pattsito, 23 September).
    $extra = @{}
    try { $extra['disk-space.txt'] = Get-M2DiskSpaceReport -ServerRoot $serverRoot }
    catch { $extra['disk-space.txt'] = "Nie udalo sie odczytac miejsca na dyskach: $($_.Exception.Message)" }
    $bundle = New-M2SupportBundle -ServerRoot $serverRoot -ExtraFiles $extra
    Write-Host "Gotowa paczka diagnostyczna: $bundle" -ForegroundColor Green
    return $bundle
}

function Send-Logs {
    $config = Get-Config
    $support = Get-M2SupportSettings -Config $config
    if (-not $support.UploadUrl) {
        throw "Kanał zgłoszeń jest teraz niedostępny. Utwórz ZIP akcją Logs i wyślij go ręcznie na Discordzie: $($support.ContactUrl)"
    }
    $bundle = Create-Logs
    Write-Host 'Paczka zawiera logi Dockera i konfigurację z usuniętymi hasłami.' -ForegroundColor Yellow
    $target = if ($support.Source -eq 'manifest') { 'kanału zgłoszeń autora' } else { $support.UploadUrl }
    if (-not (Confirm-Operation "Wysłać $bundle do $target?")) {
        Write-Host 'Nie wysłano. ZIP pozostał na dysku.' -ForegroundColor Yellow
        return
    }
    $response = Send-M2SupportBundle -BundlePath $bundle -UploadUrl $support.UploadUrl
    if ($response) { Write-Host "Wysłano. Odpowiedź serwera: $response" -ForegroundColor Green }
    else { Write-Host 'Wysłano paczkę diagnostyczną.' -ForegroundColor Green }
}

# ---------------------------------------------------------------- co-op
# Playing the host's world with friends over the Internet (experimental;
# launcher\Metin2Launcher.Coop.psm1 does the work). The window's COOP dialog
# runs CoopHost, CoopStop and CoopCheck through here and does the rest itself:
# anything that prints a password is for the console only, because the
# window's action output is a file under launcher-logs, which the support
# bundle collects.

function Assert-CoopModule {
    if (-not (Get-Command Get-M2CoopNetworkReport -ErrorAction SilentlyContinue)) {
        throw 'Brak modułu launcher\Metin2Launcher.Coop.psm1 - ta paczka nie ma trybu COOP.'
    }
}

function Assert-CoopHostAccess {
    # Hosting is for the Patreon testers while COOP is tried out. The text menu
    # asks for their password here; an action started by the window runs with
    # no console to answer from, and the window asks before it starts one.
    # Ending hosting, renewing the lease and joining a friend never ask.
    Assert-CoopModule
    if (Test-M2CoopAccess -ServerRoot $serverRoot) { return }
    if ($Action -ne 'Menu') {
        throw 'Hostowanie w COOP testują na razie patroni: odblokuj je ich hasłem w oknie COOP launchera albo w menu tekstowym.'
    }
    Write-Host 'Hostowanie w COOP testują na razie patroni - hasło jest w poście dla patronów.' -ForegroundColor Yellow
    $secure = Read-Host 'Hasło testów COOP' -AsSecureString
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try { $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
    if (-not (Grant-M2CoopAccess -ServerRoot $serverRoot -Password $plain)) { throw 'To nie jest hasło testów COOP.' }
    Write-Host 'Hostowanie w COOP odblokowane na tej instalacji.' -ForegroundColor Green
}

function Write-CoopNetworkReport {
    param($Report)
    Write-Host ("Karta sieciowa: {0} ({1}), brama {2}" -f $Report.LanAddress, $Report.Interface, $Report.Gateway)
    Write-Host ("Adres widziany z internetu: {0}" -f $(if ($Report.PublicAddress) { $Report.PublicAddress } else { 'nie odczytano' }))
    if ($Report.Router) { Write-Host ("Router (UPnP): {0}, adres WAN {1}" -f $Report.Router, $Report.RouterWan) }
    else { Write-Host 'Router: nie odpowiedział na UPnP' }
    $color = $(if ($Report.Verdict -eq 'public') { 'Green' } elseif ($Report.Verdict -eq 'no-upnp' -or $Report.Verdict -eq 'mismatch') { 'Yellow' } else { 'Red' })
    Write-Host ("Wynik: {0}" -f $Report.Text) -ForegroundColor $color
    $vpns = @($Report.Vpns)
    foreach ($vpn in $vpns) { Write-Host ("Sieć VPN: {0}, adres {1} (karta {2})" -f $vpn.Name, $vpn.Address, $vpn.Interface) }
    if ($vpns.Count -eq 0) { Write-Host 'Sieć VPN: nie wykryto (Radmin VPN, Tailscale, ZeroTier, Hamachi).' }
}

function Get-CoopHostingField {
    # One field of the hosting record, '' when the record or the field is not
    # there (a state file written before a field existed). Under StrictMode a
    # missing property is an error, not an empty value.
    param($Hosting, [Parameter(Mandatory = $true)][string]$Name)
    if (-not $Hosting) { return '' }
    if (-not (@($Hosting.PSObject.Properties.Name) -contains $Name)) { return '' }
    return [string]$Hosting.$Name
}

function Show-CoopCheckAction {
    Assert-CoopModule
    Write-Phase 'sprawdzanie sieci'
    $report = Get-M2CoopNetworkReport
    Write-CoopNetworkReport -Report $report
    if (@('cgnat', 'double-nat') -contains $report.Verdict) {
        $vpns = @($report.Vpns)
        if ($vpns.Count -gt 0) { Write-Host ("Rozwiązanie: hostuj przez {0} - HOSTUJ ŚWIAT wybierze go sam." -f $vpns[0].Name) -ForegroundColor Yellow }
        else {
            Write-Host ('Rozwiązanie: zainstaluj Radmin VPN albo Tailscale, połącz się ze znajomymi w jednej sieci i hostuj ponownie - ' +
                'launcher wykryje VPN i użyje go zamiast routera.') -ForegroundColor Yellow
        }
    }
    $ports = Get-M2CoopGamePorts -ServerRoot $serverRoot
    Write-Host ("Porty gry: {0}" -f ($ports -join ', '))
    $bindings = Get-M2CoopGameBindings -ServerRoot $serverRoot
    if (-not $bindings.Running) { Write-Host 'Serwer gry nie działa (brak opublikowanych portów).' -ForegroundColor Yellow }
    elseif ($bindings.Public) { Write-Host 'Porty gry są otwarte na wszystkich kartach sieciowych - świat jest hostowany.' -ForegroundColor Green }
    else { Write-Host 'Porty gry słuchają tylko lokalnie (127.0.0.1) - świat nie jest hostowany.' }
    $hostingState = (Read-M2CoopState -ServerRoot $serverRoot).hosting
    if ((Get-CoopHostingField $hostingState 'mode') -eq 'vpn') {
        Write-Host ("Ostatnie hostowanie: przez {0}, adres dla znajomych {1}." -f (Get-CoopHostingField $hostingState 'vpnName'), (Get-CoopHostingField $hostingState 'friendAddress'))
    }
    if ($report.GatewayInfo) {
        foreach ($port in $ports) {
            $m = Get-M2CoopPortMapping -Gateway $report.GatewayInfo -Port $port
            if ($m) { Write-Host ("  router: port {0} -> {1}:{2} ({3})" -f $port, $m.InternalClient, $m.InternalPort, $m.Description) }
            else { Write-Host ("  router: port {0} bez przekierowania" -f $port) }
        }
    }
    Write-Host ("Reguła zapory Windows dla portów gry: {0}" -f $(if (Test-M2CoopFirewallRule) { 'jest' } else { 'brak (doda ją Hostuj)' }))
    foreach ($block in @(Get-M2CoopFirewallBlocks)) {
        Write-Host ("  UWAGA: zapora blokuje program {0} (reguła '{1}', profil {2}) - taka reguła wygrywa z każdą regułą zezwalającą." -f $block.Program, $block.Name, $block.Profile) -ForegroundColor Yellow
    }
    try {
        $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $serverRoot)
        if ($defaults.Count -gt 0) { Write-Host ("Konta z hasłem z paczki: {0} - przed hostowaniem użyj 'Zabezpiecz konta'." -f ($defaults -join ', ')) -ForegroundColor Yellow }
        else { Write-Host 'Konta admin i test nie mają haseł z paczki.' -ForegroundColor Green }
    }
    catch { Write-Host "Baza nie odpowiada: $($_.Exception.Message)" -ForegroundColor Yellow }
    $state = Read-M2CoopState -ServerRoot $serverRoot
    Write-Host ("Znajomi: {0}" -f @($state.friends).Count)
    foreach ($f in @($state.friends)) {
        Write-Host ("  {0}: login {1}{2}" -f $f.name, $f.login, $(if ($f.blocked) { ' (zablokowany)' } else { '' }))
    }
}

function Protect-CoopAccountsAction {
    Assert-CoopHostAccess
    $changed = Protect-M2CoopAccounts -ServerRoot $serverRoot
    $names = @($changed.PSObject.Properties | ForEach-Object { $_.Name })
    if ($names.Count -eq 0) {
        Write-Host 'Konta admin i test nie mają haseł z paczki - nic do zmiany.' -ForegroundColor Green
        return
    }
    foreach ($name in $names) {
        Write-Host ("Nowe hasło konta {0}: {1}" -f $name, $changed.$name) -ForegroundColor Yellow
    }
    Write-Host 'Zapisz je - od teraz logujesz się nimi (okno COOP w launcherze też je pokazuje).'
}

function Add-CoopFriendAction {
    Assert-CoopHostAccess
    $name = $FriendName
    if (-not $name) { $name = Read-Host 'Imię albo nick znajomego' }
    if (-not $name) { throw 'Nie podano imienia znajomego.' }
    $friend = New-M2CoopFriend -ServerRoot $serverRoot -Name $name
    Write-Host ("Konto dla {0}: login {1}, hasło {2}, kod usuwania postaci {3}" -f $friend.name, $friend.login, $friend.password, $friend.socialId) -ForegroundColor Green
    $target = Get-M2CoopInviteTarget -ServerRoot $serverRoot
    if ($target.Address) {
        Write-Host 'Kod zaproszenia (skopiuj i wyślij znajomemu):'
        Write-Host (Get-M2CoopFriendInvite -ServerRoot $serverRoot -Friend $friend -HostAddress $target.Address -Vpn $target.Vpn) -ForegroundColor Cyan
        if ($target.Vpn) { Write-Host ("Znajomy musi być w Twojej sieci {0} - kod prowadzi na adres {1}." -f $target.VpnName, $target.Address) -ForegroundColor Yellow }
    }
}

function Set-CoopFriendBlockedAction {
    param([bool]$Blocked = $true)
    Assert-CoopHostAccess
    $login = $FriendLogin
    if (-not $login) { $login = Read-Host 'Login znajomego' }
    if (-not $login) { throw 'Nie podano loginu.' }
    Set-M2CoopFriendBlocked -ServerRoot $serverRoot -Login $login -Blocked $Blocked
    if ($Blocked) { Write-Host "Konto $login zablokowane: nie zaloguje się, dopóki go nie odblokujesz." -ForegroundColor Green }
    else { Write-Host "Konto $login odblokowane." -ForegroundColor Green }
}

function Show-CoopInviteAction {
    Assert-CoopHostAccess
    $state = Read-M2CoopState -ServerRoot $serverRoot
    $target = Get-M2CoopInviteTarget -ServerRoot $serverRoot
    if (-not $target.Address) {
        if ($target.Vpn) { throw ("Nie udało się odczytać adresu {0} - uruchom go i spróbuj jeszcze raz." -f $target.VpnName) }
        throw 'Nie udało się odczytać adresu publicznego (brak internetu?).'
    }
    $shown = 0
    foreach ($f in @($state.friends)) {
        if ($FriendLogin -and [string]$f.login -ne $FriendLogin) { continue }
        if ($f.blocked) { continue }
        Write-Host ("{0} (login {1}, hasło {2}):" -f $f.name, $f.login, $f.password)
        Write-Host (Get-M2CoopFriendInvite -ServerRoot $serverRoot -Friend $f -HostAddress $target.Address -Vpn $target.Vpn) -ForegroundColor Cyan
        $shown++
    }
    if ($shown -eq 0) { Write-Host 'Brak znajomych - dodaj ich najpierw.' -ForegroundColor Yellow }
    elseif ($target.Vpn) { Write-Host ("Kody prowadzą na adres {0} w sieci {1} - znajomi muszą być w tej sieci." -f $target.Address, $target.VpnName) -ForegroundColor Yellow }
}

function Invoke-CoopGameRecreate {
    # Docker cannot move a running container's published ports, so the game
    # container is recreated with the new address - every core restarts, about
    # a minute. The panels stay on M2_PANEL_BIND_ADDRESS, written out as
    # 127.0.0.1 first if it was empty, so they never follow the game outwards.
    param([Parameter(Mandatory = $true)][string]$BindAddress)
    if (-not (Get-DotEnvValue -Key 'M2_PANEL_BIND_ADDRESS')) { Set-DotEnvValue -Key 'M2_PANEL_BIND_ADDRESS' -Value '127.0.0.1' }
    Set-DotEnvValue -Key 'M2_HOST_BIND_ADDRESS' -Value $BindAddress
    $composeDir = Join-Path $serverRoot 'linux-port\docker'
    $composeFile = Join-Path $composeDir 'docker-compose.yml'
    # compose writes its progress to stderr, which 'Stop' would turn into a
    # failure; the exit code decides (the same shape as Stop-Server).
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        docker compose --project-directory $composeDir -f $composeFile up -d --no-deps game
        $exit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($exit -ne 0) { throw "docker compose up game zakończył się kodem $exit." }
}

function Test-CoopCoreAnswers {
    # A core is up when it sends its handshake. A connection alone proves
    # nothing: Docker Desktop's port proxy accepts one before anything inside
    # the container listens and then closes it, so "connected" came back
    # eleven seconds into a boot the cores needed forty for.
    param([int]$Port)
    $client = New-Object Net.Sockets.TcpClient
    try {
        $wait = $client.BeginConnect('127.0.0.1', $Port, $null, $null)
        if (-not ($wait.AsyncWaitHandle.WaitOne(2000) -and $client.Connected)) { return $false }
        $client.EndConnect($wait)
        $stream = $client.GetStream()
        $stream.ReadTimeout = 3000
        $buffer = New-Object byte[] 16
        return ($stream.Read($buffer, 0, $buffer.Length) -gt 0)
    }
    catch { return $false }
    finally { $client.Close() }
}

function Wait-CoopGameReady {
    # Every core the client may be sent to has to answer, not only the auth:
    # a friend who logs in while game2 is still booting is dropped at the
    # first map that core hosts.
    param([int]$TimeoutSeconds = 240)
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $pending = New-Object System.Collections.Generic.List[int]
    foreach ($port in @(Get-M2CoopGamePorts -ServerRoot $serverRoot)) { $pending.Add([int]$port) }
    while ((Get-Date) -lt $deadline) {
        foreach ($port in @($pending)) { if (Test-CoopCoreAnswers -Port $port) { [void]$pending.Remove($port) } }
        if ($pending.Count -eq 0) { return $true }
        Start-Sleep -Seconds 3
    }
    Write-Host ("Nie odpowiadają jeszcze porty: {0}" -f ($pending -join ', ')) -ForegroundColor Yellow
    return $false
}

function Start-CoopHostingAction {
    Assert-CoopHostAccess
    Write-Phase 'sprawdzanie sieci'
    $report = Get-M2CoopNetworkReport
    Write-CoopNetworkReport -Report $report
    if (@('no-lan', 'offline') -contains $report.Verdict) {
        throw 'Ten komputer nie ma połączenia z internetem - hostowanie przerwane, nic nie zmieniono.'
    }
    # The text menu asks when there is a real choice - a VPN here and an
    # Internet that could work too; the window's button passes its own answer.
    $requested = $CoopVia
    $vpns = @($report.Vpns)
    if ($Action -eq 'Menu' -and $requested -eq 'auto' -and $vpns.Count -gt 0 -and -not (@('cgnat', 'double-nat') -contains $report.Verdict)) {
        if (Confirm-Action ("Wykryto {0} (adres {1}). Hostować przez VPN zamiast przez internet?" -f $vpns[0].Name, $vpns[0].Address)) { $requested = $vpns[0].Kind }
        else { $requested = 'internet' }
    }
    $via = Resolve-M2CoopHostingVia -Report $report -Requested $requested
    if ($via.Mode -eq 'blocked') {
        throw ('Z tej sieci znajomi nie połączą się bezpośrednio (operator albo drugi router nie daje publicznego adresu) - hostowanie przerwane, nic nie zmieniono. ' +
            'Zainstaluj Radmin VPN albo Tailscale, połącz się ze znajomymi w jednej sieci i hostuj ponownie: launcher wykryje VPN i użyje go zamiast routera.')
    }
    if ($via.Mode -eq 'vpn') { Write-Host ("Hostowanie przez {0}, adres {1}." -f $via.Vpn.Name, $via.Vpn.Address) -ForegroundColor Green }
    $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $serverRoot)
    if ($defaults.Count -gt 0) {
        throw ("Konta {0} mają hasła z paczki - każdy w internecie mógłby się na nie zalogować. Najpierw 'Zabezpiecz konta'." -f ($defaults -join ', '))
    }
    $ports = Get-M2CoopGamePorts -ServerRoot $serverRoot
    $bindings = Get-M2CoopGameBindings -ServerRoot $serverRoot
    if ($bindings.Public) { Write-Host 'Porty gry są już otwarte na wszystkich kartach sieciowych.' -ForegroundColor Green }
    else {
        Write-Phase 'porty gry dla sieci (restart serwera gry, około minuty)'
        Invoke-CoopGameRecreate -BindAddress '0.0.0.0'
        if (Wait-CoopGameReady) { Write-Host 'Serwer gry wstał.' -ForegroundColor Green }
        else { Write-Host 'Serwer gry jeszcze wstaje - znajomi zalogują się za chwilę.' -ForegroundColor Yellow }
    }
    Write-Host ("Opublikowane: {0}" -f ((Get-M2CoopGameBindings -ServerRoot $serverRoot).Lines -join '; '))
    Write-Phase 'zapora Windows'
    if (Test-M2CoopFirewallRule) { Write-Host 'Reguła zapory dla portów gry już jest.' }
    else {
        Write-Host 'Windows zapyta o zgodę administratora na regułę zapory dla portów gry - potwierdź.' -ForegroundColor Yellow
        if (Add-M2CoopFirewallRule -Ports $ports) { Write-Host 'Reguła zapory dodana.' -ForegroundColor Green }
        else { Write-Host 'Reguły zapory nie dodano (odmowa zgody?) - zapora może nie wpuścić znajomych.' -ForegroundColor Yellow }
    }
    foreach ($block in @(Get-M2CoopFirewallBlocks)) {
        Write-Host ("UWAGA: zapora blokuje program {0} (reguła '{1}') - usuń tę regułę w Zaporze Windows, inaczej znajomi się nie połączą." -f $block.Program, $block.Name) -ForegroundColor Yellow
    }
    $mapped = @()
    $state = Read-M2CoopState -ServerRoot $serverRoot
    if ($via.Mode -eq 'vpn') {
        # Nothing is opened in the router, and what hosting over the Internet
        # opened before is closed: through a VPN the world is for the VPN's
        # members and the LAN, not for everybody who scans the address.
        $wasMapped = @()
        if ($state.hosting -and (@($state.hosting.PSObject.Properties.Name) -contains 'mapped')) { $wasMapped = @($state.hosting.mapped) }
        if ($report.GatewayInfo -and $wasMapped.Count -gt 0) {
            Write-Phase 'router: zamykanie portów z hostowania przez internet'
            foreach ($port in $wasMapped) {
                if (Remove-M2CoopPortMapping -Gateway $report.GatewayInfo -Port ([int]$port) -LanAddress $report.LanAddress) { Write-Host "  port $port zamknięty" }
            }
        }
        Write-Host ("W routerze nic nie otwieram - znajomi łączą się przez {0}." -f $via.Vpn.Name)
    }
    elseif ($report.GatewayInfo) {
        Write-Phase 'przekierowania w routerze (UPnP)'
        foreach ($port in $ports) {
            $r = Add-M2CoopPortMapping -Gateway $report.GatewayInfo -Port $port -LanAddress $report.LanAddress
            if ($r.Ok) {
                $mapped += $port
                $lease = $(if ($r.Lease -gt 0) { "na $([int]($r.Lease / 3600)) h" } else { 'bez terminu' })
                Write-Host ("  port {0}: otwarty ({1})" -f $port, $lease) -ForegroundColor Green
            }
            else { Write-Host ("  port {0}: {1}" -f $port, $r.Reason) -ForegroundColor Red }
        }
    }
    else {
        Write-Host ("Router nie odpowiada na UPnP: przekieruj w nim ręcznie TCP {0} na {1}." -f ($ports -join ', '), $report.LanAddress) -ForegroundColor Yellow
    }
    $friendAddress = $(if ($via.Mode -eq 'vpn') { $via.Vpn.Address } else { $report.PublicAddress })
    $state.hosting = [pscustomobject]@{
        active = $true; since = (Get-Date).ToString('s'); lanAddress = $report.LanAddress
        publicAddress = $report.PublicAddress; ports = @($ports); mapped = @($mapped)
        mode = $via.Mode; vpn = $(if ($via.Vpn) { $via.Vpn.Kind } else { '' }); vpnName = $(if ($via.Vpn) { $via.Vpn.Name } else { '' })
        friendAddress = $friendAddress
    }
    Save-M2CoopState -ServerRoot $serverRoot -State $state
    Write-Host ''
    if ($via.Mode -eq 'vpn') {
        Write-Host ("Hostowanie włączone przez {0}. Adres dla znajomych: {1}" -f $via.Vpn.Name, $friendAddress) -ForegroundColor Green
        Write-Host ("Znajomi muszą dołączyć do Twojej sieci {0}, zanim wkleją kod zaproszenia." -f $via.Vpn.Name) -ForegroundColor Yellow
    }
    else { Write-Host ("Hostowanie włączone. Adres dla znajomych: {0}" -f $friendAddress) -ForegroundColor Green }
    Write-Host 'Ty grasz dalej na serwerze 1 (Metin2 SinglePlayer). Kody zaproszeń dla znajomych są w oknie COOP.'
    if (@($state.friends).Count -eq 0) { Write-Host 'Nie masz jeszcze znajomych - dodaj ich w oknie COOP.' -ForegroundColor Yellow }
    if ($mapped.Count -gt 0 -and $mapped.Count -lt $ports.Count) {
        Write-Host 'Nie wszystkie porty udało się otworzyć - bez nich znajomy utknie przy zmianie mapy.' -ForegroundColor Yellow
    }
}

function Stop-CoopHostingAction {
    Assert-CoopModule
    $state = Read-M2CoopState -ServerRoot $serverRoot
    $ports = Get-M2CoopGamePorts -ServerRoot $serverRoot
    $lan = Get-M2CoopLanAddress
    if ($lan) {
        $gateway = Find-M2CoopGateway -LanAddress $lan.Address
        if ($gateway) {
            Write-Phase 'router: zamykanie portów'
            foreach ($port in $ports) {
                if (Remove-M2CoopPortMapping -Gateway $gateway -Port $port -LanAddress $lan.Address) { Write-Host "  port $port zamknięty" }
                else { Write-Host "  port $port ma cudze przekierowanie - nie ruszam go" -ForegroundColor Yellow }
            }
        }
    }
    $bindings = Get-M2CoopGameBindings -ServerRoot $serverRoot
    $previous = Get-DotEnvValue -Key 'M2_HOST_BIND_ADDRESS'
    if ($bindings.Public -or $previous -ne '127.0.0.1') {
        Write-Phase 'porty gry tylko dla tego komputera (restart serwera gry, około minuty)'
        Invoke-CoopGameRecreate -BindAddress '127.0.0.1'
        [void](Wait-CoopGameReady)
    }
    Write-Host ("Opublikowane: {0}" -f ((Get-M2CoopGameBindings -ServerRoot $serverRoot).Lines -join '; '))
    if ($state.hosting) { $state.hosting.active = $false }
    Save-M2CoopState -ServerRoot $serverRoot -State $state
    Write-Host 'Hostowanie wyłączone. Reguła zapory zostaje, ale porty słuchają już tylko na tym komputerze.' -ForegroundColor Green
}

function Update-CoopHostingLease {
    # A mapping leased for four hours has to be renewed by somebody: GRAJ and
    # the window's timer both come here. Only while hosting is on, and quietly.
    if (-not (Get-Command Read-M2CoopState -ErrorAction SilentlyContinue)) { return }
    $state = Read-M2CoopState -ServerRoot $serverRoot
    if (-not ($state.hosting -and $state.hosting.active)) { return }
    # Through a VPN nothing is leased in the router.
    if ((Get-CoopHostingField $state.hosting 'mode') -eq 'vpn') { return }
    $lan = Get-M2CoopLanAddress
    if (-not $lan) { return }
    $gateway = Find-M2CoopGateway -LanAddress $lan.Address
    if (-not $gateway) { return }
    $ok = 0
    foreach ($port in @(Get-M2CoopGamePorts -ServerRoot $serverRoot)) {
        if ((Add-M2CoopPortMapping -Gateway $gateway -Port $port -LanAddress $lan.Address).Ok) { $ok++ }
    }
    Write-Host ("COOP: przekierowania w routerze odnowione ({0})." -f $ok)
}

function Join-CoopAction {
    Assert-CoopModule
    $code = $Invite
    if (-not $code) { $code = Read-Host 'Wklej kod zaproszenia od znajomego' }
    $inv = Read-M2CoopInvite -Code $code
    $client = Get-M2CoopClientFolder -ServerRoot $serverRoot
    if (-not $client) { throw 'Nie znaleziono folderu klienta (wskaż go przyciskiem WYBIERZ KLIENTA).' }
    $path = Write-M2CoopClientConfig -ClientFolder $client -Invite $inv
    Write-Host ("Zapisano {0}" -f $path) -ForegroundColor Green
    Write-Host ("W kliencie wybierz serwer 'Online: {0}' i zaloguj się: login {1}, hasło {2}" -f $inv.name, $inv.login, $inv.password) -ForegroundColor Cyan
    $advice = Get-M2CoopJoinAdvice -Invite $inv
    if ($advice) { Write-Host $advice -ForegroundColor Yellow }
    if (Test-M2CoopHostAnswers -HostAddress ([string]$inv.host) -Port ([int]$inv.auth)) { Write-Host 'Serwer znajomego odpowiada z tego komputera.' -ForegroundColor Green }
    else { Write-Host 'Serwer znajomego teraz nie odpowiada - sprawdź, czy ma uruchomiony serwer i włączone hostowanie.' -ForegroundColor Yellow }
}

function Invoke-Action {
    param([Parameter(Mandatory = $true)][string]$SelectedAction)
    $config = Get-Config
    switch ($SelectedAction) {
        'Start' { Start-Server }
        'Stop' { Stop-Server }
        'StartDocker' { Start-Docker }
        'StopAll' { Stop-DockerAndServer }
        'FreePorts' { Clear-PortConflictsAction }
        'Check' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
        }
        'UpdateServer' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
            Update-Server -RemoteManifest $remote
        }
        'UpdateClient' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
            Update-Client -RemoteManifest $remote -Config $config
        }
        'UpdateAll' {
            $remote = Get-M2UpdateManifest -Source (Get-ManifestSource $config)
            Show-UpdateStatus -RemoteManifest $remote
            $clientComponent = Get-ManifestComponent -RemoteManifest $remote -Name 'client'
            if ($clientComponent -and -not (Test-InstalledVersion -Installed ([string](Read-State).client) -Available ([string]$clientComponent.version))) {
                Assert-ClientNotRunning -Config $config
            }
            Update-Server -RemoteManifest $remote
            Update-Client -RemoteManifest $remote -Config $config
        }
        'Diagnose' { Show-DockerDiagnostics -CheckPanelPort | Out-Null }
        'Logs' { Create-Logs | Out-Null }
        'SendLogs' { Send-Logs }
        'Configure' { Configure-Launcher }
        'SetBots' { Set-BotCountAction }
        'SetDifficulty' { Set-DifficultyAction }
        'ImportDb' { Import-DatabaseAction }
        'BackupDb' { Backup-DatabaseAction }
        'RestoreDb' { Restore-DatabaseAction }
        'ResetWorld' { Reset-WorldAction }
        'RepairDb' { Repair-DatabaseAction }
        'DbAccess' { Show-DatabaseAccessAction }
        'PanelPassword' { Reset-PanelPasswordAction }
        'CoopCheck' { Show-CoopCheckAction }
        'CoopSecure' { Protect-CoopAccountsAction }
        'CoopAddFriend' { Add-CoopFriendAction }
        'CoopBlockFriend' { Set-CoopFriendBlockedAction -Blocked $true }
        'CoopUnblockFriend' { Set-CoopFriendBlockedAction -Blocked $false }
        'CoopInvite' { Show-CoopInviteAction }
        'CoopHost' { Start-CoopHostingAction }
        'CoopStop' { Stop-CoopHostingAction }
        'CoopRenew' { Assert-CoopModule; Update-CoopHostingLease }
        'CoopJoin' { Join-CoopAction }
        default { throw "Nieznana akcja: $SelectedAction" }
    }
}

function Show-Menu {
    while ($true) {
        Write-Header
        Write-Host '  1. Uruchom serwer'
        Write-Host '  2. Zatrzymaj serwer'
        Write-Host '  3. Uruchom tylko Docker Desktop'
        Write-Host '  4. Zatrzymaj serwer i Docker (postęp zostaje)'
        Write-Host '  5. Sprawdź aktualizacje'
        Write-Host '  6. Aktualizuj serwer'
        Write-Host '  7. Aktualizuj klienta'
        Write-Host '  8. Aktualizuj wszystko'
        Write-Host '  9. Sprawdź Docker, WSL, wirtualizację i porty'
        Write-Host ' 10. Utwórz paczkę diagnostyczną ZIP'
        Write-Host ' 11. Utwórz i wyślij logi (po potwierdzeniu)'
        Write-Host ' 12. Konfiguracja launchera'
        Write-Host ' 13. Ustaw liczbę grających botów (0-2500)'
        Write-Host ' 14. Importuj bazę z innej instalacji (wyższe postacie)'
        Write-Host ' 15. Zapisz kopię świata (backup do pliku zip)'
        Write-Host ' 16. Przywróć świat z kopii'
        Write-Host ' 17. Wyzeruj świat i zacznij od nowa (świeża instalacja; kopia zapisywana automatycznie)'
        Write-Host ' 18. Napraw dostęp do bazy (gdy migrate/serwer nie startuje albo Navicat odrzuca hasło)'
        Write-Host ' 19. Dane do połączenia z bazą (Navicat, HeidiSQL)'
        Write-Host ' 20. Hasło do panelu WWW (pokaż / zresetuj)'
        Write-Host ' 21. Zwolnij porty (gdy „port jest już zajęty” blokuje start lub aktualizację)'
        Write-Host ' 22. Poziom trudności (czekanie u Biologa i Stajennego: easy / medium / hard / własne godziny)'
        if (Get-Command Get-M2CoopNetworkReport -ErrorAction SilentlyContinue) {
            Write-Host ' 23. COOP: sprawdź sieć i stan hostowania (eksperymentalne)'
            if (-not (Test-M2CoopAccess -ServerRoot $serverRoot)) {
                Write-Host '     Hostowanie (24-27) testują na razie patroni - launcher zapyta o ich hasło.' -ForegroundColor DarkGray
            }
            Write-Host ' 24. COOP: zabezpiecz konta admin i test (nowe hasła)'
            Write-Host ' 25. COOP: dodaj znajomego (konto i kod zaproszenia)'
            Write-Host ' 26. COOP: pokaż kody zaproszeń'
            Write-Host ' 27. COOP: hostuj świat dla znajomych'
            Write-Host ' 28. COOP: zakończ hostowanie'
            Write-Host ' 29. COOP: dołącz do świata znajomego (wklej kod)'
        }
        Write-Host '  0. Wyjście'
        Write-Host ''
        $choice = Read-Host 'Wybierz opcję'
        $selected = switch ($choice) {
            '1' { 'Start' } '2' { 'Stop' } '3' { 'StartDocker' } '4' { 'StopAll' }
            '5' { 'Check' } '6' { 'UpdateServer' } '7' { 'UpdateClient' }
            '8' { 'UpdateAll' } '9' { 'Diagnose' } '10' { 'Logs' } '11' { 'SendLogs' } '12' { 'Configure' }
            '13' { 'SetBots' }
            '14' { 'ImportDb' }
            '15' { 'BackupDb' }
            '16' { 'RestoreDb' }
            '17' { 'ResetWorld' }
            '18' { 'RepairDb' }
            '19' { 'DbAccess' }
            '20' { 'PanelPassword' }
            '21' { 'FreePorts' }
            '22' { 'SetDifficulty' }
            '23' { 'CoopCheck' }
            '24' { 'CoopSecure' }
            '25' { 'CoopAddFriend' }
            '26' { 'CoopInvite' }
            '27' { 'CoopHost' }
            '28' { 'CoopStop' }
            '29' { 'CoopJoin' }
            '0' { return }
            default { '' }
        }
        if (-not $selected) { continue }
        try { Invoke-Action -SelectedAction $selected }
        catch { Write-Host "BŁĄD: $($_.Exception.Message)" -ForegroundColor Red }
        Write-Host ''
        Read-Host 'Naciśnij Enter, aby wrócić do menu' | Out-Null
    }
}

try {
    if ($Action -eq 'Menu') { Show-Menu }
    else { Invoke-Action -SelectedAction $Action }
}
catch {
    Write-Host "BŁĄD: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
