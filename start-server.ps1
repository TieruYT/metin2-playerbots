[CmdletBinding()]
param(
    [ValidateRange(30, 600)]
    [int]$DockerTimeoutSeconds = 180,
    [switch]$NoSocketRecovery,
    [switch]$DockerOnly,
    [switch]$IdentityOnly,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'

function Test-DockerApi {
    $previousPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 can promote native stderr to a terminating
        # NativeCommandError while Docker Desktop is stopped. An unavailable
        # engine is a normal status here, not a launcher failure.
        $ErrorActionPreference = 'SilentlyContinue'
        & docker info 1>$null 2>$null
        return $LASTEXITCODE -eq 0
    }
    finally { $ErrorActionPreference = $previousPreference }
}

function Set-DotEnvValue {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value
    )
    $pattern = '(?m)^' + [Regex]::Escape($Name) + '=.*$'
    if ([Regex]::IsMatch($Content, $pattern)) {
        return [Regex]::Replace($Content, $pattern, "$Name=$Value")
    }
    if ($Content -and -not $Content.EndsWith("`n")) { $Content += [Environment]::NewLine }
    return $Content + "$Name=$Value" + [Environment]::NewLine
}

function Get-DotEnvValue {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $match = [Regex]::Match($Content, '(?m)^' + [Regex]::Escape($Name) + '=(.*)$')
    if ($match.Success) { return $match.Groups[1].Value.Trim() }
    return ''
}

function Invoke-DockerQuery {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & docker @Arguments 2>&1
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Output = (($output | Out-String).Trim())
        }
    }
    finally { $ErrorActionPreference = $previousPreference }
}

function Get-ObjectPropertyValue {
    param($Object, [Parameter(Mandatory = $true)][string]$Name)
    if ($null -eq $Object) { return '' }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) { return '' }
    return [string]$property.Value
}

function Find-LegacyEnvironmentPath {
    param(
        [AllowEmptyString()][string]$WorkingDirectory,
        [AllowEmptyString()][string]$ProjectName
    )

    $directories = New-Object System.Collections.Generic.List[string]
    foreach ($candidate in @(
        $WorkingDirectory,
        $env:M2_EXISTING_SERVER_DIR,
        (Join-Path $env:USERPROFILE 'Metin2Server'),
        'C:\Metin2Server'
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and -not $directories.Contains($candidate)) {
            $directories.Add($candidate)
        }
    }

    foreach ($directory in $directories) {
        try { $full = [IO.Path]::GetFullPath($directory).TrimEnd('\') }
        catch { continue }
        $envPath = Join-Path $full '.env'
        $composePath = Join-Path $full 'docker-compose.yml'
        if ((Test-Path -LiteralPath $envPath -PathType Leaf) -and
            (Test-Path -LiteralPath $composePath -PathType Leaf)) {
            return $envPath
        }
    }
    return ''
}

function Get-CompatibleDockerInstallations {
    # A legacy install.ps1 deployment and the All-in-One package use the same
    # persistent-volume layout. Docker labels are a safer source of truth than
    # guessing folders or looking only at occupied ports.
    $idsResult = Invoke-DockerQuery @('ps', '-aq')
    if ($idsResult.ExitCode -ne 0 -or -not $idsResult.Output) { return @() }
    $ids = @($idsResult.Output -split '\s+' | Where-Object { $_ })
    if ($ids.Count -eq 0) { return @() }

    $inspection = Invoke-DockerQuery (@('inspect') + $ids)
    if ($inspection.ExitCode -ne 0 -or -not $inspection.Output) { return @() }
    # Windows PowerShell 5.1 quirk: `@(<pipeline> | ConvertFrom-Json)' wraps a
    # multi-element JSON array into a single nested item instead of enumerating
    # it, which silently collapsed two running installations into one and
    # defeated the ambiguity guard below. Assign first, then wrap.
    try { $parsed = $inspection.Output | ConvertFrom-Json; $objects = @($parsed) }
    catch { return @() }

    $result = New-Object System.Collections.Generic.List[object]
    foreach ($container in $objects) {
        $labels = $container.Config.Labels
        $project = Get-ObjectPropertyValue $labels 'com.docker.compose.project'
        $service = Get-ObjectPropertyValue $labels 'com.docker.compose.service'
        if (-not $project -or $service -notin @('mariadb', 'db')) { continue }

        $dbMount = @($container.Mounts | Where-Object {
            $_.Type -eq 'volume' -and $_.Destination -eq '/var/lib/mysql'
        } | Select-Object -First 1)
        if ($dbMount.Count -eq 0 -or -not [string]$dbMount[0].Name) { continue }

        $containerName = ([string]$container.Name).TrimStart('/')
        $prefix = if ($containerName -match '^(.+)-db$') { $Matches[1] } else { $project }
        $workingDirectory = Get-ObjectPropertyValue $labels 'com.docker.compose.project.working_dir'
        $envPath = Find-LegacyEnvironmentPath -WorkingDirectory $workingDirectory -ProjectName $project
        $result.Add([pscustomobject]@{
            projectName = $project
            containerPrefix = $prefix
            workingDirectory = $workingDirectory
            environmentPath = $envPath
            databaseVolume = [string]$dbMount[0].Name
            source = 'container'
        })
    }
    return $result.ToArray()
}

function Get-CompatibleDockerVolumes {
    # `docker compose down' removes containers but deliberately keeps named
    # volumes. Cover that state too; adoption is allowed only if the original
    # .env can also be found, because it contains the MariaDB credentials.
    $namesResult = Invoke-DockerQuery @(
        'volume', 'ls',
        '--filter', 'label=com.docker.compose.volume=db-data',
        '--format', '{{.Name}}')
    if ($namesResult.ExitCode -ne 0 -or -not $namesResult.Output) { return @() }

    $result = New-Object System.Collections.Generic.List[object]
    foreach ($name in @($namesResult.Output -split '\s+' | Where-Object { $_ })) {
        $inspection = Invoke-DockerQuery @('volume', 'inspect', $name)
        if ($inspection.ExitCode -ne 0 -or -not $inspection.Output) { continue }
        try { $parsed = $inspection.Output | ConvertFrom-Json; $volume = @($parsed)[0] }
        catch { continue }
        $project = Get-ObjectPropertyValue $volume.Labels 'com.docker.compose.project'
        if (-not $project) { continue }
        $envPath = Find-LegacyEnvironmentPath -WorkingDirectory '' -ProjectName $project
        if (-not $envPath) { continue }
        $result.Add([pscustomobject]@{
            projectName = $project
            containerPrefix = $project
            workingDirectory = Split-Path -Parent $envPath
            environmentPath = $envPath
            databaseVolume = [string]$volume.Name
            source = 'volume'
        })
    }
    return $result.ToArray()
}

function Find-CompatibleDockerInstallation {
    $candidates = @(Get-CompatibleDockerInstallations)
    if ($candidates.Count -eq 0) { $candidates = @(Get-CompatibleDockerVolumes) }
    if ($candidates.Count -eq 0) { return $null }

    # One database volume may have a stopped and a replaced DB container in a
    # rare failed upgrade. Collapse identical projects before deciding whether
    # the situation is ambiguous.
    $unique = @($candidates | Group-Object projectName | ForEach-Object { $_.Group | Select-Object -First 1 })
    if ($unique.Count -gt 1) {
        $details = ($unique | ForEach-Object {
            "  - $($_.projectName) ($($_.workingDirectory))"
        }) -join [Environment]::NewLine
        throw "Znaleziono kilka instalacji Metin2. Launcher niczego nie zmienił. Uruchom go z folderu właściwej instalacji albo zatrzymaj pozostałe stosy:`n$details"
    }

    $selected = $unique[0]
    if (-not $selected.environmentPath) {
        throw "Znaleziono bazę $($selected.databaseVolume), ale nie znaleziono jej oryginalnego pliku .env w $($selected.workingDirectory). Nie uruchomiono drugiego serwera i nie zmieniono wolumenu. Ustaw M2_EXISTING_SERVER_DIR na stary folder serwera i spróbuj ponownie."
    }
    return $selected
}

function Merge-DotEnvFile {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$SourcePath
    )
    foreach ($line in [IO.File]::ReadAllLines($SourcePath)) {
        if ($line -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {
            $name = $Matches[1]
            if ($name -in @('M2_COMPOSE_PROJECT_NAME', 'M2_CONTAINER_PREFIX')) { continue }
            $Content = Set-DotEnvValue -Content $Content -Name $name -Value $Matches[2]
        }
    }
    return $Content
}

function Initialize-InstallationIdentity {
    $envPath = Join-Path $PSScriptRoot 'linux-port\docker\.env'
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        throw "Missing Docker environment file: $envPath"
    }
    $statePath = Join-Path $PSScriptRoot '.m2install.json'
    $project = ''
    $prefix = ''
    $createdAt = ''
    $migratedFrom = ''
    $databaseVolume = ''

    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $project = [string]$state.projectName
            $prefix = [string]$state.containerPrefix
            $createdAt = Get-ObjectPropertyValue $state 'createdAt'
            $migratedFrom = Get-ObjectPropertyValue $state 'migratedFrom'
            $databaseVolume = Get-ObjectPropertyValue $state 'databaseVolume'
        }
        catch { throw "Invalid installation identity file: $statePath" }
    }

    $content = [IO.File]::ReadAllText($envPath)
    if (-not $project -and $content -match '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$') {
        $project = $Matches[1]
    }
    if (-not $prefix -and $content -match '(?m)^M2_CONTAINER_PREFIX=([a-z0-9][a-z0-9_-]+)\s*$') {
        $prefix = $Matches[1]
    }

    if (-not $project) {
        $existing = Find-CompatibleDockerInstallation
        if ($null -ne $existing) {
            $project = [string]$existing.projectName
            $prefix = [string]$existing.containerPrefix
            $migratedFrom = [string]$existing.workingDirectory
            $databaseVolume = [string]$existing.databaseVolume
            $content = Merge-DotEnvFile -Content $content -SourcePath ([string]$existing.environmentPath)
            Write-Host "Wykryto istniejący serwer '$project'. Launcher przejmuje go bez przenoszenia ani usuwania wolumenu $($existing.databaseVolume)." -ForegroundColor Yellow
        }
    }

    if (-not $project) { $project = 'm2pb-' + [Guid]::NewGuid().ToString('N').Substring(0, 8) }
    if (-not $prefix) { $prefix = $project }
    if ($project -notmatch '^[a-z0-9][a-z0-9_-]+$' -or $prefix -notmatch '^[a-z0-9][a-z0-9_-]+$') {
        throw 'Installation identity contains unsupported characters.'
    }

    if (-not $createdAt) { $createdAt = [DateTime]::UtcNow.ToString('o') }
    $stateObject = [ordered]@{
        schema = 1
        projectName = $project
        containerPrefix = $prefix
        createdAt = $createdAt
    }
    if ($migratedFrom) { $stateObject.migratedFrom = $migratedFrom }
    if ($databaseVolume) { $stateObject.databaseVolume = $databaseVolume }
    [IO.File]::WriteAllText(
        $statePath,
        ($stateObject | ConvertTo-Json),
        [Text.UTF8Encoding]::new($false))
    $content = Set-DotEnvValue -Content $content -Name 'M2_COMPOSE_PROJECT_NAME' -Value $project
    $content = Set-DotEnvValue -Content $content -Name 'M2_CONTAINER_PREFIX' -Value $prefix
    [IO.File]::WriteAllText($envPath, $content, [Text.UTF8Encoding]::new($false))
    Write-Host "Installation identity: $project" -ForegroundColor DarkGray
}

function Assert-NoForeignPortOwner {
    param(
        [Parameter(Mandatory = $true)][int]$Port,
        [Parameter(Mandatory = $true)][string]$Project
    )
    $published = Invoke-DockerQuery @('ps', '--filter', "publish=$Port", '--format', '{{.ID}}')
    $ids = if ($published.ExitCode -eq 0 -and $published.Output) {
        @($published.Output -split '\s+' | Where-Object { $_ })
    }
    else { @() }

    foreach ($id in $ids) {
        $inspection = Invoke-DockerQuery @('inspect', $id)
        if ($inspection.ExitCode -ne 0 -or -not $inspection.Output) { continue }
        try { $parsed = $inspection.Output | ConvertFrom-Json; $container = @($parsed)[0] }
        catch { continue }
        $owner = Get-ObjectPropertyValue $container.Config.Labels 'com.docker.compose.project'
        if ($owner -and -not $owner.Equals($Project, [StringComparison]::OrdinalIgnoreCase)) {
            $workingDirectory = Get-ObjectPropertyValue $container.Config.Labels 'com.docker.compose.project.working_dir'
            throw "Port 127.0.0.1:$Port jest już używany przez inną instalację '$owner' ($workingDirectory). Launcher nie uruchomi drugiego serwera. Użyj istniejącej instalacji albo zatrzymaj ją bez opcji -v."
        }
    }

    if ($ids.Count -eq 0) {
        $used = $null
        try {
            $used = [Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners() |
                Where-Object { $_.Port -eq $Port } | Select-Object -First 1
        }
        catch { }
        if ($used) {
            throw "Port 127.0.0.1:$Port jest już zajęty przez inny program. Launcher nie uruchomił drugiego serwera."
        }
    }
}

function Assert-NoForeignPortOwners {
    $envPath = Join-Path $PSScriptRoot 'linux-port\docker\.env'
    $content = [IO.File]::ReadAllText($envPath)
    $statePath = Join-Path $PSScriptRoot '.m2install.json'
    $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
    $project = [string]$state.projectName
    $panelPort = Get-DotEnvValue -Content $content -Name 'M2_PANEL_PUBLIC_PORT'
    $authPort = Get-DotEnvValue -Content $content -Name 'M2_AUTH_PORT'
    if ($panelPort -notmatch '^\d+$') { $panelPort = '7788' }
    if ($authPort -notmatch '^\d+$') { $authPort = '11000' }
    Assert-NoForeignPortOwner -Port ([int]$panelPort) -Project $project
    Assert-NoForeignPortOwner -Port ([int]$authPort) -Project $project
}

function Get-DockerDesktopProcesses {
    $processNames = @(
        'Docker Desktop',
        'com.docker.backend',
        'com.docker.build',
        'com.docker.dev-envs',
        'com.docker.extensions',
        'vpnkit'
    )
    return @(Get-Process -Name $processNames -ErrorAction SilentlyContinue)
}

function Wait-DockerApi([int]$TimeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        if (Test-DockerApi) {
            return $true
        }
        Start-Sleep -Seconds 2
    } while ((Get-Date) -lt $deadline)
    return $false
}

function Move-StaleDockerSocketDirectory([string]$Path, [string]$ExpectedParent) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
        return
    }

    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullParent = [IO.Path]::GetFullPath((Split-Path -Parent $fullPath)).TrimEnd('\')
    $allowedParent = [IO.Path]::GetFullPath($ExpectedParent).TrimEnd('\')
    if (-not $fullParent.Equals($allowedParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to move unexpected Docker path: $fullPath"
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $leaf = Split-Path -Leaf $fullPath
    $backupPath = Join-Path $fullParent "$leaf.stale-$stamp"
    Move-Item -LiteralPath $fullPath -Destination $backupPath
    New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
    Write-Host "Recovered stale Docker socket directory (backup kept): $backupPath" -ForegroundColor Yellow
}

function Repair-DockerDesktopSocketState {
    # Docker Desktop on some Windows 25H2 builds can leave broken AF_UNIX
    # reparse points behind. They cannot be deleted individually (Windows error
    # 1920), but rotating only these ephemeral directories is safe and preserves
    # images, volumes, containers and the Metin2 database.
    $dockerProcesses = Get-DockerDesktopProcesses
    if ($dockerProcesses.Count -gt 0) {
        Write-Host 'Docker API is unavailable; waiting for an in-progress startup...' -ForegroundColor DarkYellow
        if (Wait-DockerApi 45) {
            return
        }

        Write-Host 'Docker Desktop is stuck. Stopping only its frontend/backend processes...' -ForegroundColor Yellow
        $dockerProcesses | Stop-Process -Force -ErrorAction SilentlyContinue
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'SilentlyContinue'
            & wsl.exe --terminate docker-desktop 1>$null 2>$null
        }
        finally { $ErrorActionPreference = $previousPreference }
        Start-Sleep -Seconds 3
    }

    $localAppData = [IO.Path]::GetFullPath($env:LOCALAPPDATA)
    $dockerLocalRoot = Join-Path $localAppData 'Docker'
    Move-StaleDockerSocketDirectory `
        -Path (Join-Path $dockerLocalRoot 'run') `
        -ExpectedParent $dockerLocalRoot
    Move-StaleDockerSocketDirectory `
        -Path (Join-Path $localAppData 'docker-secrets-engine') `
        -ExpectedParent $localAppData
}

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw 'Docker CLI was not found. Install Docker Desktop first.'
}

if (-not (Test-DockerApi)) {
    if (-not $NoSocketRecovery) {
        Repair-DockerDesktopSocketState
    }

    if (-not (Test-DockerApi)) {
        # Wrap the pipeline result as an array. Without the outer @(), a single
        # matching path becomes a scalar string and [0] means its first letter
        # ("C") instead of the first path.
        $desktopCandidates = @(
            @(
                (Join-Path $env:ProgramFiles 'Docker\Docker\Docker Desktop.exe'),
                (Join-Path ${env:ProgramFiles(x86)} 'Docker\Docker\Docker Desktop.exe'),
                (Join-Path $env:LOCALAPPDATA 'Docker\Docker Desktop.exe')
            ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
        )
        if ($desktopCandidates.Count -eq 0) {
            throw 'Docker Desktop executable was not found.'
        }
        Start-Process -FilePath $desktopCandidates[0] -WindowStyle Hidden
    }

    Write-Host 'Waiting for Docker Engine...' -ForegroundColor Cyan
    if (-not (Wait-DockerApi $DockerTimeoutSeconds)) {
        throw "Docker Engine did not become ready within $DockerTimeoutSeconds seconds."
    }
}

if ($DockerOnly) {
    Write-Host 'Docker Engine is ready.' -ForegroundColor Green
    return
}

# Identity is resolved only after Docker is reachable. That lets a fresh
# All-in-One launcher discover a stack created by the older install.ps1 and
# attach to its existing named volumes instead of inventing a second project.
Initialize-InstallationIdentity

if ($IdentityOnly) {
    Write-Host 'Installation identity is ready.' -ForegroundColor Green
    return
}

Assert-NoForeignPortOwners

$composeDirectory = Join-Path $PSScriptRoot 'linux-port\docker'
$composeFile = Join-Path $composeDirectory 'docker-compose.yml'
if (-not (Test-Path -LiteralPath $composeFile)) {
    throw "Compose file was not found: $composeFile"
}

# The build context under linux-port/docker/game/src is a staged copy of the
# overlay sources, and nothing on a player's machine keeps it current:
# prepare-context.sh needs the pristine engine tree, which the distribution
# does not ship. An update that adds a source file therefore compiled against
# whatever the copy happened to hold - in 1.23.2, against a missing header, and
# the build stopped with "playerbot_types.h: No such file or directory".
$overlaySource = Join-Path $PSScriptRoot 'linux-port\overlays\playerbot\src\game\src'
$overlayStaged = Join-Path $PSScriptRoot 'linux-port\docker\game\src\server\game\src'
if ((Test-Path -LiteralPath $overlaySource -PathType Container) -and
    (Test-Path -LiteralPath $overlayStaged -PathType Container)) {
    $syncedFiles = 0
    foreach ($overlayFile in Get-ChildItem -LiteralPath $overlaySource -File) {
        $stagedFile = Join-Path $overlayStaged $overlayFile.Name
        $stagedHash = $null
        if (Test-Path -LiteralPath $stagedFile -PathType Leaf) {
            $stagedHash = (Get-FileHash -LiteralPath $stagedFile -Algorithm SHA256).Hash
        }
        if ($stagedHash -ne (Get-FileHash -LiteralPath $overlayFile.FullName -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $overlayFile.FullName -Destination $stagedFile -Force
            $syncedFiles++
        }
    }
    # prepare-context.sh patches the engine Makefile to compile every
    # playerbot_*.cpp it finds, and it never runs on a player's machine. Repair
    # that one line here rather than shipping the whole Makefile over theirs.
    $gameMakefile = Join-Path $PSScriptRoot 'linux-port\docker\game\src\server\game\src\Makefile'
    if (Test-Path -LiteralPath $gameMakefile -PathType Leaf) {
        $makefileText = Get-Content -LiteralPath $gameMakefile -Raw
        if ($makefileText -match '(?m)^CPPFILE \+= playerbot_manager\.cpp\s*$') {
            $makefileText = $makefileText -replace '(?m)^CPPFILE \+= playerbot_manager\.cpp\s*$',
                'CPPFILE += $(wildcard playerbot_*.cpp)'
            [IO.File]::WriteAllText($gameMakefile, $makefileText)
            $syncedFiles++
        }
    }
    if ($syncedFiles -gt 0) {
        Write-Host "Synchronised $syncedFiles playerbot build input(s) into the build context." -ForegroundColor DarkGray
    }
}

Write-Host 'Starting Metin2 services...' -ForegroundColor Cyan
Push-Location $composeDirectory
try {
    $composeArguments = @('compose', 'up', '-d')
    if ($Build) { $composeArguments += '--build' }
    # Windows PowerShell 5.1 promotes a native command's stderr to a terminating
    # NativeCommandError under $ErrorActionPreference='Stop'. `docker compose'
    # writes its ordinary progress ("Container ... Recreate/Started") to stderr,
    # so the start aborted as a false failure even though every container came
    # up. Run the compose calls under 'Continue' and decide success from the
    # real exit code -- the same pattern Test-DockerApi/Invoke-DockerQuery use.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & docker @composeArguments
        $upExitCode = $LASTEXITCODE
        & docker compose ps
        $psExitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($upExitCode -ne 0) {
        throw "docker compose up failed with exit code $upExitCode"
    }
    if ($psExitCode -ne 0) {
        throw "docker compose ps failed with exit code $psExitCode"
    }
}
finally {
    Pop-Location
}

Write-Host 'Metin2 server startup completed.' -ForegroundColor Green
