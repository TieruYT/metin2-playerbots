# The full package of the mt2009 line - the one zip a player downloads from
# the hosting: the client and the whole server tree side by side, as
#
#     Metin2 Singleplayer\
#         CZYTAJ.txt
#         Klient\      the client, its packs pointing at 127.0.0.1
#         Serwer\      the deploy tree from New-M2DeployTree.ps1
#
# Nothing personal goes in: the server's .env, installation identity, logs,
# backups and support bundles stay out, and so do the client's saved
# credentials, settings, screenshots and syserr.txt. The launcher makes a
# fresh .env and identity on the player's first start.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Deploy,
    [Parameter(Mandatory = $true)][string]$Client,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Version = '',
    [string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe'
)
$ErrorActionPreference = 'Stop'
foreach ($p in @($Deploy, $Client)) {
    if (-not (Test-Path -LiteralPath $p -PathType Container)) { throw "no such directory: $p" }
}
if (-not (Test-Path -LiteralPath $SevenZip -PathType Leaf)) { throw "7-Zip not found at $SevenZip" }
if (-not (Test-Path -LiteralPath (Join-Path $Client 'metin2client.exe') -PathType Leaf)) { throw "no metin2client.exe in $Client" }
if (-not $Version) { $Version = ([IO.File]::ReadAllText((Join-Path $Deploy 'VERSION'))).Trim() }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$staging = Join-Path ([IO.Path]::GetTempPath()) ('m2-full-' + [Guid]::NewGuid().ToString('N'))
$rootName = 'Metin2 Singleplayer'
$stageRoot = Join-Path $staging $rootName
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

function Invoke-Mirror {
    param([string]$From, [string]$To, [string[]]$ExcludeFiles = @(), [string[]]$ExcludeDirs = @())
    $args = @($From, $To, '/E', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    if ($ExcludeFiles.Count -gt 0) { $args += '/XF'; $args += $ExcludeFiles }
    if ($ExcludeDirs.Count -gt 0) { $args += '/XD'; $args += $ExcludeDirs }
    & robocopy @args | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $From -> $To" }
}

try {
    Write-Host "staging the server from $Deploy"
    # Every copy of .env the launcher leaves beside it - .env.last-good after
    # each identity step, .env.bak-<version>, .env.damaged-<stamp> - carries
    # the database's passwords like .env itself; the 2.0.86 package shipped two
    # of them from the test world. The scan below refuses any other shape.
    Invoke-Mirror -From $Deploy -To (Join-Path $stageRoot 'Serwer') `
        -ExcludeFiles @('.env', '.env.last-good', '.env.bak*', '.env.damaged*', '.env.new', '.env.tmp',
                        '.m2install.json', '.m2coop.json', '.m2launcher.json', '.m2launcher-state.json',
                        '.m2launcher-offers.json', '.m2launcher-classic-layout', '*.log',
                        'Metin2-Launcher-GUI-UI-Test.*', 'Metin2-Launcher-UI-Test.*') `
        -ExcludeDirs @('launcher-logs', 'backups', 'support-bundles', 'seban-panel.prev')
    foreach ($must in @('VERSION', 'Metin2-Launcher-GUI.bat', 'linux-port\docker\ENGINE', 'linux-port\docker\.env.example',
                        'linux-port\docker\game\src\server\game\src\playerbot_manager.cpp',
                        'linux-port\docker\mariadb\initdb.d\dumps\world.sql')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stageRoot "Serwer\$must") -PathType Leaf)) { throw "the deploy tree lacks $must" }
    }
    if (Test-Path -LiteralPath (Join-Path $stageRoot 'Serwer\linux-port\docker\.env')) { throw '.env leaked into the staging' }

    Write-Host "staging the client from $Client"
    # A player's client writes its own settings beside the exe: the COOP
    # server entry (coop.cfg - a friend's world and password), Auto Lowy per
    # character, the bot-title switch, the login preload log; and the packs'
    # *.bak are the operator's repack backups under pack\. Other exes beside
    # metin2client.exe are the operator's test builds, and stats_session_*
    # is the session statistics window's record (uiplayerstat.py) of the
    # operator's characters.
    Invoke-Mirror -From $Client -To (Join-Path $stageRoot 'Klient') `
        -ExcludeFiles @('syserr.txt', 'credentials.json', 'game_settings.json', 'coop.cfg',
                        'playerbot_titles.cfg', 'autohunt_*.cfg', 'login_preload.log', '*.bak',
                        'metin2client-*.exe', 'stats_session_*.json') `
        -ExcludeDirs @('screenshot', 'upload', 'autohunt')

    # The exclusions above name what is known; this names the shape, so a new
    # kind of copy the launcher starts leaving refuses the package instead of
    # travelling in it.
    $private = @(Get-ChildItem -LiteralPath $stageRoot -Recurse -Force -File | Where-Object {
        ($_.Name -like '.env*' -and $_.Name -ne '.env.example') -or
        (@('.m2install.json', '.m2coop.json', 'coop.cfg', 'credentials.json', 'm2panel.conf') -contains $_.Name) -or
        $_.Name -like 'metin2-support-*.zip'
    })
    if ($private.Count -gt 0) {
        throw ('installation-private files in the staging: ' +
            (($private | ForEach-Object { $_.FullName.Substring($stageRoot.Length + 1) }) -join ', '))
    }

    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\CZYTAJ.txt') -Destination (Join-Path $stageRoot 'CZYTAJ.txt') -Force

    $zipPath = Join-Path $OutputDirectory ("Metin2-Singleplayer-$Version.zip")
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Write-Host "zipping to $zipPath"
    Push-Location $staging
    try {
        & $SevenZip a -tzip -mx=5 -mmt=on -bso0 -bsp0 $zipPath $rootName
        if ($LASTEXITCODE -ne 0) { throw "7z failed with $LASTEXITCODE" }
    }
    finally { Pop-Location }

    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
    [IO.File]::WriteAllText("$zipPath.sha256", "$hash  $(Split-Path -Leaf $zipPath)`n", [Text.UTF8Encoding]::new($false))
    $mb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB)
    Write-Host "Created: $zipPath ($mb MB)"
    Write-Host "SHA-256: $hash"
}
finally {
    if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
