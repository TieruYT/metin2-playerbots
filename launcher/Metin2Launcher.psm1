Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

function Get-M2DefaultLauncherConfig {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    [pscustomobject]@{
        schema = 1
        manifestUrl = 'https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/update-manifest.json'
        clientRoot = ''
        clientExecutable = ''
        supportUploadUrl = ''
        serverRoot = [IO.Path]::GetFullPath($ServerRoot)
    }
}

function Get-M2LauncherConfig {
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [Parameter(Mandatory = $true)][string]$ConfigPath
    )

    $defaults = Get-M2DefaultLauncherConfig -ServerRoot $ServerRoot
    if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
        return $defaults
    }

    $loaded = Get-Content -LiteralPath $ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($name in @('manifestUrl', 'clientRoot', 'clientExecutable', 'supportUploadUrl')) {
        if ($null -ne $loaded.PSObject.Properties[$name]) {
            $defaults.$name = [string]$loaded.$name
        }
    }
    return $defaults
}

function Save-M2LauncherConfig {
    param(
        [Parameter(Mandatory = $true)]$Config,
        [Parameter(Mandatory = $true)][string]$ConfigPath
    )

    $Config | Select-Object schema, manifestUrl, clientRoot, clientExecutable, supportUploadUrl |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ConfigPath -Encoding UTF8
}

function Get-M2UpdateManifest {
    param([Parameter(Mandatory = $true)][string]$Source)

    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        return Get-Content -LiteralPath $Source -Raw -Encoding UTF8 | ConvertFrom-Json
    }

    $uri = $null
    if (-not [Uri]::TryCreate($Source, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw 'Manifest musi być lokalnym plikiem albo adresem HTTPS.'
    }
    try {
        return Invoke-RestMethod -Uri $uri -Method Get -UseBasicParsing -TimeoutSec 30
    }
    catch {
        $statusCode = 0
        try {
            if ($null -ne $_.Exception.Response) {
                $statusCode = [int]$_.Exception.Response.StatusCode
            }
        }
        catch { $statusCode = 0 }

        # The stable channel may intentionally be empty between releases. A
        # missing manifest must never make the launcher reinstall the server,
        # create another Compose project or touch the user's database.
        if ($statusCode -eq 404) {
            return [pscustomobject]@{
                schema = 1
                channel = 'unavailable'
                publishedAt = $null
                server = $null
                client = $null
                statusMessage = 'Kanał aktualizacji nie został jeszcze opublikowany. Obecna instalacja pozostaje bez zmian.'
            }
        }

        throw "Nie można sprawdzić aktualizacji pod adresem $Source. Sprawdź internet, zaporę i ustawienia DNS. Szczegóły: $($_.Exception.Message)"
    }
}

function Test-M2Sha256 {
    param([Parameter(Mandatory = $true)][string]$Value)
    return $Value -match '^[A-Fa-f0-9]{64}$'
}

function Get-M2Download {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        return
    }

    $uri = $null
    if (-not [Uri]::TryCreate($Source, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw 'Pakiet aktualizacji musi pochodzić z lokalnego pliku albo adresu HTTPS.'
    }
    Invoke-WebRequest -Uri $uri -OutFile $Destination -UseBasicParsing -TimeoutSec 300
}

function Expand-M2SafeZip {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $root = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($entry in $archive.Entries) {
            $relative = $entry.FullName.Replace('/', '\').TrimStart('\')
            if (-not $relative) { continue }
            if ([IO.Path]::IsPathRooted($relative) -or $relative.Split('\') -contains '..') {
                throw "Niedozwolona ścieżka w ZIP: $($entry.FullName)"
            }

            $target = [IO.Path]::GetFullPath((Join-Path $root $relative))
            if (-not $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Plik ZIP wychodzi poza katalog docelowy: $($entry.FullName)"
            }

            if (-not $entry.Name) {
                New-Item -ItemType Directory -Path $target -Force | Out-Null
                continue
            }

            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            $input = $entry.Open()
            try {
                $output = [IO.File]::Open($target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
                try { $input.CopyTo($output) } finally { $output.Dispose() }
            }
            finally { $input.Dispose() }
        }
    }
    finally { $archive.Dispose() }
}

function Test-M2ProtectedPath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $path = $RelativePath.Replace('/', '\').TrimStart('\')
    $protectedFiles = @(
        'linux-port\docker\.env',
        '.m2launcher.json',
        '.m2launcher-state.json',
        '.m2install.json'
    )
    foreach ($protectedFile in $protectedFiles) {
        if ($path.Equals($protectedFile, [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    $protectedDirectories = @('.git', 'backups', 'support-bundles', 'launcher-logs')
    foreach ($protectedDirectory in $protectedDirectories) {
        if ($path.Equals($protectedDirectory, [StringComparison]::OrdinalIgnoreCase) -or
            $path.StartsWith($protectedDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

function Invoke-M2PackageUpdate {
    param(
        [Parameter(Mandatory = $true)]$Component,
        [Parameter(Mandatory = $true)][string]$TargetRoot,
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )

    $url = [string]$Component.url
    $expectedHash = ([string]$Component.sha256).ToUpperInvariant()
    if (-not $url -or -not (Test-M2Sha256 $expectedHash)) {
        throw 'Manifest nie zawiera poprawnego URL i SHA-256 dla tej aktualizacji.'
    }

    $target = [IO.Path]::GetFullPath($TargetRoot).TrimEnd('\')
    if (-not (Test-Path -LiteralPath $target -PathType Container)) {
        throw "Katalog docelowy nie istnieje: $target"
    }

    $tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('m2-update-' + [Guid]::NewGuid().ToString('N'))
    $download = Join-Path $tempRoot 'update.zip'
    $expanded = Join-Path $tempRoot 'expanded'
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    try {
        Get-M2Download -Source $url -Destination $download
        $actualHash = (Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actualHash -ne $expectedHash) {
            throw "Błędna suma SHA-256. Oczekiwano $expectedHash, otrzymano $actualHash."
        }

        Expand-M2SafeZip -ArchivePath $download -Destination $expanded
        $files = @(Get-ChildItem -LiteralPath $expanded -Recurse -File -Force)
        if ($files.Count -eq 0) { throw 'Pakiet aktualizacji jest pusty.' }

        $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
        $backup = Join-Path $BackupRoot ("update-$stamp")
        New-Item -ItemType Directory -Path $backup -Force | Out-Null

        $changes = @()
        foreach ($file in $files) {
            $relative = $file.FullName.Substring($expanded.Length).TrimStart('\')
            if (Test-M2ProtectedPath -RelativePath $relative) {
                throw "Pakiet próbuje zmienić chroniony plik: $relative"
            }
            $destination = [IO.Path]::GetFullPath((Join-Path $target $relative))
            if (-not $destination.StartsWith($target + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw "Niedozwolona ścieżka aktualizacji: $relative"
            }
            $changes += [pscustomobject]@{
                Relative = $relative
                Source = $file.FullName
                Destination = $destination
                Existed = Test-Path -LiteralPath $destination -PathType Leaf
            }
        }

        foreach ($change in $changes) {
            if ($change.Existed) {
                $backupFile = Join-Path $backup $change.Relative
                New-Item -ItemType Directory -Path (Split-Path -Parent $backupFile) -Force | Out-Null
                Copy-Item -LiteralPath $change.Destination -Destination $backupFile -Force
            }
        }

        try {
            foreach ($change in $changes) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $change.Destination) -Force | Out-Null
                Copy-Item -LiteralPath $change.Source -Destination $change.Destination -Force
            }
        }
        catch {
            foreach ($change in $changes) {
                $backupFile = Join-Path $backup $change.Relative
                if (Test-Path -LiteralPath $backupFile -PathType Leaf) {
                    Copy-Item -LiteralPath $backupFile -Destination $change.Destination -Force
                }
                elseif (-not $change.Existed -and (Test-Path -LiteralPath $change.Destination -PathType Leaf)) {
                    Remove-Item -LiteralPath $change.Destination -Force
                }
            }
            throw
        }

        return [pscustomobject]@{
            Version = [string]$Component.version
            Files = $changes.Count
            Backup = $backup
            Sha256 = $actualHash
        }
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}

function Get-M2SanitizedEnv {
    param([Parameter(Mandatory = $true)][string]$EnvPath)

    if (-not (Test-Path -LiteralPath $EnvPath -PathType Leaf)) { return @() }
    return @(Get-Content -LiteralPath $EnvPath | ForEach-Object {
        if ($_ -match '^([A-Za-z_][A-Za-z0-9_]*(?:PASSWORD|SECRET|TOKEN|PRIVATE_KEY)[A-Za-z0-9_]*)=(.*)$') {
            "$($Matches[1])=<redacted>"
        }
        else { $_ }
    })
}

function Protect-M2LogContent {
    param([AllowEmptyString()][string]$Text)
    if (-not $Text) { return '' }

    $safe = $Text
    $safe = [Regex]::Replace(
        $safe,
        '(?im)(password|passwd|secret|token|private[_-]?key)(\s*[:=]\s*)([^\s,;]+)',
        '$1$2<redacted>')
    $safe = [Regex]::Replace(
        $safe,
        '(?im)(M2_[A-Z0-9_]*(?:PASSWORD|SECRET|TOKEN)[A-Z0-9_]*=).*$',
        '$1<redacted>')
    # On the first panel start the generated administrator password is printed
    # on a line of its own, below a heading. It has no "password=" prefix, so
    # the generic key/value rules above cannot recognize it.
    $safe = [Regex]::Replace(
        $safe,
        '(?is)(ADMIN PANEL PASSWORD[^\r\n]*\r?\n\s*\r?\n\s*)[^\r\n]+',
        '$1<redacted>')
    $safe = [Regex]::Replace(
        $safe,
        '(?i)(\b(?:mysql|mariadb)://[^:\s/]+:)[^@\s/]+(@)',
        '$1<redacted>$2')
    return $safe
}

function Invoke-M2CapturedCommand {
    param(
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )

    $previousPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 turns some native stderr lines into error
        # records. Diagnostics should capture those lines, not abort at them.
        $ErrorActionPreference = 'Continue'
        $text = Protect-M2LogContent -Text (& $Command 2>&1 | Out-String)
        [IO.File]::WriteAllText($OutputPath, $text, [Text.UTF8Encoding]::new($false))
    }
    catch {
        [IO.File]::WriteAllText($OutputPath, ($_ | Out-String), [Text.UTF8Encoding]::new($false))
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function New-M2SupportBundle {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $root = [IO.Path]::GetFullPath($ServerRoot).TrimEnd('\')
    $outputDir = Join-Path $root 'support-bundles'
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $work = Join-Path $outputDir ('.work-' + $stamp)
    $zip = Join-Path $outputDir ("metin2-support-$stamp.zip")
    New-Item -ItemType Directory -Path $work -Force | Out-Null

    try {
        $summary = @(
            'Metin2 Playerbots - pakiet diagnostyczny',
            "Utworzono: $([DateTime]::Now.ToString('s'))",
            "PowerShell: $($PSVersionTable.PSVersion)",
            "Windows: $([Environment]::OSVersion.VersionString)",
            "Folder serwera: $([IO.Path]::GetFileName($root))"
        ) -join [Environment]::NewLine
        [IO.File]::WriteAllText((Join-Path $work 'summary.txt'), $summary, [Text.UTF8Encoding]::new($false))

        $versionFile = Join-Path $root 'VERSION'
        if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
            Copy-Item -LiteralPath $versionFile -Destination (Join-Path $work 'server-version.txt') -Force
        }

        $envPath = Join-Path $root 'linux-port\docker\.env'
        $safeEnv = Get-M2SanitizedEnv -EnvPath $envPath
        [IO.File]::WriteAllLines((Join-Path $work 'environment-redacted.txt'), $safeEnv, [Text.UTF8Encoding]::new($false))

        $composeDir = Join-Path $root 'linux-port\docker'
        $composeFile = Join-Path $composeDir 'docker-compose.yml'
        Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'docker-version.txt') -Command { docker version }
        Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'docker-info.txt') -Command { docker info }
        if (Test-Path -LiteralPath $composeFile -PathType Leaf) {
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'compose-ps.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile ps -a
            }
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'compose-logs.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile logs --no-color --tail 800
            }
            Invoke-M2CapturedCommand -OutputPath (Join-Path $work 'compose-services.txt') -Command {
                docker compose --project-directory $composeDir -f $composeFile config --services
            }
        }

        $launcherLogDir = Join-Path $root 'launcher-logs'
        if (Test-Path -LiteralPath $launcherLogDir -PathType Container) {
            $logOutput = Join-Path $work 'launcher-logs'
            New-Item -ItemType Directory -Path $logOutput -Force | Out-Null
            Get-ChildItem -LiteralPath $launcherLogDir -File -Filter '*.log' |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 5 |
                ForEach-Object {
                    $safeLog = Protect-M2LogContent -Text (Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue)
                    [IO.File]::WriteAllText((Join-Path $logOutput $_.Name), $safeLog, [Text.UTF8Encoding]::new($false))
                }
        }

        Compress-Archive -Path (Join-Path $work '*') -DestinationPath $zip -CompressionLevel Optimal -Force
        return $zip
    }
    finally {
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
    }
}

function Send-M2SupportBundle {
    param(
        [Parameter(Mandatory = $true)][string]$BundlePath,
        [Parameter(Mandatory = $true)][string]$UploadUrl
    )

    $uri = $null
    if (-not [Uri]::TryCreate($UploadUrl, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw 'Adres wysyłki logów musi używać HTTPS.'
    }
    if (-not (Test-Path -LiteralPath $BundlePath -PathType Leaf)) {
        throw "Nie znaleziono paczki: $BundlePath"
    }

    Add-Type -AssemblyName System.Net.Http
    $client = [Net.Http.HttpClient]::new()
    $form = [Net.Http.MultipartFormDataContent]::new()
    $stream = [IO.File]::OpenRead($BundlePath)
    try {
        $content = [Net.Http.StreamContent]::new($stream)
        $content.Headers.ContentType = [Net.Http.Headers.MediaTypeHeaderValue]::Parse('application/zip')
        $isDiscordWebhook =
            ($uri.Host -ieq 'discord.com' -or $uri.Host -ieq 'discordapp.com') -and
            $uri.AbsolutePath.StartsWith('/api/webhooks/', [StringComparison]::OrdinalIgnoreCase)
        if ($isDiscordWebhook) {
            $payload = [Net.Http.StringContent]::new(
                '{"content":"Paczka diagnostyczna Metin2 Playerbots (hasła automatycznie usunięte).","allowed_mentions":{"parse":[]}}',
                [Text.Encoding]::UTF8,
                'application/json')
            $form.Add($payload, 'payload_json')
            $form.Add($content, 'files[0]', [IO.Path]::GetFileName($BundlePath))
        }
        else {
            $form.Add($content, 'file', [IO.Path]::GetFileName($BundlePath))
        }
        $response = $client.PostAsync($uri, $form).GetAwaiter().GetResult()
        $body = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        if (-not $response.IsSuccessStatusCode) {
            throw "Serwer pomocy odrzucił paczkę: HTTP $([int]$response.StatusCode) $body"
        }
        return $body
    }
    finally {
        $stream.Dispose()
        $form.Dispose()
        $client.Dispose()
    }
}

# =============================================================================
#  Database import -- copy an existing world (higher-level characters) from
#  another Docker installation's db-data volume into this install. Uses a
#  throwaway MariaDB container with --skip-grant-tables so no volume password
#  needs to be known, dumps the five game databases and reloads them into the
#  target. mysql.* (the game DB user and its grants) is never touched, so the
#  game keeps authenticating with the target install's own password. The source
#  volume is only ever read; the target is backed up before it is replaced.
# =============================================================================

$script:M2_DB_IMAGE = 'mariadb:10.11'
$script:M2_DB_LIST = @('account', 'common', 'player', 'log', 'hotbackup')

function Get-M2DbDataVolumes {
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        $names = New-Object System.Collections.Generic.List[string]
        $labelled = & docker volume ls --filter 'label=com.docker.compose.volume=db-data' --format '{{.Name}}' 2>$null
        if ($LASTEXITCODE -eq 0 -and $labelled) {
            foreach ($n in @($labelled -split '\r?\n' | Where-Object { $_ })) { if (-not $names.Contains($n)) { $names.Add($n) } }
        }
        $all = & docker volume ls --format '{{.Name}}' 2>$null
        if ($LASTEXITCODE -eq 0 -and $all) {
            foreach ($n in @($all -split '\r?\n' | Where-Object { $_ -match '_db-data$' })) { if (-not $names.Contains($n)) { $names.Add($n) } }
        }
        $result = New-Object System.Collections.Generic.List[object]
        foreach ($name in $names) {
            $project = if ($name -match '^(.*)_db-data$') { $Matches[1] } else { $name }
            $result.Add([pscustomobject]@{ Name = $name; Project = $project })
        }
        return $result.ToArray()
    }
    finally { $ErrorActionPreference = $previous }
}

function Start-M2ThrowawayDb {
    param([Parameter(Mandatory = $true)][string]$Volume)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        $container = 'm2dbimp-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
        $null = & docker run -d --name $container -e MARIADB_ALLOW_EMPTY_ROOT_PASSWORD=1 -v "${Volume}:/var/lib/mysql" $script:M2_DB_IMAGE --skip-grant-tables 2>$null
        if ($LASTEXITCODE -ne 0) { throw "Nie udało się uruchomić kontenera bazy dla wolumenu '$Volume' (czy jest zajęty przez działający serwer?)." }
        $deadline = (Get-Date).AddSeconds(120)
        do {
            & docker exec $container sh -c "mariadb -uroot -e 'SELECT 1'" 1>$null 2>$null
            if ($LASTEXITCODE -eq 0) { return $container }
            Start-Sleep -Seconds 2
        } while ((Get-Date) -lt $deadline)
        & docker rm -f $container 1>$null 2>$null
        throw "Baza dla wolumenu '$Volume' nie wystartowała w 120 s."
    }
    finally { $ErrorActionPreference = $previous }
}

function Stop-M2ThrowawayDb {
    param([string]$Container)
    if ($Container) {
        $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        try { & docker rm -f $Container 1>$null 2>$null } finally { $ErrorActionPreference = $previous }
    }
}

function Get-M2VolumeWorldStats {
    param([Parameter(Mandatory = $true)][string]$Volume)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $container = $null
    $created = ''
    try {
        # The volume's own creation time answers "when was this world made?"
        # without touching the data or starting a container.
        $created = [string](& docker volume inspect $Volume --format '{{.CreatedAt}}' 2>$null | Select-Object -First 1)
        $container = Start-M2ThrowawayDb -Volume $Volume
        # -B keeps the columns tab-separated so a last_play datetime (which has a
        # space) is not split apart.
        $out = & docker exec $container sh -c "mariadb -uroot -N -B -e 'SELECT COUNT(*), IFNULL(MAX(level),0), IFNULL(MAX(last_play),0) FROM player.player'" 2>$null
        if ($LASTEXITCODE -eq 0 -and $out) {
            $parts = ($out.ToString().Trim() -split "`t")
            return [pscustomobject]@{
                Players  = [int]$parts[0]
                MaxLevel = [int]$parts[1]
                LastPlay = [string]$parts[2]
                Created  = $created
                Ok       = $true
            }
        }
        return [pscustomobject]@{ Players = 0; MaxLevel = 0; LastPlay = ''; Created = $created; Ok = $false }
    }
    catch { return [pscustomobject]@{ Players = 0; MaxLevel = 0; LastPlay = ''; Created = $created; Ok = $false } }
    finally { Stop-M2ThrowawayDb -Container $container; $ErrorActionPreference = $previous }
}

function Export-M2Database {
    param([string]$Container, [string]$Database, [string]$OutFile)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        $line = "docker exec $Container mariadb-dump -uroot --single-transaction --no-tablespaces --skip-lock-tables $Database > `"$OutFile`""
        & cmd.exe /c $line 2>$null
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutFile)) { throw "Zrzut bazy '$Database' nie powiódł się." }
    }
    finally { $ErrorActionPreference = $previous }
}

function Invoke-M2SqlFile {
    param([string]$Container, [string]$Database, [string]$InFile)
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try {
        $target = if ($Database) { " $Database" } else { '' }
        $line = "docker exec -i $Container mariadb -uroot$target < `"$InFile`""
        & cmd.exe /c $line 2>$null
        if ($LASTEXITCODE -ne 0) { throw "Wczytanie SQL nie powiodło się." }
    }
    finally { $ErrorActionPreference = $previous }
}

function Invoke-M2DatabaseImport {
    param(
        [Parameter(Mandatory = $true)][string]$SourceVolume,
        [Parameter(Mandatory = $true)][string]$TargetVolume,
        [Parameter(Mandatory = $true)][string]$BackupRoot
    )
    if ($SourceVolume -eq $TargetVolume) { throw 'Źródło i cel to ten sam wolumen.' }
    $previous = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $work = Join-Path ([IO.Path]::GetTempPath()) ('m2dbimp-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    $backupDir = Join-Path $BackupRoot "db-import-$stamp"
    New-Item -ItemType Directory -Path (Join-Path $work 'source') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $backupDir 'target-before-import') -Force | Out-Null

    $srcC = $null; $tgtC = $null
    try {
        # 1. Reversible backup of the current target world.
        $tgtC = Start-M2ThrowawayDb -Volume $TargetVolume
        foreach ($db in $script:M2_DB_LIST) {
            $exists = & docker exec $tgtC sh -c "mariadb -uroot -N -B -e `"SELECT 1 FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='$db' LIMIT 1`"" 2>$null
            if ($exists) { Export-M2Database -Container $tgtC -Database $db -OutFile (Join-Path $backupDir "target-before-import\$db.sql") }
        }
        Stop-M2ThrowawayDb -Container $tgtC; $tgtC = $null

        # 2. Dump the source world (read only; the source volume is untouched).
        $srcC = Start-M2ThrowawayDb -Volume $SourceVolume
        foreach ($db in $script:M2_DB_LIST) {
            Export-M2Database -Container $srcC -Database $db -OutFile (Join-Path $work "source\$db.sql")
        }
        Stop-M2ThrowawayDb -Container $srcC; $srcC = $null

        # 3. Replace the five game databases in the target.
        $tgtC = Start-M2ThrowawayDb -Volume $TargetVolume
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.AppendLine('SET FOREIGN_KEY_CHECKS=0;')
        foreach ($db in $script:M2_DB_LIST) {
            [void]$sb.AppendLine("DROP DATABASE IF EXISTS $db;")
            [void]$sb.AppendLine("CREATE DATABASE $db DEFAULT CHARACTER SET latin1 COLLATE latin1_swedish_ci;")
        }
        $createFile = Join-Path $work 'create.sql'
        [IO.File]::WriteAllText($createFile, $sb.ToString(), [Text.UTF8Encoding]::new($false))
        Invoke-M2SqlFile -Container $tgtC -Database '' -InFile $createFile
        foreach ($db in $script:M2_DB_LIST) {
            Invoke-M2SqlFile -Container $tgtC -Database $db -InFile (Join-Path $work "source\$db.sql")
        }
        $stats = & docker exec $tgtC sh -c "mariadb -uroot -N -B -e 'SELECT COUNT(*), IFNULL(MAX(level),0) FROM player.player'" 2>$null
        Stop-M2ThrowawayDb -Container $tgtC; $tgtC = $null

        $players = 0; $maxLevel = 0
        if ($stats) { $p = ($stats.ToString().Trim() -split '\s+'); $players = [int]$p[0]; $maxLevel = [int]$p[1] }
        return [pscustomobject]@{ Backup = $backupDir; Players = $players; MaxLevel = $maxLevel }
    }
    finally {
        if ($srcC) { Stop-M2ThrowawayDb -Container $srcC }
        if ($tgtC) { Stop-M2ThrowawayDb -Container $tgtC }
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
        $ErrorActionPreference = $previous
    }
}

Export-ModuleMember -Function @(
    'Get-M2DefaultLauncherConfig',
    'Get-M2LauncherConfig',
    'Save-M2LauncherConfig',
    'Get-M2UpdateManifest',
    'Invoke-M2PackageUpdate',
    'New-M2SupportBundle',
    'Send-M2SupportBundle',
    'Get-M2DbDataVolumes',
    'Get-M2VolumeWorldStats',
    'Invoke-M2DatabaseImport'
)
