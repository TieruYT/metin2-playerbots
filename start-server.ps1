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
        # A real .env is mostly empty values (M2_BRAND=, M2_CLIENT_URL=...);
        # a Mandatory string refuses '' and the whole start died on the first
        # one while adopting an older installation.
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value
    )
    $pattern = '(?m)^' + [Regex]::Escape($Name) + '=.*$'
    if ([Regex]::IsMatch($Content, $pattern)) {
        return [Regex]::Replace($Content, $pattern, "$Name=$Value")
    }
    if ($Content -and -not $Content.EndsWith("`n")) { $Content += [Environment]::NewLine }
    return $Content + "$Name=$Value" + [Environment]::NewLine
}

function Add-MissingDotEnvKeys {
    # A player's .env is written once and never rewritten: what is in it is
    # theirs, including passwords nobody else has a copy of. So a switch added
    # to .env.example after they installed never appears for them, and telling
    # them to "set M2_PLAYERBOT_KINGDOMS=1 in .env" is advice about a line that
    # is not there. That is how the three kingdoms looked broken on an install
    # which had been updated rather than made fresh (jaksiezabic, 1.32.1):
    # Compose still had its own default, so nothing failed - the player simply
    # had no way to turn the feature on.
    #
    # Only ABSENT keys are added, always with the example's own default, and a
    # line that already exists is never touched. Secrets are skipped whatever
    # happens: a placeholder quietly landing next to a real password is a far
    # worse failure than a missing switch.
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content,
        [Parameter(Mandatory = $true)][string]$ExamplePath
    )
    if (-not (Test-Path -LiteralPath $ExamplePath -PathType Leaf)) { return $Content }
    $added = New-Object System.Collections.Generic.List[string]
    foreach ($line in [IO.File]::ReadAllLines($ExamplePath)) {
        $match = [Regex]::Match($line, '^\s*([A-Za-z0-9_]+)=(.*)$')
        if (-not $match.Success) { continue }
        $name = $match.Groups[1].Value
        if ($name -match 'PASSWORD|SECRET|TOKEN|_KEY$') { continue }
        # An empty value in the example means "leave it to Compose", and every
        # place that reads one uses ${VAR:-default}, which falls back on an
        # empty value too. Writing the empty line would gain nothing and would
        # put a setting in front of the player that has no meaning on its own -
        # M2_SEBAN_TIERU_PANEL_URL is the one that made this worth a rule.
        if (-not $match.Groups[2].Value.Trim()) { continue }
        if ([Regex]::IsMatch($Content, '(?m)^' + [Regex]::Escape($name) + '=')) { continue }
        $Content = Set-DotEnvValue -Content $Content -Name $name -Value $match.Groups[2].Value
        $added.Add($name)
    }
    if ($added.Count -gt 0) {
        Write-Host ("Dopisano do .env brakujace ustawienia: " + ($added -join ', ')) -ForegroundColor DarkGray
    }
    return $Content
}

function Get-ServerEngine {
    # Which engine sits under linux-port\docker - see Get-M2ServerEngine in
    # launcher\Metin2Launcher.psm1. This script is standalone and imports no
    # module, so it reads the same marker itself.
    $marker = Join-Path $PSScriptRoot 'linux-port\docker\ENGINE'
    if (Test-Path -LiteralPath $marker -PathType Leaf) {
        $engine = ([IO.File]::ReadAllText($marker)).Trim().ToLowerInvariant()
        if ($engine -match '^[a-z0-9]+$') { return $engine }
    }
    return 'r40250'
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

function Get-InstallationEngine {
    # The engine of another installation, read the way Get-ServerEngine reads
    # ours: an ENGINE file in its linux-port\docker, r40250 when there is none.
    param([AllowEmptyString()][string]$DockerDirectory)
    if (-not $DockerDirectory) { return 'r40250' }
    $marker = Join-Path $DockerDirectory 'ENGINE'
    if (Test-Path -LiteralPath $marker -PathType Leaf) {
        $engine = ([IO.File]::ReadAllText($marker)).Trim().ToLowerInvariant()
        if ($engine -match '^[a-z0-9]+$') { return $engine }
    }
    return 'r40250'
}

function Select-SameEngineInstallations {
    # Only a stack of the same engine can be adopted. An mt2009 tree taking
    # over an r40250 project would start against a database volume whose
    # schema it cannot use (no `world', another log layout), and initdb would
    # never run because the volume is already initialised - which is exactly
    # what a player coming from the r40250 line would hit, their old install
    # being the one existing stack on the PC. Another line's stack is simply
    # somebody else's server, side by side, and never counts as ambiguity.
    param([object[]]$Candidates)
    $mine = Get-ServerEngine
    return @($Candidates | Where-Object {
        $directory = if ($_.environmentPath) { Split-Path -Parent ([string]$_.environmentPath) } else { [string]$_.workingDirectory }
        (Get-InstallationEngine -DockerDirectory $directory) -eq $mine
    })
}

function Find-CompatibleDockerInstallation {
    $candidates = @(Select-SameEngineInstallations -Candidates @(Get-CompatibleDockerInstallations))
    if ($candidates.Count -eq 0) { $candidates = @(Select-SameEngineInstallations -Candidates @(Get-CompatibleDockerVolumes)) }
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
            # What the old file left blank must not blank what the new one
            # has - the passwords and addresses are the point of the merge.
            if ($Matches[2].Trim() -eq '') { continue }
            $Content = Set-DotEnvValue -Content $Content -Name $name -Value $Matches[2]
        }
    }
    return $Content
}

function New-DotEnvSecret {
    # Read only by the server, never typed: hex, so it can never hold a space
    # or a quote that the game's config parser would split on.
    $bytes = New-Object byte[] 24
    $rng = [System.Security.Cryptography.RNGCryptoServiceProvider]::new()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

function New-DotEnvPassphrase {
    # Read off the screen and typed into a browser, so the alphabet leaves out
    # the characters people confuse: 0/O and 1/l/I.
    $alphabet = 'abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789'
    $bytes = New-Object byte[] 20
    $rng = [System.Security.Cryptography.RNGCryptoServiceProvider]::new()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    $sb = New-Object System.Text.StringBuilder
    foreach ($b in $bytes) { [void]$sb.Append($alphabet[$b % $alphabet.Length]) }
    return $sb.ToString()
}

function Write-FileDurable {
    # WriteAllText leaves the bytes in the cache until Windows gets round to
    # them, and a machine that loses its power first comes back with the
    # file's length and zeros where its bytes were. That is what Greess's .env
    # was after a crash in the middle of an update (19 September): 21 395 zero
    # bytes with the database's passwords among them, and the launcher's own
    # log with the same hole at the same minute. Written to a file beside it,
    # flushed to the disk and only then swapped in, the old file stays whole
    # until the new one is.
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Content)
    $temp = $Path + '.tmp'
    $stream = [IO.FileStream]::new($temp, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally { $stream.Dispose() }
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        try { [IO.File]::Replace($temp, $Path, $null) }
        catch {
            [IO.File]::Copy($temp, $Path, $true)
            Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
        }
    }
    else {
        [IO.File]::Move($temp, $Path)
    }
}

function Test-FileZeroFilled {
    # A text file with a NUL byte in it is one whose length reached the disk
    # and whose bytes did not; nothing this launcher writes ever holds one.
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][byte[]]$Bytes)
    return ([Array]::IndexOf($Bytes, [byte]0) -ge 0)
}

function Get-DotEnvFromContainers {
    # What Compose put into this installation's containers when it last
    # started them: every key of the example that a container carries, the
    # root password under the database image's own name, and the two bind
    # addresses from where the ports were published. A start that cannot read
    # .env recreates nothing, so after a crash these are the values the file
    # held.
    param(
        [Parameter(Mandatory = $true)][string]$Project,
        [Parameter(Mandatory = $true)][string[]]$Keys
    )
    $values = @{}
    $known = @{}
    foreach ($key in $Keys) { $known[$key] = $true }
    $listed = Invoke-DockerQuery @('ps', '-a', '--filter', "label=com.docker.compose.project=$Project", '--format', '{{.ID}}')
    if ($listed.ExitCode -ne 0 -or -not $listed.Output) { return $values }
    foreach ($id in @($listed.Output -split '\s+' | Where-Object { $_ })) {
        $inspection = Invoke-DockerQuery @('inspect', $id)
        if ($inspection.ExitCode -ne 0 -or -not $inspection.Output) { continue }
        try { $container = @($inspection.Output | ConvertFrom-Json)[0] }
        catch { continue }
        foreach ($entry in @($container.Config.Env)) {
            $text = [string]$entry
            $at = $text.IndexOf('=')
            if ($at -lt 1) { continue }
            $name = $text.Substring(0, $at)
            $value = $text.Substring($at + 1)
            if (-not $value) { continue }
            if ($name -eq 'MARIADB_ROOT_PASSWORD') { $name = 'M2_DB_ROOT_PASSWORD' }
            elseif ($name -eq 'TZ') { $name = 'M2_TZ' }
            if (-not $known.ContainsKey($name) -or $values.ContainsKey($name)) { continue }
            $values[$name] = $value
        }
        $service = Get-ObjectPropertyValue $container.Config.Labels 'com.docker.compose.service'
        $bindKey = ''
        if ($service -eq 'game') { $bindKey = 'M2_HOST_BIND_ADDRESS' }
        elseif ($service -eq 'panel') { $bindKey = 'M2_PANEL_BIND_ADDRESS' }
        if (-not $bindKey -or $values.ContainsKey($bindKey) -or $null -eq $container.HostConfig.PortBindings) { continue }
        foreach ($property in $container.HostConfig.PortBindings.PSObject.Properties) {
            foreach ($binding in @($property.Value)) {
                $ip = [string]$binding.HostIp
                $parsed = $null
                if ($ip -and [Net.IPAddress]::TryParse($ip, [ref]$parsed)) {
                    $values[$bindKey] = $ip
                    break
                }
            }
            if ($values.ContainsKey($bindKey)) { break }
        }
    }
    return $values
}

function Repair-DotEnvAfterCrash {
    # Compose cannot read a line of a zero-filled .env, and the passwords of a
    # database that already exists were in it and nowhere else, so a fresh file
    # would lock the world out for good. The damaged file is kept beside it;
    # the copy the last successful start left (.env.last-good) goes back if
    # there is one; otherwise what survived the zeros is kept and the values
    # the containers still carry go over it - they win, because the lines
    # after the zeros are what an older launcher appended from the example to
    # a file it could no longer read (a fresh panel password among them).
    param(
        [Parameter(Mandatory = $true)][string]$EnvPath,
        [AllowEmptyString()][string]$Project
    )
    if (-not (Test-Path -LiteralPath $EnvPath -PathType Leaf)) { return }
    $bytes = [IO.File]::ReadAllBytes($EnvPath)
    if (-not (Test-FileZeroFilled -Bytes $bytes)) { return }
    $damaged = $EnvPath + '.damaged-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
    [IO.File]::Copy($EnvPath, $damaged, $true)
    Write-Host "Plik .env jest uszkodzony: zamiast tresci ma zera, jak po naglym wylaczeniu komputera w trakcie zapisu. Kopia uszkodzonego pliku: $damaged" -ForegroundColor Yellow

    $lastGood = $EnvPath + '.last-good'
    if (Test-Path -LiteralPath $lastGood -PathType Leaf) {
        $good = [IO.File]::ReadAllBytes($lastGood)
        if ($good.Length -gt 0 -and -not (Test-FileZeroFilled -Bytes $good)) {
            $text = [Text.UTF8Encoding]::new($false).GetString($good)
            if ((Get-DotEnvValue -Content $text -Name 'M2_DB_ROOT_PASSWORD') -and
                    (Get-DotEnvValue -Content $text -Name 'M2_DB_PASSWORD')) {
                Write-FileDurable -Path $EnvPath -Content $text
                Write-Host 'Przywrocono .env z kopii z ostatniego udanego startu (.env.last-good).' -ForegroundColor Green
                return
            }
        }
    }

    $text = [Text.UTF8Encoding]::new($false).GetString($bytes).Replace([string][char]0, '')
    # Ordinal, not StartsWith: a culture-sensitive comparison ignores U+FEFF,
    # so every string "starts with" it and the first letter went instead.
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF) { $text = $text.Substring(1) }
    if (-not $Project -and $text -match '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$') {
        $Project = $Matches[1]
    }
    if (-not $Project) {
        # Both halves of the identity gone: the containers still say which
        # project was started from this folder.
        # (No Go template with quotes in it: PowerShell 5.1 would end the
        # argument at the first one.)
        $composeDirectory = Split-Path -Parent $EnvPath
        $owners = Invoke-DockerQuery @('ps', '-a', '--filter', "label=com.docker.compose.project.working_dir=$composeDirectory",
            '--format', '{{.ID}}')
        if ($owners.ExitCode -eq 0 -and $owners.Output) {
            $first = @($owners.Output -split '\s+' | Where-Object { $_ })[0]
            $inspection = Invoke-DockerQuery @('inspect', $first)
            if ($inspection.ExitCode -eq 0 -and $inspection.Output) {
                try {
                    $Project = Get-ObjectPropertyValue (@($inspection.Output | ConvertFrom-Json)[0]).Config.Labels 'com.docker.compose.project'
                }
                catch { $Project = '' }
            }
        }
    }
    $keys = @()
    $example = Join-Path (Split-Path -Parent $EnvPath) '.env.example'
    if (Test-Path -LiteralPath $example -PathType Leaf) {
        foreach ($line in [IO.File]::ReadAllLines($example)) {
            $match = [Regex]::Match($line, '^\s*([A-Za-z0-9_]+)=')
            if ($match.Success) { $keys += $match.Groups[1].Value }
        }
    }
    $recovered = @()
    if ($Project -and $keys.Count -gt 0) {
        $values = Get-DotEnvFromContainers -Project $Project -Keys $keys
        foreach ($name in @($values.Keys | Sort-Object)) {
            $text = Set-DotEnvValue -Content $text -Name $name -Value ([string]$values[$name])
            $recovered += $name
        }
    }
    if ($recovered.Count -gt 0) {
        Write-Host ('Odzyskano z kontenerow serwera: ' + ($recovered -join ', ')) -ForegroundColor Green
    }
    if (-not (Get-DotEnvValue -Content $text -Name 'M2_DB_ROOT_PASSWORD') -or
            -not (Get-DotEnvValue -Content $text -Name 'M2_DB_PASSWORD')) {
        # New passwords only where no database can be holding the old ones.
        $volumeExists = $true
        if ($Project) {
            $volume = Invoke-DockerQuery @('volume', 'inspect', "${Project}_db-data")
            $volumeExists = ($volume.ExitCode -eq 0)
        }
        if ($volumeExists) {
            throw ("Plik .env jest uszkodzony, a hasel do bazy serwera nie udalo sie odzyskac z kontenerow. " +
                "Przywroc .env z kopii (folder backups albo inny folder z serwerem). Uszkodzony plik: $damaged")
        }
        $text = Set-DotEnvValue -Content $text -Name 'M2_DB_ROOT_PASSWORD' -Value (New-DotEnvSecret)
        $text = Set-DotEnvValue -Content $text -Name 'M2_DB_PASSWORD' -Value (New-DotEnvSecret)
    }
    Write-FileDurable -Path $EnvPath -Content $text
    Write-Host 'Plik .env naprawiony.' -ForegroundColor Green
}

function Initialize-DotEnvFile {
    param([Parameter(Mandatory = $true)][string]$EnvPath)
    # The installer writes this file. A copy unpacked by hand from the
    # repository has none, and every launcher action used to stop here - and
    # the rebuild after an update ran compose without it, so the update could
    # never finish either. Seed it the way the installer would: the example,
    # fresh passwords, everything bound to localhost.
    $statePath = Join-Path $PSScriptRoot '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        # An identity without its .env means the passwords are gone, and a
        # database volume built with them would reject fresh ones forever.
        # That is the one case a new file cannot fix; say so instead.
        $known = ''
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $known = [string]$state.projectName
        }
        catch { $known = '' }
        if ($known) {
            $volume = Invoke-DockerQuery @('volume', 'inspect', "${known}_db-data")
            if ($volume.ExitCode -eq 0) {
                throw ("Brak pliku $EnvPath, a baza serwera '$known' juz istnieje. Hasla do niej byly tylko w tym pliku. " +
                    "Przywroc .env z kopii (katalog backups lub inny folder), albo zacznij od nowa: usuniecie bazy kasuje wszystkie postacie.")
            }
        }
    }

    $example = Join-Path (Split-Path -Parent $EnvPath) '.env.example'
    $content = ''
    if (Test-Path -LiteralPath $example -PathType Leaf) {
        $content = [IO.File]::ReadAllText($example)
    }
    $panelPassword = New-DotEnvPassphrase
    $content = Set-DotEnvValue -Content $content -Name 'M2_DB_ROOT_PASSWORD' -Value (New-DotEnvSecret)
    $content = Set-DotEnvValue -Content $content -Name 'M2_DB_PASSWORD' -Value (New-DotEnvSecret)
    $content = Set-DotEnvValue -Content $content -Name 'M2_PANEL_PASSWORD' -Value $panelPassword
    $content = Set-DotEnvValue -Content $content -Name 'M2_ADMINPAGE_PASSWORD' -Value (New-DotEnvSecret)
    foreach ($name in @('M2_PUBLIC_ADDRESS', 'M2_CLIENT_ADDRESS', 'M2_HOST_BIND_ADDRESS', 'M2_PANEL_BIND_ADDRESS')) {
        $content = Set-DotEnvValue -Content $content -Name $name -Value '127.0.0.1'
    }
    Write-FileDurable -Path $EnvPath -Content $content
    Write-Host "Nie bylo pliku .env (instalator nie byl uruchamiany) - utworzono go z nowymi haslami." -ForegroundColor Yellow
    Write-Host "Haslo do panelu administracyjnego: $panelPassword" -ForegroundColor Yellow
    Write-Host "Zapisz je. Jest tez w pliku linux-port\docker\.env (M2_PANEL_PASSWORD)." -ForegroundColor Yellow
}

function Get-DockerDesktopCandidates {
    # The three stock folders, then wherever the CLI on PATH lives (Docker
    # Desktop keeps docker.exe under <install>\resources\bin), then the
    # uninstall entry's InstallLocation. Two players had Docker on another
    # drive: the launcher stopped Docker Desktop for them and then could not
    # start it again, and the update that happened to come between the two
    # got the blame.
    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($p in @(
            (Join-Path $env:ProgramFiles 'Docker\Docker\Docker Desktop.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'Docker\Docker\Docker Desktop.exe'),
            (Join-Path $env:LOCALAPPDATA 'Docker\Docker Desktop.exe'))) {
        if ($p) { $paths.Add($p) }
    }
    try {
        $cli = (Get-Command docker -ErrorAction Stop).Source
        if ($cli) {
            $root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $cli))
            if ($root) { $paths.Add((Join-Path $root 'Docker Desktop.exe')) }
        }
    }
    catch { }
    foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop',
                       'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop')) {
        try {
            $loc = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).InstallLocation
            if ($loc) { $paths.Add((Join-Path $loc 'Docker Desktop.exe')) }
        }
        catch { }
    }
    return @($paths | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Unique)
}

function Assert-KingdomsDefault {
    # Every 2.x world is meant to run all three kingdoms (the operator's call:
    # "istotne, by tak bylo u kazdego"), but a .env is written once and kept,
    # so every install made before 2.0.8 carries the old default
    # M2_PLAYERBOT_KINGDOMS=0 and would stay a Chunjo-only world for ever.
    # The switch is flipped to 1 exactly once, and M2_PLAYERBOT_KINGDOMS_DEFAULTED
    # records that it was - an operator who sets 0 again afterwards keeps 0.
    # Only on the mt2009 line: the r40250 tree keeps its opt-in.
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content,
        [Parameter(Mandatory = $true)][string]$EnvPath
    )
    $marker = Join-Path (Split-Path -Parent $EnvPath) 'ENGINE'
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { return $Content }
    if ((Get-Content -LiteralPath $marker -Raw).Trim() -eq 'r40250') { return $Content }
    if ([Regex]::IsMatch($Content, '(?m)^M2_PLAYERBOT_KINGDOMS_DEFAULTED=')) { return $Content }
    $current = [Regex]::Match($Content, '(?m)^M2_PLAYERBOT_KINGDOMS=(.*)$')
    if ($current.Success -and $current.Groups[1].Value.Trim() -ne '1') {
        Write-Host 'Trzy krolestwa: M2_PLAYERBOT_KINGDOMS przelaczone na 1 (Shinsoo, Chunjo i Jinno; boty dzielone po rowno).' -ForegroundColor Cyan
        Write-Host '  Przy tym starcie migrator dosieje boty dwoch nowych krolestw - to potrwa chwile dluzej.' -ForegroundColor Gray
    }
    $Content = Set-DotEnvValue -Content $Content -Name 'M2_PLAYERBOT_KINGDOMS' -Value '1'
    return (Set-DotEnvValue -Content $Content -Name 'M2_PLAYERBOT_KINGDOMS_DEFAULTED' -Value '1')
}

function Assert-WorldLayoutDefault {
    # Split is three kingdoms on three cores, and a bot has no client, so it
    # cannot cross between them: every shared map - Orc Valley, the desert,
    # Sohan, both Spider Dungeons - is Chunjo's core, and Shinsoo and Jinno
    # wedge at about thirty-six. The answer has been an operator switch since
    # 2.0.30 and hardly anybody knew of it: on 19 September players were
    # passing each other screenshots of the line to paste into .env by hand,
    # and Iwakura's word on it was "unified powinno byc domyslnie tbh".
    #
    # So it is, once, the way the three kingdoms were: unified unless this
    # world is big enough to want the parallelism back. One core carrying
    # everything was measured at 9.4 s of every 60 at 1500 bots, so a world
    # asking for more than that keeps split and is left alone.
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content,
        [Parameter(Mandatory = $true)][string]$EnvPath
    )
    $marker = Join-Path (Split-Path -Parent $EnvPath) 'ENGINE'
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { return $Content }
    if ((Get-Content -LiteralPath $marker -Raw).Trim() -eq 'r40250') { return $Content }
    if ([Regex]::IsMatch($Content, '(?m)^M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=')) { return $Content }
    # How big this world is. Since 2.0.83 an operator may ask per kingdom
    # instead of once, and then PLAYERBOT_AUTOSPAWN_COUNT says nothing about
    # the size - three times seven hundred is the world one core would carry.
    $bots = 0
    $perKingdom = [Regex]::Match($Content, '(?m)^PLAYERBOT_AUTOSPAWN_PER_KINGDOM=(.*)$')
    if ($perKingdom.Success -and $perKingdom.Groups[1].Value.Trim() -eq '1') {
        foreach ($key in @('PLAYERBOT_AUTOSPAWN_SHINSOO', 'PLAYERBOT_AUTOSPAWN_CHUNJO', 'PLAYERBOT_AUTOSPAWN_JINNO')) {
            $one = 0
            $match = [Regex]::Match($Content, ('(?m)^' + $key + '=(.*)$'))
            if ($match.Success) { [int]::TryParse($match.Groups[1].Value.Trim(), [ref]$one) | Out-Null }
            $bots += $one
        }
    } else {
        $count = [Regex]::Match($Content, '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=(.*)$')
        if ($count.Success) { [int]::TryParse($count.Groups[1].Value.Trim(), [ref]$bots) | Out-Null }
    }
    if ($bots -gt 1500) {
        Write-Host "Uklad swiata: zostaje split - ten swiat prosi o $bots botow, a przy takiej liczbie jeden rdzen bylby za wolny." -ForegroundColor Gray
        return (Set-DotEnvValue -Content $Content -Name 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED' -Value '1')
    }
    $current = [Regex]::Match($Content, '(?m)^M2_PLAYERBOT_WORLD_LAYOUT=(.*)$')
    if (-not $current.Success -or $current.Groups[1].Value.Trim() -ne 'unified') {
        Write-Host 'Uklad swiata: M2_PLAYERBOT_WORLD_LAYOUT przelaczony na unified.' -ForegroundColor Cyan
        Write-Host '  Wszystkie trzy krolestwa i caly front na jednym rdzeniu, wiec boty Shinsoo i Jinno' -ForegroundColor Gray
        Write-Host '  przestaja konczyc na ~36 poziomie. Wroc na split w .env, jesli wolisz po staremu.' -ForegroundColor Gray
    }
    $Content = Set-DotEnvValue -Content $Content -Name 'M2_PLAYERBOT_WORLD_LAYOUT' -Value 'unified'
    return (Set-DotEnvValue -Content $Content -Name 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED' -Value '1')
}

function Get-M2HostTimeZoneName {
    # The tz database name of this Windows' own zone, for the containers' TZ.
    # Windows keeps ids of its own ("Central European Standard Time") and the
    # .NET under Windows PowerShell 5.1 has no converter to the tz database, so
    # the zones players here are likely to have are named below. Anything else
    # becomes a fixed offset, Etc/GMT-N - the right hour today, without the
    # summer change - and a zone off the whole hour is left alone.
    $zones = @{
        'Central European Standard Time' = 'Europe/Warsaw'
        'Central Europe Standard Time'   = 'Europe/Budapest'
        'W. Europe Standard Time'        = 'Europe/Berlin'
        'Romance Standard Time'          = 'Europe/Paris'
        'GMT Standard Time'              = 'Europe/London'
        'Greenwich Standard Time'        = 'Atlantic/Reykjavik'
        'GTB Standard Time'              = 'Europe/Bucharest'
        'FLE Standard Time'              = 'Europe/Kiev'
        'E. Europe Standard Time'        = 'Europe/Chisinau'
        'Belarus Standard Time'          = 'Europe/Minsk'
        'Russian Standard Time'          = 'Europe/Moscow'
        'Turkey Standard Time'           = 'Europe/Istanbul'
        'Eastern Standard Time'          = 'America/New_York'
        'Central Standard Time'          = 'America/Chicago'
        'Mountain Standard Time'         = 'America/Denver'
        'Pacific Standard Time'          = 'America/Los_Angeles'
        'UTC'                            = 'UTC'
    }
    $local = [TimeZoneInfo]::Local
    if ($zones.ContainsKey($local.Id)) { return $zones[$local.Id] }
    $offset = $local.BaseUtcOffset
    if ($offset.Minutes -ne 0) { return '' }
    $hours = [int]$offset.TotalHours
    if ($hours -eq 0) { return 'UTC' }
    # The Etc/GMT names carry the sign the other way round: UTC+1 is Etc/GMT-1.
    if ($hours -gt 0) { return ('Etc/GMT-' + $hours) }
    return ('Etc/GMT+' + (-$hours))
}

function Assert-TimezoneDefault {
    # Every container takes its clock's zone from M2_TZ, and .env.example has
    # always said UTC - so a Polish player's panel showed every time two hours
    # behind the machine it runs on ("czas jest cofniety o dwie godziny",
    # hunmar, 14 September), and the logs were named by an hour nobody lives
    # in. The example's UTC is replaced by this machine's own zone exactly
    # once, and M2_TZ_DEFAULTED records that it was: an operator who sets UTC,
    # or anything else, afterwards keeps it. A zone other than UTC already in
    # the file is the operator's and is only marked.
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )
    if ([Regex]::IsMatch($Content, '(?m)^M2_TZ_DEFAULTED=')) { return $Content }
    $current = [Regex]::Match($Content, '(?m)^M2_TZ=(.*)$')
    $value = ''
    if ($current.Success) { $value = $current.Groups[1].Value.Trim() }
    if ($value -and $value -ne 'UTC') {
        return (Set-DotEnvValue -Content $Content -Name 'M2_TZ_DEFAULTED' -Value '1')
    }
    $zone = ''
    try { $zone = Get-M2HostTimeZoneName } catch { $zone = '' }
    if (-not $zone) { return $Content }
    if ($zone -ne 'UTC') {
        Write-Host ('Strefa czasowa serwera: ' + $zone + ' (jak w Windows) - panel i logi pokaza godzine z Twojego zegara.') -ForegroundColor Cyan
    }
    $Content = Set-DotEnvValue -Content $Content -Name 'M2_TZ' -Value $zone
    return (Set-DotEnvValue -Content $Content -Name 'M2_TZ_DEFAULTED' -Value '1')
}

function Assert-PanelPassphrase {
    # The one password an operator actually types, and the one way it can go
    # missing.
    #
    # .env.example ships M2_PANEL_PASSWORD empty. The installer fills it in and
    # prints it at the end, but every other route to a .env leaves it empty - a
    # file copied from the example by hand, an interrupted install, a stack
    # started with `docker compose up` directly. The panel container then does
    # what its entrypoint has always done: invents a twenty-character password,
    # writes only its PBKDF2 hash into m2panel.conf, and prints the plaintext
    # once to a container log nobody reads. From then on the panel has a
    # password that exists nowhere: "ja nie mam zadnego hasla nawet w panelu
    # tieru", "przy czystej instalacji losuje haslo".
    #
    # So the launcher fills the blank before Compose ever sees it. Written to
    # .env, where the operator can read it back, and said out loud once here.
    #
    # An .env that already carries one is never touched - it may be the hash in
    # m2panel.conf, and overwriting it would lock the operator out for real.
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content,
        [ref]$Generated
    )
    $match = [Regex]::Match($Content, '(?m)^M2_PANEL_PASSWORD=(.*)$')
    if ($match.Success -and $match.Groups[1].Value.Trim()) { return $Content }
    $passphrase = New-DotEnvPassphrase
    if ($Generated) { $Generated.Value = $passphrase }
    return (Set-DotEnvValue -Content $Content -Name 'M2_PANEL_PASSWORD' -Value $passphrase)
}

function Initialize-InstallationIdentity {
    $envPath = Join-Path $PSScriptRoot 'linux-port\docker\.env'
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        Initialize-DotEnvFile -EnvPath $envPath
    }
    $statePath = Join-Path $PSScriptRoot '.m2install.json'
    $project = ''
    $prefix = ''
    $createdAt = ''
    $migratedFrom = ''
    $databaseVolume = ''

    if ((Test-Path -LiteralPath $statePath -PathType Leaf) -and
            (Test-FileZeroFilled -Bytes ([IO.File]::ReadAllBytes($statePath)))) {
        # The same crash as the .env's: the project is read back from .env or
        # from the containers instead.
        Move-Item -LiteralPath $statePath -Destination ($statePath + '.damaged-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) -Force
        Write-Host 'Plik .m2install.json byl uszkodzony (zera zamiast tresci) - odtwarzam go.' -ForegroundColor Yellow
    }
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

    Repair-DotEnvAfterCrash -EnvPath $envPath -Project $project
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
    Write-FileDurable -Path $statePath -Content ($stateObject | ConvertTo-Json)
    $content = Set-DotEnvValue -Content $content -Name 'M2_COMPOSE_PROJECT_NAME' -Value $project
    $content = Set-DotEnvValue -Content $content -Name 'M2_CONTAINER_PREFIX' -Value $prefix
    # Before the example's keys are added, because the marker it sets is one
    # of them: an older .env is switched to all three kingdoms exactly once.
    $content = Assert-KingdomsDefault -Content $content -EnvPath $envPath
    # And, the same shape again, the world those three kingdoms live on: one
    # core unless this world is too big for one.
    $content = Assert-WorldLayoutDefault -Content $content -EnvPath $envPath
    # The same shape for the clock's zone: the example's UTC becomes this
    # machine's own, once.
    $content = Assert-TimezoneDefault -Content $content
    # Last, so anything the identity decides above wins over the example.
    $content = Add-MissingDotEnvKeys -Content $content -ExamplePath (
        Join-Path (Split-Path -Parent $envPath) '.env.example')
    # And after that, because Add-MissingDotEnvKeys deliberately never touches a
    # PASSWORD key - which is right for not clobbering one, and leaves an empty
    # one empty.
    $panelGenerated = ''
    $content = Assert-PanelPassphrase -Content $content -Generated ([ref]$panelGenerated)
    Write-FileDurable -Path $envPath -Content $content
    # The copy Repair-DotEnvAfterCrash puts back first: the file as this
    # start leaves it, once it holds the database's passwords.
    if ((Get-DotEnvValue -Content $content -Name 'M2_DB_ROOT_PASSWORD') -and
            (Get-DotEnvValue -Content $content -Name 'M2_DB_PASSWORD')) {
        Write-FileDurable -Path ($envPath + '.last-good') -Content $content
    }
    if ($panelGenerated) {
        Write-Host ''
        Write-Host '=============================================================' -ForegroundColor Yellow
        Write-Host '  HASLO DO PANELU WWW (wygenerowane, bo w .env go nie bylo)' -ForegroundColor Yellow
        Write-Host ''
        Write-Host "      $panelGenerated" -ForegroundColor White
        Write-Host ''
        Write-Host '  Jest tez w linux-port\docker\.env (M2_PANEL_PASSWORD).' -ForegroundColor Yellow
        Write-Host '  Jesli panel go nie przyjmuje, to znaczy, ze zapamietal' -ForegroundColor Yellow
        Write-Host '  starsze - uzyj przycisku HASLO DO PANELU w launcherze.' -ForegroundColor Yellow
        Write-Host '=============================================================' -ForegroundColor Yellow
        Write-Host ''
    }
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
        $desktopCandidates = @(Get-DockerDesktopCandidates)
        if ($desktopCandidates.Count -eq 0) {
            throw 'Nie znaleziono programu Docker Desktop (szukano w Program Files, LOCALAPPDATA, obok docker.exe i w rejestrze). Uruchom Docker Desktop recznie i kliknij GRAJ.'
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
    # Everything else prepare-context.sh stages, for the same reason as the
    # three above: it does not run here. Left alone, each of these stays at
    # whatever the installer shipped however many updates go by - and
    # panel/app/VERSION is the one that gets noticed, because it is what the
    # panel reports it is running. An operator three releases past 1.15.6 was
    # still being told 1.15.6 by a panel rebuilt with --no-cache.
    #
    # speed_boost.quest is not here on purpose: prepare-context rewrites a line
    # in it rather than copying it, and this script renders that file itself
    # further down.
    $stagedPairs = @(
        @{ From = 'files\admin_panel.py';       To = 'linux-port\docker\panel\app\admin_panel.py' },
        @{ From = 'VERSION';                    To = 'linux-port\docker\panel\app\VERSION' },
        @{ From = 'VERSION';                    To = 'linux-port\docker\seban-panel\VERSION' },
        @{ From = 'CHANGELOG.md';               To = 'linux-port\docker\panel\app\CHANGELOG.md' },
        @{ From = 'files\items.json';           To = 'linux-port\docker\panel\app\items.json' },
        @{ From = 'files\favicon.png';          To = 'linux-port\docker\panel\app\favicon.png' },
        @{ From = 'files\web_admin_schema.sql'; To = 'linux-port\docker\panel\schema\web_admin_schema.sql' },
        @{ From = 'files\web_admin.quest';      To = 'linux-port\docker\game\quest\web_admin.quest' },
        @{ From = 'files\high_risk.quest';      To = 'linux-port\docker\game\quest\high_risk.quest' },
        @{ From = 'linux-port\overlays\playerbot\serverfiles\mob_drop_item.m3.append.txt';
           To   = 'linux-port\docker\game\mob_drop_item.m3.append.txt' },
        @{ From = 'linux-port\overlays\playerbot\serverfiles\special_item_group.moonlight.txt';
           To   = 'linux-port\docker\game\special_item_group.moonlight.txt' }
    )
    foreach ($pair in $stagedPairs) {
        $from = Join-Path $PSScriptRoot $pair.From
        $to   = Join-Path $PSScriptRoot $pair.To
        if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { continue }
        # The target directory is made when it is missing. panel\schema and
        # game\quest are ignored by Git, so a fresh install unpacked from the
        # repository archive has neither - and skipping the copy for want of a
        # directory left the panel image without its schema and the build
        # failing at "COPY schema/" on every Start, update or not.
        $toParent = Split-Path -Parent $to
        if (-not (Test-Path -LiteralPath $toParent -PathType Container)) {
            New-Item -ItemType Directory -Path $toParent -Force | Out-Null
        }
        $toHash = $null
        if (Test-Path -LiteralPath $to -PathType Leaf) {
            $toHash = (Get-FileHash -LiteralPath $to -Algorithm SHA256).Hash
        }
        if ($toHash -ne (Get-FileHash -LiteralPath $from -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $from -Destination $to -Force
            $syncedFiles++
        }
    }

    # /static is a directory, and prepare-context copies it whole so that adding
    # an icon set is only ever a matter of putting one there. Same rule here.
    $staticSource = Join-Path $PSScriptRoot 'files\static'
    $staticStaged = Join-Path $PSScriptRoot 'linux-port\docker\panel\app\static'
    if (Test-Path -LiteralPath $staticSource -PathType Container) {
        foreach ($asset in Get-ChildItem -LiteralPath $staticSource -Recurse -File) {
            $relative = $asset.FullName.Substring($staticSource.Length).TrimStart('\')
            $target = Join-Path $staticStaged $relative
            $targetParent = Split-Path -Parent $target
            if (-not (Test-Path -LiteralPath $targetParent -PathType Container)) {
                New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
            }
            $targetHash = $null
            if (Test-Path -LiteralPath $target -PathType Leaf) {
                $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
            }
            if ($targetHash -ne (Get-FileHash -LiteralPath $asset.FullName -Algorithm SHA256).Hash) {
                Copy-Item -LiteralPath $asset.FullName -Destination $target -Force
                $syncedFiles++
            }
        }
    }

    # The migrate container mounts mariadb/playerbot, so the seed it applies is a
    # copy of the overlay's, staged by prepare-context.sh - which never runs
    # here. Left alone it stays at whatever the distribution shipped.
    $seedSource = Join-Path $PSScriptRoot 'linux-port\overlays\playerbot\sql\playerbots_seed.sql'
    $seedStaged = Join-Path $PSScriptRoot 'linux-port\docker\mariadb\playerbot\playerbots_seed.sql'
    if ((Get-ServerEngine) -ne 'r40250') {
        # mt2009's seed is rendered from the overlay's by port/seedify.py and
        # ships where the migrate container mounts it; the overlay's own would
        # write columns this schema does not have. Nothing to copy over it.
    }
    elseif ((Test-Path -LiteralPath $seedSource -PathType Leaf) -and
        (Test-Path -LiteralPath (Split-Path -Parent $seedStaged) -PathType Container)) {
        $seedHash = $null
        if (Test-Path -LiteralPath $seedStaged -PathType Leaf) {
            $seedHash = (Get-FileHash -LiteralPath $seedStaged -Algorithm SHA256).Hash
        }
        if ($seedHash -ne (Get-FileHash -LiteralPath $seedSource -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $seedSource -Destination $seedStaged -Force
            $syncedFiles++
        }
    }
    elseif (-not (Test-Path -LiteralPath $seedSource -PathType Leaf)) {
        Write-Host "UWAGA: brak $seedSource - stragan botow nie zostanie odswiezony." -ForegroundColor Yellow
    }

    # The migrate container's first act is `[ -s playerbots_seed.sql ] || exit 1`.
    # A missing or empty seed therefore fails the whole start one second after
    # the database comes up, and used to do so with nothing in the log but
    # "exit 1". Refuse here, with the path, rather than let compose discover it.
    if (-not (Test-Path -LiteralPath $seedStaged -PathType Leaf) -or
        (Get-Item -LiteralPath $seedStaged).Length -eq 0) {
        throw ("Brak pliku z postaciami botow: $seedStaged (lub jest pusty). " +
               "Paczka jest niekompletna - uruchom aktualizacje z launchera albo " +
               "rozpakuj archiwum serwera ponownie. Bez tego pliku playerbot-migrate " +
               "konczy sie bledem exit 1 przy kazdym starcie.")
    }

    if ($syncedFiles -gt 0) {
        Write-Host "Synchronised $syncedFiles playerbot build input(s) into the build context." -ForegroundColor DarkGray
    }
}

# The game image is built from linux-port/docker/game/src, and that tree is put
# there once by fetch-sources.sh at install time out of the operator's own
# r40250 package. It is not ours to ship, so an update never restores it - and
# an update *does* write linux-port/docker/game/src/server/game/src, because
# that is where the playerbot sources belong. On an install whose staged tree
# has gone missing that combination is quietly misleading: src/server/game
# exists, everything beside it does not, and `docker compose' answers with a
# dozen "failed to calculate checksum ... not found" lines naming paths the
# operator never touched. Reported from the Discord with a 1.61 MB build
# context, where a complete one is hundreds of megabytes.
#
# So say it here, once, in words, before Docker gets a chance to say it badly.
$gameContext = Join-Path $PSScriptRoot 'linux-port\docker\game\src'
# An empty directory is the one build input a package cannot be relied on to
# deliver - git does not track one and an extraction tool may drop the bare
# zip entry - and share\package is empty on every install of both lines. Make
# it rather than refuse over it (Restore-M2EmptyGameContextDirs in the module
# says the same; this script imports no module and carries its own copy).
$emptyByDesign = Join-Path $gameContext 'serverfiles\share\package'
if (-not (Test-Path -LiteralPath $emptyByDesign)) {
    try { New-Item -ItemType Directory -Path $emptyByDesign -Force -ErrorAction Stop | Out-Null } catch { }
}
# Per engine, the same list as Get-M2RequiredGameContext in the module:
# mt2009 keeps its protos in the database (no share\conf) and its
# dependency script one level up, in game\.
$requiredContext = if ((Get-ServerEngine) -eq 'mt2009') {
    @(
        '..\build-deps-mt2009.sh', 'extern\include', 'extern\cryptopp', 'extern-tarballs',
        'server\__REVISION__',
        'server\common', 'server\db', 'server\game', 'server\libgame',
        'server\liblua', 'server\libpoly', 'server\libsql', 'server\libthecore',
        'serverfiles\share\CMD', 'serverfiles\share\data',
        'serverfiles\share\locale', 'serverfiles\share\package',
        'serverfiles\mark-default'
    )
} else {
    @(
        'build-deps-40250.sh',
        'extern',
        'server\common', 'server\db', 'server\game', 'server\libgame',
        'server\liblua', 'server\libpoly', 'server\libserverkey',
        'server\libsql', 'server\libthecore',
        'serverfiles\share\conf', 'serverfiles\share\data',
        'serverfiles\share\locale', 'serverfiles\share\package',
        'serverfiles\mark-default'
    )
}
$missingContext = @()
foreach ($entry in $requiredContext) {
    if (-not (Test-Path -LiteralPath (Join-Path $gameContext $entry))) {
        $missingContext += $entry
    }
}
# And the database's half of the context. The dumps are only read on the very
# first start of an empty volume, so an install whose database already exists
# is not held up by them; a fresh one without them would come up with an empty
# MariaDB that reports healthy while playerbot-migrate waits thirty minutes.
$dumpDir = Join-Path $PSScriptRoot 'linux-port\docker\mariadb\initdb.d\dumps'
$missingDumps = @()
# r40250's package ships hotbackup (empty by design); mt2009's keeps the
# protos in a sixth database, world, and has no hotbackup dump.
$requiredDumps = if ((Get-ServerEngine) -eq 'mt2009') { @('account', 'common', 'player', 'log', 'world') }
                 else { @('account', 'common', 'player', 'log', 'hotbackup') }
foreach ($db in $requiredDumps) {
    $f = Join-Path $dumpDir "$db.sql"
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { $missingDumps += "$db.sql" }
    elseif ($db -ne 'hotbackup' -and (Get-Item -LiteralPath $f).Length -eq 0) { $missingDumps += "$db.sql (pusty)" }
}
if ($missingDumps.Count -gt 0) {
    $dbVolumeInitialized = $false
    $envFile = Join-Path $PSScriptRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envFile -PathType Leaf) {
        $projectMatch = [Regex]::Match([IO.File]::ReadAllText($envFile), '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$')
        if ($projectMatch.Success) {
            $dbVolume = $projectMatch.Groups[1].Value + '_db-data'
            $previousEap = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
            try {
                & docker volume inspect $dbVolume 1>$null 2>$null
                if ($LASTEXITCODE -eq 0) {
                    & docker run --rm --entrypoint sh -v "${dbVolume}:/v:ro" mariadb:10.11 -c 'test -d /v/mysql' 1>$null 2>$null
                    $dbVolumeInitialized = ($LASTEXITCODE -eq 0)
                }
            } finally { $ErrorActionPreference = $previousEap }
        }
    }
    if (-not $dbVolumeInitialized) {
        throw ("Brakuje zrzutow bazy danych w " + $dumpDir + ".`n" +
               "Brakuje: " + ($missingDumps -join ', ') + "`n`n" +
               "Bez nich MariaDB uruchomi sie pusta (i zglosi 'healthy'), a playerbot-migrate " +
               "bedzie czekal 30 minut na schemat, ktory nigdy nie powstanie. Zrzuty pochodza " +
               "z Twojej paczki serwera r40250 (Server\metin2_mysql_dump.zip) i wystawia je " +
               "instalator - aktualizacja ich nie przywraca.`n" +
               "Uruchom ponownie instalator (installer\install.ps1), wskazujac paczke przez " +
               "`$env:M2_SRC_ARCHIVE, albo rozpakuj metin2_mysql_dump.zip do tego katalogu " +
               "(account.sql, common.sql, player.sql, log.sql, hotbackup.sql) i kliknij GRAJ jeszcze raz.")
    }
}
if ($missingContext.Count -gt 0) {
    throw ("Niekompletne zrodla gry w " + $gameContext + ".`n" +
           "Brakuje: " + ($missingContext -join ', ') + "`n`n" +
           "To nie jest blad Dockera ani aktualizacji. Te pliki pochodza z Twojej " +
           "wlasnej paczki serwera r40250 i sa rozpakowywane raz, przy instalacji - " +
           "aktualizacja ich nie przywraca, bo nie wolno nam ich rozpowszechniac.`n" +
           "Uruchom ponownie instalator (installer\install.ps1), ktory pobierze " +
           "zrodla i odtworzy kontekst budowy. Twoja baza, postacie i ustawienia " +
           "zostaja nietkniete.")
}

Write-Host 'Starting Metin2 services...' -ForegroundColor Cyan
# What the advanced panel reports as the Playerbots release: compose reads the
# process environment ahead of .env, and this file is never rewritten by us.
# See Set-M2PlayerbotsVersionEnvironment in the launcher module - this script
# does not import it.
if (-not $env:M2_PLAYERBOTS_VERSION) {
    $versionFile = Join-Path $PSScriptRoot 'VERSION'
    if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
        $versionText = ([IO.File]::ReadAllText($versionFile)).Trim()
        if ($versionText -match '^\d+\.\d+\.\d+$') { $env:M2_PLAYERBOTS_VERSION = $versionText }
    }
}
# An older installation of this same server, holding a port this stack is about
# to publish. Every container ships `restart: unless-stopped`, so Docker Desktop
# starts that project again on every engine start and it binds 7788/7790/11000
# before this one can - which is why quitting Docker by hand never helped, and
# why the collision came back on every single update. This script imports
# nothing (see the note at the top), so the lookup is local: a running container
# whose compose project differs from ours and which publishes one of our host
# ports. `docker stop` is what holds, because its manual-stop flag survives an
# engine restart. Volumes are never touched - the collision is containers, and a
# removed volume is the world.
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    $portValues = @{}
    $portEnvPath = Join-Path $composeDirectory '.env'
    if (Test-Path -LiteralPath $portEnvPath -PathType Leaf) {
        foreach ($line in @([IO.File]::ReadAllLines($portEnvPath))) {
            if ("$line" -match '^\s*([A-Za-z0-9_]+)=(.*)$') { $portValues[$Matches[1]] = $Matches[2].Trim() }
        }
    }
    $wantedPorts = @()
    foreach ($pair in @(@('M2_PANEL_PUBLIC_PORT', 7788), @('M2_SEBAN_PANEL_PORT', 7790),
                        @('M2_ITEMSHOP_PUBLIC_PORT', 7791), @('M2_AUTH_PORT', 11000),
                        @('M2_DB_PUBLISH_PORT', 3306))) {
        $raw = [string]$portValues[$pair[0]]
        if ($raw -match '^\d+$') { $wantedPorts += [int]$raw } else { $wantedPorts += [int]$pair[1] }
    }
    $rangeFirst = 13000
    $rangeLast = 13002
    $gameRange = [string]$portValues['M2_GAME_PORT_RANGE']
    if ($gameRange -match '^(\d+)\s*-\s*(\d+)$') { $rangeFirst = [int]$Matches[1]; $rangeLast = [int]$Matches[2] }
    elseif ($gameRange -match '^(\d+)$') { $rangeFirst = [int]$Matches[1]; $rangeLast = $rangeFirst }
    if ($rangeLast -lt $rangeFirst -or ($rangeLast - $rangeFirst) -gt 32) { $rangeLast = $rangeFirst }
    for ($p = $rangeFirst; $p -le $rangeLast; $p++) { $wantedPorts += [int]$p }
    $ourProject = [string]$portValues['M2_COMPOSE_PROJECT_NAME']
    $stoppedProjects = @()
    # '{{json .}}' carries no double quote, so PowerShell 5.1 cannot break this
    # argument the way it breaks an `sh -c` script handed to docker.
    foreach ($line in @(& docker ps --format '{{json .}}' 2>$null)) {
        if (-not "$line".Trim()) { continue }
        try { $container = "$line" | ConvertFrom-Json } catch { continue }
        $project = ''
        $labels = [string]$container.Labels
        if ($labels -match '(?:^|,)com\.docker\.compose\.project=([^,]+)') { $project = $Matches[1] }
        if (-not $project -or $stoppedProjects -contains $project) { continue }
        if ($ourProject -and $project.Equals($ourProject, [StringComparison]::OrdinalIgnoreCase)) { continue }
        $publishedPorts = [string]$container.Ports
        $collides = $false
        foreach ($wanted in $wantedPorts) {
            if ($publishedPorts -match ('(?:^|,\s*)(?:(?:0\.0\.0\.0|127\.0\.0\.1|\[::\]|\*):)?' + $wanted + '->')) {
                $collides = $true
                break
            }
        }
        if (-not $collides) { continue }
        $stoppedProjects += $project
        Write-Host ("Inna instalacja serwera ('{0}') trzyma port tego serwera - zatrzymuje ja, zeby ten serwer mogl wstac." -f $project) -ForegroundColor Yellow
        Write-Host '   Baza, wolumeny i postep tamtej instalacji pozostaja nietkniete.' -ForegroundColor DarkGray
        $ids = @(& docker ps -aq --filter ('label=com.docker.compose.project=' + $project) 2>$null | Where-Object { $_ })
        if ($ids.Count -gt 0) { & docker stop $ids 1>$null 2>$null }
    }
}
catch {
    Write-Host ("Nie udalo sie sprawdzic zajetych portow: {0}" -f $_.Exception.Message) -ForegroundColor DarkYellow
}
finally { $ErrorActionPreference = $previousPreference }

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
        # Shown as it arrives and kept: the one line that says why a start
        # failed comes from compose itself, and the failure branch below wants
        # to read it after the fact.
        $composeWatch = [Diagnostics.Stopwatch]::StartNew()
        $composeOutput = & docker @composeArguments 2>&1 | ForEach-Object { $line = "$_"; Write-Host $line; $line }
        $upExitCode = $LASTEXITCODE
        # compose waits for the database to be healthy, the migrator to finish
        # and the game to answer its healthcheck before it returns; this one
        # number is "how long the server took to come up".
        Write-Host ("[faza] docker compose up zakonczone po {0} s (kod {1})" -f [int]$composeWatch.Elapsed.TotalSeconds, $upExitCode) -ForegroundColor DarkCyan
        & docker compose ps
        $psExitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($upExitCode -ne 0) {
        # The one line that explains a failed start is inside the container that
        # failed, and compose's own progress output never shows it. Pull it into
        # this log so the next report from a player carries the cause, not just
        # "exit 1". playerbot-migrate is named first because it is the service
        # that fails fast and blocks everything behind it.
        $previousPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            Write-Host '--- ostatnie linie logow kontenerow ---' -ForegroundColor DarkGray
            foreach ($svc in @('playerbot-migrate', 'mariadb', 'game', 'panel')) {
                Write-Host "[$svc]" -ForegroundColor DarkGray
                & docker compose logs --no-color --no-log-prefix --tail 40 $svc 2>&1 |
                    ForEach-Object { Write-Host "  $_" }
            }
        }
        finally { $ErrorActionPreference = $previousPreference }
        # Windows refused the bind: the port sits in a range Hyper-V or WSL
        # reserved after the last restart. Say so, with the two commands that
        # free it - the exit code alone sent one player through five identical
        # update attempts.
        $composeText = ($composeOutput | ForEach-Object { "$_" }) -join "`n"
        if ($composeText -match '(?i)ports are not available|forbidden by its access permissions|zabroniony przez uprawnienia|WSAEACCES|\b10013\b') {
            $port = if ($composeText -match '(?i)listen (?:tcp\d? )?[^\s:]+:(\d{2,5})') { $Matches[1] } else { '11000' }
            Write-Host ''
            Write-Host "Windows zarezerwowal port $port dla siebie (Hyper-V/WSL), wiec Docker nie moze na nim nasluchiwac." -ForegroundColor Yellow
            Write-Host 'To nie jest zajety port ani blad plikow serwera. Uruchom PowerShell jako administrator i wykonaj:' -ForegroundColor Yellow
            Write-Host '    net stop winnat' -ForegroundColor Yellow
            Write-Host 'potem kliknij GRAJ, a gdy serwer wstanie:' -ForegroundColor Yellow
            Write-Host '    net start winnat' -ForegroundColor Yellow
            Write-Host 'Zwykle pomaga tez restart Windows. Zakresy: netsh interface ipv4 show excludedportrange protocol=tcp' -ForegroundColor Yellow
        }
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
