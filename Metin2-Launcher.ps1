[CmdletBinding()]
param(
    [ValidateSet('Menu', 'Start', 'Stop', 'StartDocker', 'StopAll', 'Check', 'UpdateServer', 'UpdateClient', 'UpdateAll', 'Diagnose', 'Logs', 'SendLogs', 'Configure', 'SetBots', 'ImportDb', 'RepairDb')]
    [string]$Action = 'Menu',
    [string]$Manifest = '',
    [int]$BotCount = -1,
    [string]$ImportSource = '',
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
$serverRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$modulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.psm1'
$diagnosticsModulePath = Join-Path $serverRoot 'launcher\Metin2Launcher.Diagnostics.psm1'
$configPath = Join-Path $serverRoot '.m2launcher.json'
$statePath = Join-Path $serverRoot '.m2launcher-state.json'

foreach ($requiredModule in @($modulePath, $diagnosticsModulePath)) {
    if (-not (Test-Path -LiteralPath $requiredModule -PathType Leaf)) {
        throw "Brakuje modułu launchera: $requiredModule"
    }
}
Import-Module $modulePath -Force
Import-Module $diagnosticsModulePath -Force

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

function Read-State {
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        return Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    $versionFile = Join-Path $serverRoot 'VERSION'
    $serverVersion = if (Test-Path -LiteralPath $versionFile) {
        (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    else { 'unknown' }
    return [pscustomobject]@{ schema = 1; server = $serverVersion; client = 'unknown' }
}

function Save-State {
    param([string]$ServerVersion, [string]$ClientVersion)
    $state = Read-State
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

function Start-Server {
    Assert-DockerPrerequisites -CheckPanelPort
    $script = Join-Path $serverRoot 'start-server.ps1'
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) { throw 'Brakuje start-server.ps1.' }
    & $script
    if ($LASTEXITCODE -ne 0) { throw "Uruchamianie serwera zakończyło się kodem $LASTEXITCODE." }
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
    # See Stop-Server: compose progress on stderr must not be treated as failure
    # under $ErrorActionPreference='Stop' in Windows PowerShell 5.1.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        docker compose --project-directory $composeDir -f $composeFile up -d --build
        $buildExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($buildExit -ne 0) {
        throw 'Nowa wersja plików została zapisana, ale Docker nie zbudował serwera. Kopia plików jest w katalogu backups.'
    }
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
    if (-not (Confirm-Operation 'Zaktualizować pliki serwera i przebudować kontenery? Baza postaci pozostanie bez zmian.')) {
        Write-Host 'Anulowano.' -ForegroundColor Yellow
        return
    }
    $result = Invoke-M2PackageUpdate -Component $component -TargetRoot $serverRoot -BackupRoot (Join-Path $serverRoot 'backups')
    Write-Host "Podmieniono $($result.Files) plików. Kopia: $($result.Backup)" -ForegroundColor Green
    Rebuild-Server
    Save-State -ServerVersion $result.Version -ClientVersion ''
    Write-Host "Serwer działa w wersji $($result.Version)." -ForegroundColor Green
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
    # Writes PLAYERBOT_AUTOSPAWN_COUNT to .env. The game core reads it once on
    # startup and spawns at most this many of the *seeded* bots, so the effective
    # ceiling is the number of seeded playerbots (350 in the canonical seed).
    param([Parameter(Mandatory = $true)][int]$Count)
    if ($Count -lt 0) { $Count = 0 }
    if ($Count -gt 1000) { $Count = 1000 }
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

function Set-BotCountAction {
    $current = Get-PlayerbotCount
    Write-Host "Aktualnie gra: $current botów (limit = liczba zaseedowanych botów, w kanonicznej paczce 350)." -ForegroundColor Gray

    # -BotCount passed (from the GUI or scripting) is non-interactive: never call
    # Read-Host, because the GUI runs this in a hidden, non-interactive console.
    # Restart only when -Yes is also given. Without -BotCount we are in the text
    # menu and can prompt for both the number and the restart.
    if ($BotCount -ge 0) {
        $applied = Set-PlayerbotCount -Count $BotCount
        Write-Host "Zapisano: $applied grających botów." -ForegroundColor Green
        if ($Yes) {
            Start-Server
            Write-Host "Serwer zrestartowany z liczbą botów: $applied." -ForegroundColor Green
        }
        else {
            Write-Host 'Zmiana zostanie zastosowana przy następnym starcie serwera.' -ForegroundColor Yellow
        }
        return
    }

    $answer = Read-Host 'Ilu botów ma grać (0-350)'
    if ($answer -notmatch '^\d+$') { Write-Host 'Anulowano: to nie jest liczba.' -ForegroundColor Yellow; return }
    $applied = Set-PlayerbotCount -Count ([int]$answer)
    Write-Host "Zapisano: $applied grających botów." -ForegroundColor Green
    if (Confirm-Operation 'Zrestartować serwer teraz, aby zastosować zmianę? Baza i postęp botów pozostają bez zmian') {
        Start-Server
        Write-Host "Serwer zrestartowany z liczbą botów: $applied." -ForegroundColor Green
    }
    else {
        Write-Host 'Zmiana zostanie zastosowana przy następnym starcie serwera.' -ForegroundColor Yellow
    }
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
    $target = Get-CurrentInstallTargetVolume
    if (-not $target) {
        Write-Host 'Nie można ustalić bazy tej instalacji. Uruchom najpierw serwer (GRAJ) choć raz, aby utworzyć tożsamość i wolumen.' -ForegroundColor Yellow
        return
    }
    Write-Host "Baza docelowa (ta instalacja): $target" -ForegroundColor Gray
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
        for ($i = 0; $i -lt $sources.Count; $i++) { Write-Host ("  [{0}] {1}" -f ($i + 1), $sources[$i].Project) }
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

function Get-InstallDbCredentials {
    $result = [pscustomobject]@{ User = 'metin2'; Password = '' }
    $envPath = Join-Path $serverRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $text = [IO.File]::ReadAllText($envPath)
        $userMatch = [Regex]::Match($text, '(?m)^M2_DB_USER=(.+?)\s*$')
        if ($userMatch.Success) { $result.User = $userMatch.Groups[1].Value }
        $passMatch = [Regex]::Match($text, '(?m)^M2_DB_PASSWORD=(.+?)\s*$')
        if ($passMatch.Success) { $result.Password = $passMatch.Groups[1].Value }
    }
    return $result
}

function Repair-DatabaseAction {
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
    Write-Host "Naprawiam konto techniczne bazy dla instalacji: $target" -ForegroundColor Cyan
    Write-Host 'To odtwarza wyłącznie użytkownika i uprawnienia bazy. Postacie, przedmioty i boty pozostają bez zmian.' -ForegroundColor Gray
    Write-Host 'Zatrzymuję serwer, aby zwolnić bazę...' -ForegroundColor Cyan
    Stop-Server
    if (Repair-M2GameDbUser -Volume $target -DbUser $creds.User -DbPassword $creds.Password) {
        Write-Host 'Gotowe. Konto i uprawnienia bazy odtworzone. Kliknij GRAJ, aby uruchomić serwer.' -ForegroundColor Green
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
    $bundle = New-M2SupportBundle -ServerRoot $serverRoot
    Write-Host "Gotowa paczka diagnostyczna: $bundle" -ForegroundColor Green
    return $bundle
}

function Send-Logs {
    $config = Get-Config
    if (-not $config.supportUploadUrl) {
        throw 'Nie ustawiono adresu pomocy. Utwórz ZIP akcją Logs i wyślij go ręcznie na Discordzie.'
    }
    $bundle = Create-Logs
    Write-Host 'Paczka zawiera logi Dockera i konfigurację z usuniętymi hasłami.' -ForegroundColor Yellow
    if (-not (Confirm-Operation "Wysłać $bundle do $($config.supportUploadUrl)?")) {
        Write-Host 'Nie wysłano. ZIP pozostał na dysku.' -ForegroundColor Yellow
        return
    }
    $response = Send-M2SupportBundle -BundlePath $bundle -UploadUrl $config.supportUploadUrl
    if ($response) { Write-Host "Wysłano. Odpowiedź serwera: $response" -ForegroundColor Green }
    else { Write-Host 'Wysłano paczkę diagnostyczną.' -ForegroundColor Green }
}

function Invoke-Action {
    param([Parameter(Mandatory = $true)][string]$SelectedAction)
    $config = Get-Config
    switch ($SelectedAction) {
        'Start' { Start-Server }
        'Stop' { Stop-Server }
        'StartDocker' { Start-Docker }
        'StopAll' { Stop-DockerAndServer }
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
            Update-Server -RemoteManifest $remote
            Update-Client -RemoteManifest $remote -Config $config
        }
        'Diagnose' { Show-DockerDiagnostics -CheckPanelPort | Out-Null }
        'Logs' { Create-Logs | Out-Null }
        'SendLogs' { Send-Logs }
        'Configure' { Configure-Launcher }
        'SetBots' { Set-BotCountAction }
        'ImportDb' { Import-DatabaseAction }
        'RepairDb' { Repair-DatabaseAction }
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
        Write-Host ' 13. Ustaw liczbę grających botów (0-350)'
        Write-Host ' 14. Importuj bazę z innej instalacji (wyższe postacie)'
        Write-Host ' 15. Napraw dostęp do bazy (gdy migrate/serwer nie startuje)'
        Write-Host '  0. Wyjście'
        Write-Host ''
        $choice = Read-Host 'Wybierz opcję'
        $selected = switch ($choice) {
            '1' { 'Start' } '2' { 'Stop' } '3' { 'StartDocker' } '4' { 'StopAll' }
            '5' { 'Check' } '6' { 'UpdateServer' } '7' { 'UpdateClient' }
            '8' { 'UpdateAll' } '9' { 'Diagnose' } '10' { 'Logs' } '11' { 'SendLogs' } '12' { 'Configure' }
            '13' { 'SetBots' }
            '14' { 'ImportDb' }
            '15' { 'RepairDb' }
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
