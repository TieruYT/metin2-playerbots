# =============================================================================
#  Metin2 server -- one-command installer for Windows.
#
#      irm https://example.com/install.ps1 | iex
#
#  ...or, to pass options:
#
#      iex "& { $(irm https://example.com/install.ps1) } -DryRun"
#
#  THIS INSTALLS A SERVER YOU CAN PLAY ON BY YOURSELF, ON THIS PC.
#
#  Every part of it is bound to 127.0.0.1 -- the address that means "this
#  computer and nothing else". No port is opened to your home network or to the
#  internet, no firewall rule is created, and nobody else can join, not even
#  someone sitting next to you on the same Wi-Fi. That is deliberate: putting a
#  game server on a home connection means handing out your home IP address, and
#  it is not what most people want when they say "I would like to try this".
#
#  If you want a real server that friends can play on, rent a small Linux VPS
#  and use install.sh instead. The last section of the output tells you how.
#
#  What it does, in order:
#
#     1. checks this PC can run it (processor, memory, disk, Windows version)
#     2. makes sure Docker Desktop is installed and running
#     3. downloads the server
#     4. invents a password for the admin panel and two for the database
#     5. builds and starts the server, all on 127.0.0.1
#     6. starts building a game client pointed at 127.0.0.1, and -- long after
#        this installer has finished -- unpacks it onto this PC and puts a
#        "Metin2 Singleplayer" shortcut on the Desktop
#     7. prints the three things you need: the game, panel link, password
#
#  Safe to run twice. If a server is already here it says so and leaves your
#  characters alone.
#
#  ---------------------------------------------------------------------------
#  Written so that a HALF-DOWNLOADED copy cannot do anything: everything is
#  inside functions and the only line that runs anything is the very last one.
#  A truncated download is a parse error, and PowerShell parses the whole thing
#  before running any of it -- so nothing happens at all.
#  ---------------------------------------------------------------------------
#
#  PowerShell 5.1 compatible. It does not need PowerShell 7.
# =============================================================================

$ErrorActionPreference = 'Stop'

# Windows PowerShell 5.1 still negotiates TLS 1.0 by default on some machines,
# and every host worth downloading from turned that off years ago.
try {
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch { }

# -----------------------------------------------------------------------------
#  Where the server comes from.
#
#  There is no release archive, and there never will be one. The project holds
#  the Linux port and nothing else -- one 109 KB patch touching 28 files, plus
#  the scripts that turn a checkout into something buildable. Everything
#  copyrighted (the game source, the runtime data tree, the SQL dumps) belongs
#  to Ymir/Webzen and to whoever assembled the r40250 server-file package, and
#  it is not ours to hand out.
#
#  So the server is assembled here, on this PC:
#
#     1. get the project -- a git clone of a few megabytes -- unless a copy is
#        already on this PC
#     2. run linux-port/fetch-sources.sh, which obtains the original r40250
#        package, extracts the source, the game data and the SQL dumps, applies
#        the port, and fills in the Docker build context
#
#  That second script is POSIX shell, and Windows has no shell that can run it.
#  Rather than translate 43 KB of hard-won logic into PowerShell -- and get a
#  second set of bugs for free -- it is run inside a small Debian container, on
#  the Docker that this installer has already made sure is working. The Windows
#  install and the Linux install therefore run exactly the same code, which is
#  worth a great deal when somebody has to debug one of them from a distance.
#
#  Override before running:
#      $env:M2_REPO_URL           = 'https://.../server.git'
#      $env:M2_REPO_DIR           = 'C:\path\to\checkout'
#      $env:M2_SRC_REFERENCE_DIR  = 'C:\path\to\[40250] Reference Serverfile'
#      $env:M2_SRC_ARCHIVE        = 'C:\path\to\the-package.zip'
#      $env:M2_LOCAL_CONTEXT      = 'C:\path\to\linux-port\docker'
# -----------------------------------------------------------------------------
$script:RepoUrl      = if ($env:M2_REPO_URL)          { $env:M2_REPO_URL }          else { 'https://github.com/TieruYT/metin2-playerbots.git' }
# Where this script itself lives, so it can tell the panel how to update: on
# Windows the update IS re-running this, and the panel shows the line to paste.
$script:SelfUrl      = if ($env:M2_INSTALLER_URL)     { $env:M2_INSTALLER_URL }     else { 'https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.ps1' }
$script:RepoDir      = if ($env:M2_REPO_DIR)          { $env:M2_REPO_DIR }          else { '' }
$script:LocalContext = if ($env:M2_LOCAL_CONTEXT)     { $env:M2_LOCAL_CONTEXT }     else { '' }
$script:SrcArchive   = if ($env:M2_SRC_ARCHIVE)       { $env:M2_SRC_ARCHIVE }       else { '' }
$script:SrcRefDir    = if ($env:M2_SRC_REFERENCE_DIR) { $env:M2_SRC_REFERENCE_DIR } else { '' }
$script:SrcUrl       = if ($env:M2_SRC_URL)           { $env:M2_SRC_URL }           else { '' }
$script:ClientArchive = if ($env:M2_CLIENT_ARCHIVE)   { $env:M2_CLIENT_ARCHIVE }    else { '' }

# The download, the unpacked source and the staged tree all live in a Docker
# volume rather than in a folder on C:. Three reasons, all learned by trying the
# other way: it is several times faster than a bind mount into Windows, none of
# those paths then have to fit inside Windows' 260-character limit, and the
# whole lot can be thrown away afterwards with one command.
$script:SrcVolume    = if ($env:M2_SRC_VOLUME) { $env:M2_SRC_VOLUME } else { 'metin2-src-cache' }

# Tagged with a version so that changing the recipe below rebuilds it.
# The tag carries a number so that changing what is IN the image reaches PCs
# that already have one: Initialize-FetcherImage keeps whatever it finds under
# this name, so a rebuild has to be asked for by asking for a different name.
# :2 added python3, without which the Custom Experience and the shipped-file
# fixes cannot be applied to the staged tree.
$script:FetcherImage = 'metin2-src-fetcher:2'

# Where this script is, when it is a file at all. Run as `irm ... | iex' it is
# not, and $PSScriptRoot is then empty -- which is exactly the answer we want,
# rather than a wrong guess at a checkout.
$script:SelfDir = if ($PSScriptRoot) { $PSScriptRoot } else { '' }

# State filled in as we go.
$script:InstallDir        = ''
$script:AuthPort          = 11000
$script:GamePorts         = '13000-13002'
$script:PanelPort         = 7788
# The WebSocket bridge for the browser client. Off unless .env already says
# otherwise; this installer never switches it on by itself, because it is only
# useful once a browser client has been put on the panel's volume.
$script:BrowserPlay       = '0'

# Which clients this install offers -- decided by Select-Clients. WantWebFlag
# stays $null unless -WebClient or -NoWebClient was actually passed, so "never
# asked" and "answered no" stay apart; without that a re-run could not tell a
# deliberate no from a default.
$script:WantWeb           = $false
$script:WantDesktop       = $true
$script:SkipClient        = $false
$script:WantWebFlag       = $null
$script:HaveWeb           = $false
$script:HaveDesktop       = $false
$script:BridgePort        = '7789'
$script:PanelPassword     = ''
$script:PanelPasswordKnown = $true
$script:PanelPasswordNew   = $true
$script:PanelPasswordChosen = $false   # the operator picked this one in the panel
$script:FreshInstall      = $true
$script:ClientState       = 'unavailable'
$script:ClientLog         = ''
# Where the playable game ends up on this PC, and what the Desktop shortcut
# into it is called. Both are read by the summary at the end.
$script:ClientDir         = ''
$script:ClientExe         = ''
$script:ShortcutName      = 'Metin2 Singleplayer'
# Where PowerShell's own complaints go when the background script cannot even
# be started. Filled in with a name of their own per launch -- see
# Merge-ClientSideLogs.
$script:ClientHostOut     = ''
$script:ClientHostErr     = ''
# A Windows install is always local-only -- Write-Configuration says so in the
# .env -- and that is what decides whether the game is unpacked onto this PC or
# merely offered on the panel's download page.
$script:LocalOnly         = $true
$script:DryRun            = $false
$script:AssumeYes         = $false

# The Custom Experience -- see Select-CustomExperience. The flag stays $null
# unless -CustomExperience or -NoCustomExperience was actually given, for the
# same reason WantWebFlag does: on an update, "never said" means keep what this
# server has and "said no" means take it back out, and those must not collapse
# into one another.
$script:CustomExperience     = $false
$script:CustomExperienceFlag = $null
$script:HighRisk             = ''
$script:MoveSpeedBonus       = ''

# =============================================================================
#  Talking to the human
# =============================================================================

function Write-Step  { param([string]$Text) Write-Host ''; Write-Host "==> $Text" -ForegroundColor Cyan }
function Write-Say   { param([string]$Text) Write-Host "  $Text" }
function Write-Info  { param([string]$Text) Write-Host "  $Text" -ForegroundColor DarkGray }
function Write-Good  { param([string]$Text) Write-Host "  + " -ForegroundColor Green -NoNewline; Write-Host $Text }
function Write-Warn  { param([string]$Text) Write-Host "  ! " -ForegroundColor Yellow -NoNewline; Write-Host $Text }
function Write-Rule  { Write-Host "  ----------------------------------------------------------------" -ForegroundColor DarkGray }

# Every failure this script knows about comes through here, so the user always
# gets a sentence they can act on rather than a red wall of .NET.
function Stop-Friendly {
    param([string]$Message)
    Write-Host ''
    Write-Host '  Something went wrong.' -ForegroundColor Red
    Write-Host ''
    foreach ($line in ($Message -split "`n")) { Write-Host "  $line" }
    Write-Host ''
    throw (New-Object System.OperationCanceledException 'installer stopped')
}

function Confirm-YesNo {
    param([string]$Question, [bool]$DefaultYes = $true)
    if ($script:AssumeYes) { return $true }
    if (-not [Environment]::UserInteractive) {
        Write-Info "$Question -- running unattended, assuming $(if ($DefaultYes) {'yes'} else {'no'})"
        return $DefaultYes
    }
    $hint = if ($DefaultYes) { '[Y/n]' } else { '[y/N]' }
    while ($true) {
        $a = Read-Host "  $Question $hint"
        if ([string]::IsNullOrWhiteSpace($a)) { return $DefaultYes }
        switch -Regex ($a.Trim()) {
            '^(y|yes)$' { return $true }
            '^(n|no)$'  { return $false }
            default     { Write-Host '  Please answer y or n.' }
        }
    }
}

function Test-Command {
    param([string]$Name)
    $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

# Run a program and hand back its exit code and its output, without letting
# anything it prints become a fatal error.
#
# Windows PowerShell turns a native program's standard error into an error
# record as soon as it is redirected with 2>&1, and the $ErrorActionPreference
# = 'Stop' at the top of this file then makes that record *terminating*. Several
# of the docker commands below are questions whose answer is legitimately "no" --
# "does this volume exist?", "does this container exist?" -- and docker says no
# on standard error, with a non-zero exit code. Asked directly, the first such
# question kills the installer with docker's own sentence and no context.
#
# So the preference is lowered for exactly the length of the call, which is the
# only way to ask a native program a yes/no question in PowerShell 5.1 and live
# to read the answer.
function Invoke-Native {
    param([string]$File, [string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & $File @Arguments 2>&1
        return [pscustomobject]@{ Code = $LASTEXITCODE; Output = (($out | Out-String).Trim()) }
    } finally { $ErrorActionPreference = $previous }
}

# Run docker and let the human watch it, then hand back only the exit code.
#
# The Out-Host is not decoration. PowerShell collects everything a function
# writes to the success stream into that function's return value, so without it
# the "exit code" this returns is an array of several hundred lines of docker
# output -- and `-ne 0' on a non-empty array is true, which turns every
# successful build into a reported failure. Invoke-Compose below carries the
# same note for the same reason; they were the same bug.
function Invoke-DockerLoud {
    param([string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & docker @Arguments 2>&1 | Out-Host
        return $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
}

# Copy-Item with a "dir\*" wildcard quietly leaves hidden files behind, and the
# stack's .env.example and .gitignore both start with a dot. Enumerating with
# -Force and copying each entry by its full path picks them all up.
function Copy-DirectoryContents {
    param([string]$Source, [string]$Destination)
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    foreach ($item in (Get-ChildItem -LiteralPath $Source -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $Destination -Recurse -Force
    }
}

# =============================================================================
#  Secrets -- all three generated here, on this PC, right now
# =============================================================================

function New-Secret {
    # Never typed by a human, only read by the server. Hex so that it can never
    # contain a space or a quote, which the game's config parser splits on.
    $bytes = New-Object byte[] 24
    $rng = [System.Security.Cryptography.RNGCryptoServiceProvider]::new()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

function New-Passphrase {
    # This one gets read off the screen and typed into a browser, so the
    # alphabet leaves out the characters people confuse: 0/O and 1/l/I.
    $alphabet = 'abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789'
    $bytes = New-Object byte[] 20
    $rng = [System.Security.Cryptography.RNGCryptoServiceProvider]::new()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    $sb = New-Object System.Text.StringBuilder
    foreach ($b in $bytes) { [void]$sb.Append($alphabet[$b % $alphabet.Length]) }
    return $sb.ToString()
}

# =============================================================================
#  .env handling
#
#  Written with Unix line endings on purpose. A stray carriage return at the
#  end of a value has historically travelled all the way into a database
#  password and produced an "access denied" that nothing explains.
# =============================================================================

function Get-EnvValue {
    param([string]$Path, [string]$Key)
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        if ($line -match "^$([regex]::Escape($Key))=(.*)$") { return $Matches[1].Trim() }
    }
    return ''
}

# ── artifacts.json, the one place that says where the big files live ─────────
#
# The same pointer install.sh reads, and it has to be read here too or this
# installer downloads the old combined package: fetch-sources.sh falls back to
# the address compiled into it, which is 1.6 GB of server AND desktop client,
# fetched in full even by somebody who chose the browser client.
#
# There is no checkout on this PC to read it from -- the project is cloned
# inside a container -- so it is fetched from the same place this script came
# from, and kept beside docker-compose.yml once there is an install directory.
$script:PointerPath = ''
function Get-PointerFile {
    if ($script:PointerPath -and (Test-Path -LiteralPath $script:PointerPath)) {
        return $script:PointerPath
    }
    foreach ($c in @(
        (Join-Path $script:InstallDir 'artifacts.json'),
        $(if ($script:RepoDir) { Join-Path $script:RepoDir 'artifacts.json' } else { $null })
    )) {
        if ($c -and (Test-Path -LiteralPath $c -PathType Leaf)) {
            $script:PointerPath = $c
            return $c
        }
    }
    $raw = $script:SelfUrl -replace '/installer/install\.ps1$', '/artifacts.json'
    $dest = Join-Path $env:TEMP 'm2-artifacts.json'
    try {
        Invoke-WebRequest -Uri $raw -OutFile $dest -UseBasicParsing -TimeoutSec 60
        if ((Test-Path -LiteralPath $dest) -and (Get-Item $dest).Length -gt 0) {
            $script:PointerPath = $dest
            return $dest
        }
    } catch { }
    return ''
}

function Get-PointerValue {
    param([string]$Key)
    $f = Get-PointerFile
    if (-not $f) { return '' }
    $text = [IO.File]::ReadAllText($f)
    $m = [regex]::Match($text, '"' + [regex]::Escape($Key) + '"\s*:\s*"([^"]*)"')
    if ($m.Success) { return $m.Groups[1].Value }
    $m = [regex]::Match($text, '"' + [regex]::Escape($Key) + '"\s*:\s*([0-9]+)')
    if ($m.Success) { return $m.Groups[1].Value }
    return ''
}

# A value that is not a link has not been filled in yet. Every reader of that
# file makes this check rather than attempting a meaningless download.
function Get-PointerLink {
    param([string]$Key)
    $v = Get-PointerValue $Key
    if ($v -match '^https?://') { return $v }
    return ''
}

function Set-EnvValue {
    param([string]$Path, [string]$Key, [string]$Value)
    $lines = @()
    if (Test-Path -LiteralPath $Path) { $lines = @([IO.File]::ReadAllLines($Path)) }
    $done = $false
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($line in $lines) {
        if (-not $done -and $line -match "^$([regex]::Escape($Key))=") {
            $out.Add("$Key=$Value"); $done = $true
        } else { $out.Add($line) }
    }
    if (-not $done) { $out.Add("$Key=$Value") }
    [IO.File]::WriteAllText($Path, (($out -join "`n") + "`n"), (New-Object System.Text.UTF8Encoding($false)))
}

function Assert-ValidPortConfiguration {
    if ($script:AuthPort -lt 1 -or $script:AuthPort -gt 65535) {
        throw "-AuthPort must be a number from 1 to 65535."
    }
    if ($script:PanelPort -lt 1 -or $script:PanelPort -gt 65535) {
        throw "-PanelPort must be a number from 1 to 65535."
    }
    if ($script:GamePorts -notmatch '^(\d+)-(\d+)$') {
        throw "-GamePorts must be a range of exactly three ports, for example 13000-13002."
    }
    $first = [int]$Matches[1]
    $last = [int]$Matches[2]
    if ($first -lt 1 -or $last -gt 65535 -or $last -ne ($first + 2)) {
        throw "-GamePorts must be a valid range of exactly three consecutive ports, for example 13000-13002."
    }
}

# =============================================================================
#  docker compose, always in the right directory
# =============================================================================

# Both take an explicit array rather than remaining arguments: almost every
# compose argument we pass starts with a dash ("-d", "-T", "--build"), and
# PowerShell would try to bind those as parameters of this function.
function Invoke-Compose {
    param([string[]]$ComposeArgs)
    Push-Location $script:InstallDir
    # docker compose writes its whole build log -- every "=> [ 4/17] RUN ..."
    # line of it -- to standard ERROR, and only the result to standard output.
    # That is normal for it and means nothing is wrong. But a native program's
    # standard error becomes an error record the moment anything downstream is
    # collecting streams, and the $ErrorActionPreference = 'Stop' at the top of
    # this file then makes that record terminating: the first progress line of
    # a perfectly healthy build killed the installer with
    # "Image metin2/game:40250 Building" as its entire explanation.
    #
    # It only showed up when the output was being captured -- run in a console
    # by hand it was invisible -- so it is exactly the kind of thing that would
    # otherwise be found by the first person to write `install.ps1 > log.txt'.
    # Lowered for the length of the call, as with Invoke-Native above.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # Out-Host, rather than letting the output fall through to the pipeline:
        # PowerShell collects everything a function writes to the success stream
        # into that function's return value. The exit code would then be the
        # *last* of several hundred returned objects, and the caller's `-ne 0'
        # test would compare an array -- which is true whenever the array is
        # non-empty, i.e. after every build that printed anything at all. A
        # perfectly good install reported itself as a failure that way.
        & docker compose @ComposeArgs 2>&1 | Out-Host
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
        Pop-Location
    }
}

function Invoke-ComposeQuiet {
    param([string[]]$ComposeArgs)
    Push-Location $script:InstallDir
    try {
        return (Invoke-Native 'docker' (@('compose') + $ComposeArgs))
    } finally { Pop-Location }
}

# The passphrase the operator chose in the panel, if they have.
#
# The panel writes it there when somebody uses its "change passphrase" form,
# because a hash cannot be printed and this script promises to show the
# passphrase at the end of every run. An empty string is the normal answer and
# means nobody has changed it, so .env still holds the right one.
#
# Two attempts on purpose: `exec' is cheap and works while the stack is up,
# which it is during an update; `run' starts a throwaway container off the same
# volume and covers a stack that happens to be stopped. --no-deps so it does not
# start the database to read one line, and --entrypoint because the panel
# image's own entrypoint would wait for that database.
function Get-PanelChosenPassphrase {
    if ($script:DryRun -or $script:FreshInstall) { return '' }
    $f = '/usr/local/etc/panel.passphrase'
    $r = Invoke-ComposeQuiet @('exec', '-T', 'panel', 'cat', $f)
    if ($r.Code -eq 0 -and $r.Output) { return $r.Output.Trim() }
    $r = Invoke-ComposeQuiet @('run', '--rm', '-T', '--no-deps', '--entrypoint', 'cat', 'panel', $f)
    if ($r.Code -eq 0 -and $r.Output) { return $r.Output.Trim() }
    return ''
}

# The compose file names its own project and its own containers, so read them
# out of it rather than assuming. A stack that is renamed -- or a second one
# built for testing -- then still works.
function Get-StackProject {
    $f = Join-Path $script:InstallDir 'docker-compose.yml'
    if (Test-Path -LiteralPath $f) {
        foreach ($line in [IO.File]::ReadAllLines($f)) {
            if ($line -match '^name:\s*(.+?)\s*$') { return $Matches[1].Trim('"', "'") }
        }
    }
    return (Split-Path -Leaf $script:InstallDir)
}

function Get-StackContainerNames {
    $f = Join-Path $script:InstallDir 'docker-compose.yml'
    $names = @()
    if (Test-Path -LiteralPath $f) {
        foreach ($line in [IO.File]::ReadAllLines($f)) {
            if ($line -match '^\s*container_name:\s*(.+?)\s*$') { $names += $Matches[1].Trim('"', "'") }
        }
    }
    return $names
}

# =============================================================================
#  Step 1 -- can this PC run a Metin2 server?
# =============================================================================

function Test-Machine {
    Write-Step 'Checking this PC'

    # --- Windows version. WSL2, which Docker Desktop needs, wants Windows 10
    #     build 19044 (21H2) or newer, or any Windows 11.
    $os = Get-CimInstance Win32_OperatingSystem
    $build = [int]($os.BuildNumber)
    Write-Info "Windows: $($os.Caption) (build $build)"
    if ($build -lt 19044) {
        Stop-Friendly @"
This is Windows build $build, and Docker Desktop needs build 19044 or newer
(Windows 10 21H2, or Windows 11).

What to do: run Windows Update until it stops offering updates, then run this
installer again. If Windows cannot update this far, the PC is too old for
Docker Desktop and there is no way around that here.
"@
    }

    # --- Processor. The server is built from the original source, which
    #     produces 32-bit x86 programs. There is no ARM build.
    $arch = $env:PROCESSOR_ARCHITECTURE
    if ($env:PROCESSOR_ARCHITEW6432) { $arch = $env:PROCESSOR_ARCHITEW6432 }
    Write-Info "Processor: $arch"
    if ($arch -match 'ARM') {
        Stop-Friendly @"
This PC has an ARM processor, and the Metin2 server cannot run on it.

This is not something the installer can work around. The server is built from
the original 2000s source code, which produces 32-bit x86 programs. There is
no ARM version, and making one is a large piece of work rather than a setting.

Windows on ARM can emulate x86 programs, but Docker's Linux containers cannot:
the game runs as a Linux program inside Docker, and that layer has no
emulation.

What to do: use a PC with an Intel or AMD processor, or rent a small x86 Linux
VPS (about 5 EUR a month) and use install.sh there instead.
"@
    }
    if ($arch -notmatch 'AMD64|x86') {
        Write-Warn "Unrecognised processor type '$arch'. The server needs Intel or AMD."
        if (-not (Confirm-YesNo 'Carry on anyway?' $false)) { throw (New-Object System.OperationCanceledException 'stopped') }
    }

    # --- Memory. The stack needs ~1 GB to run and more to build, and Windows
    #     plus Docker Desktop plus a browser want their share on top.
    $cs = Get-CimInstance Win32_ComputerSystem
    $ramMb = [int]($cs.TotalPhysicalMemory / 1MB)
    if ($ramMb -lt 3600) {
        Stop-Friendly @"
This PC has about $([math]::Round($ramMb/1024,1)) GB of memory.

The game server alone holds roughly 860 MB, Docker Desktop's Linux virtual
machine takes its own share, and Windows needs the rest. Below 4 GB the build
gets killed part way through, which looks like a random crash and wastes an
afternoon -- so it stops here instead.

What to do: 8 GB is the comfortable size for running this on a PC you are also
playing on. If this PC cannot be upgraded, a small Linux VPS will run it for
about 5 EUR a month -- see install.sh.
"@
    }
    if ($ramMb -lt 7600) {
        Write-Warn "This PC has about $([math]::Round($ramMb/1024,1)) GB of memory."
        Write-Warn 'It will work, but running the server and the game at the same'
        Write-Warn 'time will be tight. 8 GB is the comfortable size.'
        if (-not (Confirm-YesNo 'Carry on?' $true)) { throw (New-Object System.OperationCanceledException 'stopped') }
    } else {
        Write-Info "Memory: $([math]::Round($ramMb/1024,1)) GB -- fine"
    }

    # --- Disk. Docker Desktop keeps its images inside a WSL virtual disk, which
    #     lives on C: unless it has been moved, so C: is what matters even if we
    #     install the stack elsewhere.
    $sysDrive = ($env:SystemDrive)
    $drive = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$sysDrive'"
    if ($drive) {
        $freeGb = [math]::Round($drive.FreeSpace / 1GB, 1)
        if ($freeGb -lt 15) {
            Stop-Friendly @"
There are only $freeGb GB free on $sysDrive.

This needs about 15 GB: 8 GB to build the server and another 7 GB while the
downloadable game client is put together. Docker Desktop keeps all of it in a
virtual disk on $sysDrive whatever folder you install into. It stops here
rather than filling the drive up and failing three quarters of the way through
a twenty-minute build.

What to do: free up space until there are at least 25 GB, then run this again.
"@
        }
        if ($freeGb -lt 30) {
            Write-Warn "$freeGb GB free on $sysDrive. Enough to build, but the game"
            Write-Warn 'writes roughly 40 MB of logs per hour while it runs.'
        } else {
            Write-Info "Disk: $freeGb GB free on $sysDrive -- fine"
        }
    }

    # --- Virtualisation. Docker Desktop cannot start without it, and the error
    #     it gives when it is off in the BIOS is not helpful.
    try {
        $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
        if ($null -ne $cpu.VirtualizationFirmwareEnabled -and -not $cpu.VirtualizationFirmwareEnabled) {
            Write-Warn 'Hardware virtualisation looks switched off in this PC''s BIOS.'
            Write-Warn 'Docker Desktop cannot start without it. If Docker refuses to'
            Write-Warn 'run later, that is why: reboot into the BIOS and turn on'
            Write-Warn '"Intel VT-x" / "AMD-V" / "SVM Mode".'
        }
    } catch { }

    # Nothing else is needed from Windows itself. Everything that unpacks,
    # patches or copies the server runs inside a container -- see the note at
    # the top of this file -- so there is no tar.exe, no git.exe and no
    # 7-Zip to check for here.
}

# =============================================================================
#  Step 2 -- Docker Desktop
# =============================================================================

function Test-DockerRunning {
    if (-not (Test-Command 'docker')) { return $false }
    try {
        return ((Invoke-Native 'docker' @('info')).Code -eq 0)
    } catch { return $false }
}

function Test-ComposeAvailable {
    try {
        return ((Invoke-Native 'docker' @('compose', 'version')).Code -eq 0)
    } catch { return $false }
}

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-DockerDesktopExe {
    foreach ($p in @(
        "$env:ProgramFiles\Docker\Docker\Docker Desktop.exe",
        "${env:ProgramFiles(x86)}\Docker\Docker\Docker Desktop.exe",
        "$env:LOCALAPPDATA\Docker\Docker Desktop.exe")) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

function Initialize-Docker {
    Write-Step 'Docker Desktop'

    if ((Test-DockerRunning) -and (Test-ComposeAvailable)) {
        Write-Good 'Docker Desktop is installed and running.'
        Write-Info (Invoke-Native 'docker' @('--version')).Output
        return
    }

    $exe = Find-DockerDesktopExe

    # ------------------------------------------------ installed but not started
    if ($exe) {
        Write-Say 'Docker Desktop is installed but not running yet. Starting it...'
        Write-Say 'The first start takes a minute or two while its Linux virtual'
        Write-Say 'machine boots.'
        if ($script:DryRun) { Write-Info "[dry-run] start $exe"; return }
        try { Start-Process -FilePath $exe | Out-Null } catch { }

        $deadline = (Get-Date).AddMinutes(4)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Seconds 5
            if ((Test-DockerRunning) -and (Test-ComposeAvailable)) {
                Write-Good 'Docker Desktop is running.'
                return
            }
            Write-Host '.' -NoNewline
        }
        Write-Host ''
        Stop-Friendly @"
Docker Desktop did not finish starting within four minutes.

This is normal on a very first start on a slow PC -- but it can also mean it
is waiting for you to click something.

What to do:
  1. Open Docker Desktop from the Start menu.
  2. Accept its licence terms if it asks, and wait until the whale icon in the
     bottom-left corner stops animating and says "Engine running".
  3. Run this installer again. It will notice Docker is ready and carry on.

If it says virtualisation is not enabled, that is a BIOS setting: reboot into
the BIOS and turn on "Intel VT-x" / "AMD-V" / "SVM Mode".
"@
    }

    # ------------------------------------------------------- not installed yet
    Write-Say 'Docker Desktop is not installed. It is what actually runs the'
    Write-Say 'server: a small Linux system inside Windows. It is free for'
    Write-Say 'personal use and about a 600 MB download.'
    Write-Say ''

    if ($script:DryRun) {
        Write-Info '[dry-run] winget install Docker.DockerDesktop'
        return
    }

    if (-not (Test-Command 'winget')) {
        Stop-Friendly @"
Docker Desktop is not installed, and this PC does not have winget either, so
the installer cannot fetch it for you.

What to do -- it is three steps and none of them are hard:

  1. Go to  https://www.docker.com/products/docker-desktop/
  2. Download and run "Docker Desktop for Windows". Leave every option at its
     default. Make sure "Use WSL 2 instead of Hyper-V" stays ticked.
  3. Restart your PC when it asks, open Docker Desktop once and wait for it to
     say "Engine running".

Then run this installer again -- the same one line -- and it will carry
straight on from here.
"@
    }

    if (-not (Test-IsAdmin)) {
        Stop-Friendly @"
Docker Desktop has to be installed by an administrator, and this PowerShell
window is not running as one.

What to do:
  1. Click the Start button and type: powershell
  2. Right-click "Windows PowerShell" and choose "Run as administrator".
  3. Paste the same one line you used before and press Enter.

Everything after the Docker install works without administrator rights; it is
only this step that needs it.
"@
    }

    if (-not (Confirm-YesNo 'Install Docker Desktop now?' $true)) {
        Stop-Friendly @"
Nothing was installed.

Docker Desktop is required -- it is the piece that actually runs the server.
When you are ready, run this installer again.
"@
    }

    Write-Say 'Installing Docker Desktop. This takes several minutes and the'
    Write-Say 'screen may look idle for a while. Please do not close this window.'

    # Through Invoke-Native, not directly: winget narrates on standard error,
    # and a redirected native standard error under $ErrorActionPreference =
    # 'Stop' is a terminating error -- see the note on Invoke-Native.
    $code = (Invoke-Native 'winget' @('install', '--id', 'Docker.DockerDesktop',
                                      '--exact', '--silent',
                                      '--accept-source-agreements',
                                      '--accept-package-agreements')).Code

    if ($code -ne 0 -and $code -ne -1978335189) {   # -1978335189 = already installed
        Stop-Friendly @"
The automatic Docker Desktop install did not succeed (winget returned $code).

What to do instead -- this always works:

  1. Go to  https://www.docker.com/products/docker-desktop/
  2. Download and run "Docker Desktop for Windows", leaving every option at
     its default.
  3. Restart your PC when it asks, then open Docker Desktop once and wait for
     it to say "Engine running".

Then run this installer again and it will carry on from here.
"@
    }

    # Docker Desktop needs a reboot (or at least a sign-out) on a first install:
    # it adds you to the "docker-users" group, and Windows only reads group
    # membership when you sign in. Carrying on now would fail in a way that
    # looks like a permission bug.
    Write-Host ''
    Write-Good 'Docker Desktop is installed.'
    Write-Host ''
    Write-Host '  ================================================================' -ForegroundColor Yellow
    Write-Host '    ONE RESTART, AND THEN YOU ARE ALMOST DONE' -ForegroundColor Yellow
    Write-Host '  ================================================================' -ForegroundColor Yellow
    Write-Host ''
    Write-Say  'Docker Desktop has just given your Windows account permission to'
    Write-Say  'use it, and Windows only notices that when you sign in again.'
    Write-Say  'So the server cannot be installed in this same session.'
    Write-Say  ''
    Write-Say  'Please do this:'
    Write-Say  ''
    Write-Host '    1. Restart your PC.' -ForegroundColor White
    Write-Host '    2. Open Docker Desktop from the Start menu and wait until it' -ForegroundColor White
    Write-Host '       says "Engine running" at the bottom left.' -ForegroundColor White
    Write-Host '    3. Open PowerShell again and paste the same one line.' -ForegroundColor White
    Write-Say  ''
    Write-Say  'The installer will see that Docker is ready and go straight on to'
    Write-Say  'installing the server. Nothing you have done so far is lost.'
    Write-Host ''
    throw (New-Object System.OperationCanceledException 'reboot required')
}

# `ports: !override' in a compose override file needs Compose v2.24 or newer.
# The Windows install binds everything to 127.0.0.1, which means replacing the
# published port list outright, so this is not optional here.
function Test-ComposeOverrideSupported {
    try {
        $v = (Invoke-Native 'docker' @('compose', 'version', '--short')).Output -replace '^v', ''
        $parts = $v.Split('.')
        $maj = [int]$parts[0]; $min = [int]$parts[1]
        return ($maj -gt 2) -or ($maj -eq 2 -and $min -ge 24)
    } catch { return $false }
}

# =============================================================================
#  Step 3 -- get the server onto this PC
# =============================================================================

function Test-ContextComplete {
    param([string]$Dir)
    (Test-Path -LiteralPath (Join-Path $Dir 'docker-compose.yml')) -and
    (Test-Path -LiteralPath (Join-Path $Dir 'game\src\server')) -and
    (Test-Path -LiteralPath (Join-Path $Dir 'panel\app\admin_panel.py')) -and
    (Test-Path -LiteralPath (Join-Path $Dir 'mariadb\initdb.d\dumps')) -and
    (Test-Path -LiteralPath (Join-Path $Dir 'mariadb\playerbot\apply.sh') -PathType Leaf) -and
    ((Get-Item -LiteralPath (Join-Path $Dir 'mariadb\playerbot\apply.sh')).Length -gt 0) -and
    (Test-Path -LiteralPath (Join-Path $Dir 'mariadb\playerbot\playerbots_seed.sql') -PathType Leaf) -and
    ((Get-Item -LiteralPath (Join-Path $Dir 'mariadb\playerbot\playerbots_seed.sql')).Length -gt 0)
}

function Stop-IncompleteContext {
    param([string]$Dir)
    Stop-Friendly @"
The Docker build context in

    $Dir

is not complete: the game source, database dumps, or Playerbot seed are missing
from it.

That is exactly what a bare checkout of the project looks like. The project
contains the Linux port and nothing else -- the game itself is not ours to
publish -- so a checkout has to be filled in before it can be built. This
installer normally does that step itself; run it without M2_LOCAL_CONTEXT and
it will.
"@
}

# =============================================================================
#  Step 3a -- the helper container
#
#  linux-port/fetch-sources.sh is POSIX shell. Windows cannot run it, and the
#  three ways out of that are:
#
#    * translate it into PowerShell. 43 KB of logic that already knows every
#      way MEGA fails -- an anonymous share that answers the API, resolves the
#      filename and then returns 509 for every chunk while megatools retries
#      forever having written nothing. Rewriting that means discovering all of
#      it again, in a second language, and then maintaining two of them.
#
#    * drive WSL. Docker Desktop may be on the Hyper-V backend, and even on the
#      WSL2 backend the distributions it keeps are not general-purpose ones you
#      can run a shell script in. A PC with Docker working and no usable WSL
#      distribution is an ordinary PC, not a broken one.
#
#    * run it in a container, which is what happens here. Docker is already
#      installed and running by this point -- the installer insisted on it two
#      steps ago -- so there is nothing new to install, no elevation, and no
#      chicken-and-egg: the image is stock Debian plus a handful of packages,
#      and does not depend on any part of this project.
#
#  It also means Windows and Linux run the same script, byte for byte.
# =============================================================================

function Initialize-FetcherImage {
    if ((Invoke-Native 'docker' @('image', 'inspect', $script:FetcherImage)).Code -eq 0) {
        Write-Info "helper image $($script:FetcherImage) is already here"
        return
    }

    Write-Say 'Building a small helper image -- Debian plus the handful of'
    Write-Say 'programs that unpack and patch the server files. About a minute,'
    Write-Say 'and only the first time.'

    $dir = Join-Path ([IO.Path]::GetTempPath()) ('m2fetch-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    try {
        # megatools is the reason this is Debian and not Alpine: Alpine has no
        # megatools package in main or community, and without it the default
        # MEGA link cannot be downloaded at all. Debian also makes the "install
        # this package" advice that fetch-sources.sh prints -- which is written
        # in apt-get -- true rather than misleading.
        $dockerfile = @(
            '# Written by install.ps1. Remove it with:'
            "#     docker image rm $($script:FetcherImage)"
            'FROM debian:12-slim'
            'RUN apt-get update \'
            ' && apt-get install -y --no-install-recommends \'
            '      bash tar patch findutils coreutils diffutils grep sed gawk \'
            '      unzip p7zip-full megatools git curl ca-certificates python3 \'
            ' && rm -rf /var/lib/apt/lists/*'
            ''
        ) -join "`n"
        [IO.File]::WriteAllText((Join-Path $dir 'Dockerfile'), $dockerfile,
                                (New-Object System.Text.UTF8Encoding($false)))

        $code = Invoke-DockerLoud @('build', '-t', $script:FetcherImage, $dir)
        if ($code -ne 0) {
            Stop-Friendly @"
The helper image could not be built. The output above says why.

It is a stock Debian image plus a few packages, so the usual cause is that
this PC could not reach Docker Hub or the Debian package mirrors -- a company
proxy, a VPN, or simply no internet at that moment.

Nothing has been installed and nothing has been left running. When the
connection is working, run this installer again.
"@
        }
    } finally {
        Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Good 'Helper image ready.'
}

# A path Docker will accept on the left of a -v. A trailing backslash is the
# one that bites: PowerShell hands native programs a rebuilt command line, and
# "C:\foo\" ends up escaping the quote that follows it.
function ConvertTo-MountPath {
    param([string]$Path)
    return ((Resolve-Path -LiteralPath $Path).ProviderPath.TrimEnd('\'))
}

# Put the project inside the volume, so that everything after this happens on
# the container's own filesystem: no Windows path lengths, no bind-mount
# overhead on half a gigabyte of small files, and the user's own checkout is
# never written into.
function Copy-ProjectIntoVolume {
    Invoke-Native 'docker' @('volume', 'create', $script:SrcVolume) | Out-Null

    if ($script:RepoDir) {
        if (-not (Test-Path -LiteralPath $script:RepoDir -PathType Container)) {
            Stop-Friendly "M2_REPO_DIR points at '$($script:RepoDir)', which is not a folder."
        }
        $src = ConvertTo-MountPath $script:RepoDir
        Write-Say "Taking the project from $src ..."
        # .git is skipped because it can be far larger than the checkout, and
        # docker/game/src because prepare-context.sh rebuilds it anyway -- on a
        # developer's PC that one directory is 300 MB of files we would copy in
        # only to delete.
        $sh = 'set -e; rm -rf /work/repo; mkdir -p /work/repo; ' +
              'tar cf - -C /src --exclude=./.git --exclude=./linux-port/docker/game/src . ' +
              '| tar xf - -C /work/repo; ' +
              'test -f /work/repo/linux-port/fetch-sources.sh'
        $code = Invoke-DockerLoud @(
            'run', '--rm',
            '-v', "$($src):/src:ro",
            '-v', "$($script:SrcVolume):/work",
            $script:FetcherImage, 'bash', '-c', $sh)
        if ($code -ne 0) {
            Stop-Friendly @"
The project could not be copied out of

    $src

Either that folder is not a checkout of this project -- it must be the top of
it, the folder with installer\ and linux-port\ inside -- or Docker Desktop is
not allowed to read it.

If it is on a drive other than C:, open Docker Desktop -> Settings ->
Resources -> File sharing and add the drive, then run this installer again.
"@
        }
        Write-Good 'Project files in place.'
        return
    }

    if ($script:RepoUrl -like 'REPLACE_ME*' -or [string]::IsNullOrWhiteSpace($script:RepoUrl)) {
        Stop-Friendly @"
This copy of the installer does not know where to get the project from -- the
repository URL placeholder was never filled in, which means you are running a
development copy of install.ps1.

Point it at a checkout you already have:

    `$env:M2_REPO_DIR = 'C:\path\to\checkout'

or give it the repository:

    `$env:M2_REPO_URL = 'https://.../server.git'
"@
    }

    Write-Say "Getting the project from $($script:RepoUrl) ..."
    Write-Say 'A few megabytes -- this is the port, not the game.'
    # git runs in the container too, so Windows needs none installed.
    $sh = 'set -e; if [ -d /work/repo/.git ]; then ' +
          'cd /work/repo && git fetch --depth 1 origin && git reset --hard FETCH_HEAD; ' +
          'else rm -rf /work/repo && git clone --depth 1 "$0" /work/repo; fi; ' +
          'test -f /work/repo/linux-port/fetch-sources.sh'
    $code = Invoke-DockerLoud @(
        'run', '--rm',
        '-v', "$($script:SrcVolume):/work",
        $script:FetcherImage, 'bash', '-c', $sh, $script:RepoUrl)
    if ($code -ne 0) {
        Stop-Friendly @"
The project could not be downloaded from

    $($script:RepoUrl)

The output above says why. Check that this PC can reach that address, and that
the address is right.

Nothing has been installed and nothing has been left running.
"@
    }
    Write-Good 'Project files in place.'
}

# Everything the operator may have handed us lives on Windows and has to be
# reachable from inside the container, so each one becomes a read-only mount.
function Invoke-SourceFetch {
    $runArgs = @('run', '--rm', '--name', 'metin2-src-fetch',
                 '-v', "$($script:SrcVolume):/work")
    $tailArgs = @()

    if ($script:SrcRefDir) {
        if (-not (Test-Path -LiteralPath $script:SrcRefDir)) {
            Stop-Friendly "M2_SRC_REFERENCE_DIR points at '$($script:SrcRefDir)', which does not exist."
        }
        $p = ConvertTo-MountPath $script:SrcRefDir
        $runArgs  += @('-v', "$($p):/reference:ro")
        $tailArgs += @('--reference-dir', '/reference')
        Write-Info "using the unpacked server files in $p"
    }
    elseif ($script:SrcArchive) {
        if (-not (Test-Path -LiteralPath $script:SrcArchive -PathType Leaf)) {
            Stop-Friendly "M2_SRC_ARCHIVE points at '$($script:SrcArchive)', which is not a file."
        }
        $f = ConvertTo-MountPath $script:SrcArchive
        # Mounted as a file rather than as its folder, so nothing else in that
        # folder is exposed to the container -- but under its own name, because
        # fetch-sources.sh chooses between unzip, 7z and tar by looking at the
        # extension. Mounted as "/archive/package" a .rar would arrive with no
        # extension at all and fall through to the unzip guess, which cannot
        # read it.
        $leaf = Split-Path -Leaf $f
        $runArgs  += @('-v', "$($f):/archive/$($leaf):ro")
        $tailArgs += @('--archive', "/archive/$leaf")
        Write-Info "using the package $f"

        # The SQL dumps are not inside metin2_server+src.tar.gz -- they are a
        # zip of their own, sitting next to it in Server\. Given only the
        # tarball, fetch-sources.sh looks for that sibling by path, so on
        # Windows it has to be carried into the container as well or the
        # assembly stops one step from the end with "the database cannot be
        # created without it".
        $sibling = Join-Path (Split-Path -Parent $f) 'metin2_mysql_dump.zip'
        if ((Test-Path -LiteralPath $sibling -PathType Leaf) -and
            ($leaf -notlike '*mysql_dump*')) {
            $runArgs += @('-v', "$($sibling):/archive/metin2_mysql_dump.zip:ro")
            Write-Info 'and metin2_mysql_dump.zip from beside it'
        }
    }
    else {
        # No project-owned download source is built in. An explicit operator
        # URL still works; artifacts.json is consulted only as optional local
        # metadata and normally contains an empty URL.
        $url = $script:SrcUrl
        $sha = if ($env:M2_SRC_SHA256) { $env:M2_SRC_SHA256 } else { '' }
        if (-not $url) {
            $url = Get-PointerLink 'src_server_url'
            if ($url) {
                $sha = Get-PointerValue 'src_server_sha256'
                Write-Info "server files: $(Get-PointerValue 'src_server_file') (from artifacts.json)"
            }
        }
        if ($url) { $runArgs += @('-e', "M2_SRC_URL=$url") }
        if ($sha -match '^[0-9a-f]{64}$') { $runArgs += @('-e', "M2_SRC_SHA256=$sha") }

        # Optional operator-owned fallbacks, tried only after the primary URL.
        $fb1 = if ($env:M2_SRC_URL_FALLBACK)  { $env:M2_SRC_URL_FALLBACK }
               else { Get-PointerLink 'src_server_url_fallback' }
        $fb2 = if ($env:M2_SRC_URL_FALLBACK2) { $env:M2_SRC_URL_FALLBACK2 }
               else { Get-PointerLink 'src_server_url_fallback2' }
        if ($fb1) { $runArgs += @('-e', "M2_SRC_URL_FALLBACK=$fb1") }
        if ($fb2) { $runArgs += @('-e', "M2_SRC_URL_FALLBACK2=$fb2") }

        Write-Say 'Looking for a compatible r40250 package already present in the'
        Write-Say 'source cache. If this is a clean install, provide -Archive or'
        Write-Say '-ReferenceDir; this project does not publish game files.'
        Write-Say ''
        Write-Say 'Once supplied, it is kept in a Docker volume so later updates'
        Write-Say 'do not need the original local path again.'
    }

    # The movement-speed bonus has to be known HERE, before the build context is
    # staged: the quest that applies it is compiled into the game image, and a
    # compiled quest cannot read an environment variable. prepare-context.sh
    # writes the number into the quest before qc sees it.
    #
    # Read back out of .env on an update so the setting survives; on a first
    # install it comes from the environment ($env:M2_MOVE_SPEED_BONUS = '100').
    # Select-CustomExperience may already have put a number here -- the 20 that
    # comes with switching the Custom Experience on -- and that beats the .env
    # of an install which did not have it yet, which is exactly the case where
    # .env still says 0.
    $speed = $env:M2_MOVE_SPEED_BONUS
    if (-not $speed) { $speed = $script:MoveSpeedBonus }
    if (-not $speed) { $speed = Get-EnvValue (Join-Path $script:InstallDir '.env') 'M2_MOVE_SPEED_BONUS' }
    if ($speed -notmatch '^[0-9]+$') { $speed = '0' }
    $script:MoveSpeedBonus = $speed
    $runArgs += @('-e', "M2_MOVE_SPEED_BONUS=$speed")

    # The Custom Experience is decided in the same place and for the same
    # reason: prepare-context.sh, which runs inside this container, is the one
    # moment at which the staged tree exists and the image has not been built
    # from it yet. Select-CustomExperience has answered this already.
    $runArgs += @('-e', "M2_CUSTOM_EXPERIENCE=$(if ($script:CustomExperience) {'1'} else {'0'})")
    if ($script:HighRisk) { $runArgs += @('-e', "M2_HIGH_RISK=$($script:HighRisk)") }

    $runArgs += @($script:FetcherImage,
                  'sh', '/work/repo/linux-port/fetch-sources.sh', 'fetch',
                  '--cache', '/work/cache')
    $runArgs += $tailArgs

    Write-Host ''
    try {
        $code = Invoke-DockerLoud $runArgs
    } finally {
        # --rm covers the ordinary exits. This covers Ctrl-C, which leaves the
        # container behind and would then collide by name on the next run.
        Invoke-Native 'docker' @('rm', '-f', 'metin2-src-fetch') | Out-Null
    }

    if ($code -eq 0) {
        Write-Good 'The server is assembled.'
        return
    }

    # fetch-sources.sh gives every kind of failure its own exit code precisely
    # so that this can say something useful rather than "it did not work".
    switch ($code) {
        3 { Stop-Friendly @"
The helper container is missing a program the assembly needs -- the line above
names it. That is a fault in this installer rather than anything you did;
please report it with the output above.
"@ }
        4 { Stop-Friendly @"
The compatible r40250 server-file package was not supplied.

This repository intentionally contains no game server source, game data or
download mirror. Nothing was installed and no server was left half-built.
Use a local package that you are authorised to use, then run:

        `$env:M2_SRC_ARCHIVE = 'C:\Users\$($env:USERNAME)\Downloads\package.zip'
        irm https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.ps1 | iex

Or, if you have already unpacked it:

        `$env:M2_SRC_REFERENCE_DIR = 'C:\path\to\[40250] Reference Serverfile'
"@ }
        5 { Stop-Friendly @"
The server-file package was found, but it is not the r40250 package this port
expects -- something it must contain is not in it, and the line above says
which.

If you pointed the installer at a file or a folder, check that it really is
the "[40250] Reference Serverfile" package: the one with Server\ and Client\
inside it. If it was downloaded, it may have been cut short -- run the
installer again and it will notice.
"@ }
        6 { Stop-Friendly @"
The Linux port does not apply to this source.

That is a precise answer rather than a vague failure: the server-file package
on this PC is not the r40250 one the port was made against. It is not a fault
in the port, and it must not be forced -- a half-applied port compiles happily
and then produces a server that does not work.

What to do: use the r40250 package. Nothing was installed.
"@ }
        7 { Stop-Friendly @"
Docker ran out of disk space while assembling the server.

It needs about 4 GB inside its own virtual disk while it works, on top of the
15 GB for the build itself. Free some space on $($env:SystemDrive) -- or, in
Docker Desktop, Settings -> Resources -> Advanced, give its disk image a
larger limit -- and run this installer again.
"@ }
        8 { Stop-Friendly @"
The build context could not be filled in: the last step failed and its output
is above. Everything before it succeeded, so running this installer again will
not download anything a second time.

This is a bug rather than something you did; please report it with the output
above.
"@ }
        default { Stop-Friendly @"
Assembling the server failed (the helper exited with code $code). Its output
is above.

Nothing was installed and no server was left half-built. Running this
installer again is safe and picks up where this stopped.
"@ }
    }
}

# The finished context is inside the volume. Lift it out with docker cp, which
# is the one copy that has to land on Windows -- and the only reason the
# install folder has to be short.
function Export-BuildContext {
    Write-Say "Copying the server into $($script:InstallDir) ..."
    $created = Invoke-Native 'docker' @('create', '-v', "$($script:SrcVolume):/work",
                                        $script:FetcherImage, 'true')
    if ($created.Code -ne 0 -or -not $created.Output) {
        Stop-Friendly "Docker would not open the volume holding the assembled server:`n$($created.Output)"
    }
    $cid = ($created.Output -split "`n" | Where-Object { $_.Trim() } | Select-Object -Last 1).Trim()
    try {
        New-Item -ItemType Directory -Force -Path $script:InstallDir | Out-Null
        # The trailing "/." copies the *contents* of the directory, hidden
        # entries included -- .env.example and .gitignore both start with a dot
        # and both belong in the install.
        $code = Invoke-DockerLoud @('cp', "$($cid):/work/repo/linux-port/docker/.",
                                    $script:InstallDir)
        if ($code -ne 0) {
            Stop-Friendly @"
The server files could not be copied into

    $($script:InstallDir)

The most likely cause is Windows' 260-character path limit: the game carries
quest files about 125 characters below whatever folder you choose. Install
somewhere shorter -- C:\Metin2Server, say:

    iex "& { `$(irm https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.ps1) } -InstallDir C:\Metin2Server"
"@
        }
        # Two files the browser client's fetcher needs live OUTSIDE
        # linux-port/docker -- fetch-web-client.sh one level up, artifacts.json
        # at the top of the repository -- while the install directory is a copy
        # of the docker folder alone. Mounting them with ../ from the compose
        # file therefore pointed at nothing on a real install, and Docker answers
        # a missing bind source by creating an empty DIRECTORY, so the fetcher
        # started with a directory where its script should have been. Copied in
        # so every path in docker-compose.yml stays inside this one folder.
        foreach ($extra in @('linux-port/fetch-web-client.sh', 'artifacts.json')) {
            $rc = Invoke-DockerLoud @('cp', "$($cid):/work/repo/$extra", $script:InstallDir)
            if ($rc -ne 0) {
                Write-Warn "Could not copy $extra -- the browser client cannot be fetched."
            }
        }
    } finally {
        Invoke-Native 'docker' @('rm', '-f', $cid) | Out-Null
    }
}

# Windows still refuses to create a file whose whole path is longer than 260
# characters unless long-path support has been switched on, and it is off by
# default. The deepest thing in the release -- a quest script buried under
# game/src/serverfiles/share/locale -- sits about 130 characters below the
# install folder, so a long folder name fails a third of the way through the
# copy with a raw .NET error in whatever language Windows happens to be in.
# Caught here, while it can still be explained and acted on.
function Test-InstallPathLength {
    try {
        $v = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' `
                -Name 'LongPathsEnabled' -ErrorAction SilentlyContinue
        if ($v -and [int]$v.LongPathsEnabled -eq 1) { return }
    } catch { }

    $room = 260 - 130
    if ($script:InstallDir.Length -le $room) { return }

    Stop-Friendly @"
The folder to install into is too deep for Windows:

    $($script:InstallDir)

That path is $($script:InstallDir.Length) characters long. The server carries quest files about 130
characters below whatever folder you choose, and Windows will not create a
file whose full path passes 260 characters -- so the copy would stop part of
the way through with an error that explains nothing.

What to do: install somewhere shorter -- $room characters or fewer. The default
is well inside that:

    -InstallDir C:\Users\$($env:USERNAME)\Metin2Server
"@
}

# The version this machine is running, out of the build context the panel image
# was made from. Empty when it cannot be told, which is not an error: every
# server installed before versions existed has no such file, and right now
# those outnumber the ones that do.
function Get-InstalledVersion {
    $f = Join-Path $script:InstallDir 'panel\app\VERSION'
    if (Test-Path -LiteralPath $f) {
        $v = (Get-Content -LiteralPath $f -First 1 -ErrorAction SilentlyContinue)
        if ($v) { return $v.Trim() }
    }
    return ''
}

# The version published in the repository. Empty when it cannot be fetched --
# no network, GitHub unreachable, a proxy in the way. Never a reason to stop.
function Get-PublishedVersion {
    $url = if ($env:M2_VERSION_URL) { $env:M2_VERSION_URL }
           else { $script:SelfUrl -replace '/installer/install\.ps1$', '/VERSION' }
    try {
        $r = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 12 -ErrorAction Stop
        $v = ($r.Content -split "`n")[0].Trim()
        if ($v -match '^\d{1,5}\.\d{1,5}\.\d{1,5}$') { return $v }
    } catch { }
    return ''
}

# a.b.c > x.y.z ? Compared number by number, not as text -- 1.1.10 is newer
# than 1.1.9, and a string comparison gets that backwards.
function Test-VersionNewer([string]$a, [string]$b) {
    if (-not $a) { return $false }
    if (-not $b) { return $true }
    $x = $a -split '\.'; $y = $b -split '\.'
    for ($i = 0; $i -lt 3; $i++) {
        $p = 0; $q = 0
        [void][int]::TryParse($x[$i], [ref]$p)
        [void][int]::TryParse($y[$i], [ref]$q)
        if ($p -gt $q) { return $true }
        if ($p -lt $q) { return $false }
    }
    return $false
}

# Is there already a server here? Get-Stack works this out too, from the same
# file -- but the client question is asked BEFORE it now, and it needs the
# answer: on a first install there is nothing to detect, and asking the panel
# would build its image just to be told so.
function Test-ExistingInstall {
    if (Test-Path -LiteralPath (Join-Path $script:InstallDir 'docker-compose.yml')) {
        $script:FreshInstall = $false
    }
}

function Get-Stack {
    Write-Step 'The server files'

    if (Test-Path -LiteralPath (Join-Path $script:InstallDir 'docker-compose.yml')) {
        $script:FreshInstall = $false
        Write-Good "A server is already installed in $($script:InstallDir)."
        Write-Say ''
        Write-Say 'Nothing here will be deleted. Your accounts, characters, items'
        Write-Say 'and guilds live in a Docker volume that this installer never'
        Write-Say 'touches -- only "docker compose down -v" would remove them, and'
        Write-Say 'this script never runs that. Your settings in .env are kept'
        Write-Say 'either way.'
        Write-Say ''

        $have = Get-InstalledVersion
        $new  = Get-PublishedVersion
        if ($have) { Write-Say "  This server:   $have" }
        else       { Write-Say '  This server:   unknown -- installed before versions were added' }
        if ($new)  { Write-Say "  Published:     $new" }
        else       { Write-Say '  Published:     could not be checked just now' }
        Write-Say ''

        # Offer the update when the published version is genuinely newer, and
        # also when the local one cannot be read -- an install with no VERSION
        # predates them, so anything published is newer than it.
        $outdated = $false
        if ($new -and ((-not $have) -or (Test-VersionNewer $new $have))) { $outdated = $true }

        if ($outdated) {
            Write-Say 'Updating rebuilds the server from the published version and'
            Write-Say 'restarts it. Answering no re-applies your settings and'
            Write-Say 'restarts, and leaves the version you have alone.'
            Write-Say ''
            if (Confirm-YesNo "Update this server to $new?" $true) {
                Write-Say ''
                Write-Say "Updating to $new."
                # Falls through on purpose. This used to return here, so
                # re-running the installer rewrote .env, restarted the
                # containers and changed nothing else: the checkout was never
                # refreshed and the build context was never replaced, so the
                # rebuild had nothing new to build. On Windows that was the
                # whole update mechanism, since there is no updater service.
            } else {
                Write-Say ''
                Write-Say 'Keeping the version you have.'
                return
            }
        } else {
            if ($new -and $have) { Write-Say 'That is the newest published version.' }
            Write-Say ''
            if (-not (Confirm-YesNo 'Re-apply the settings and restart the server?' $true)) {
                Write-Say ''
                Write-Say 'Left alone. To manage the existing server:'
                Write-Say "    cd `"$($script:InstallDir)`""
                Write-Say '    docker compose ps'
                throw (New-Object System.OperationCanceledException 'user declined')
            }
            return
        }
    }

    Test-InstallPathLength

    # -- 1. a build context somebody prepared earlier -------------------------
    if ($script:LocalContext) {
        if (-not (Test-Path -LiteralPath $script:LocalContext)) {
            Stop-Friendly "M2_LOCAL_CONTEXT points at '$($script:LocalContext)', which does not exist."
        }
        if (-not (Test-ContextComplete $script:LocalContext)) { Stop-IncompleteContext $script:LocalContext }
        Write-Say "Copying the server from $($script:LocalContext) ..."
        if (-not $script:DryRun) {
            New-Item -ItemType Directory -Force -Path $script:InstallDir | Out-Null
            Copy-DirectoryContents $script:LocalContext $script:InstallDir
        }
        Write-Good 'Server files in place.'
        return
    }

    # -- 2. otherwise: get the project, then assemble the server ---------------
    #
    # If this script is itself sitting in a checkout, that is the obvious place
    # to take the project from and it costs nothing to notice. Run through
    # `irm | iex' there is no such thing, and $script:SelfDir is empty.
    if (-not $script:RepoDir -and $script:SelfDir) {
        $candidate = Split-Path -Parent $script:SelfDir
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'linux-port\fetch-sources.sh'))) {
            $script:RepoDir = $candidate
            Write-Info "using the checkout this installer is part of: $candidate"
        }
    }

    Write-Say 'The game itself is not part of this project and cannot be -- it'
    Write-Say 'belongs to Ymir/Webzen. The compatible r40250 files you supplied'
    Write-Say '(or the cached copy from an earlier run) are validated, the Linux'
    Write-Say 'port is applied, and the result becomes a Docker build context.'
    Write-Say ''

    if ($script:DryRun) {
        Write-Info "[dry-run] build $($script:FetcherImage), assemble into volume $($script:SrcVolume)"
        Write-Info "[dry-run] copy the finished context into $($script:InstallDir)"
        return
    }

    Initialize-FetcherImage
    Copy-ProjectIntoVolume
    Invoke-SourceFetch
    Export-BuildContext

    if (-not (Test-ContextComplete $script:InstallDir)) { Stop-IncompleteContext $script:InstallDir }
    Write-Good "Server files in place in $($script:InstallDir)"
}

# =============================================================================
#  Step 4 -- settings and passwords
# =============================================================================

function Write-Configuration {
    Write-Step 'Settings and passwords'

    $envPath = Join-Path $script:InstallDir '.env'

    if ($script:DryRun) {
        Write-Info "[dry-run] write $envPath with freshly generated passwords"
        $script:PanelPassword = '(generated at install time)'
        return
    }

    if (-not (Test-Path -LiteralPath $envPath)) {
        $example = Join-Path $script:InstallDir '.env.example'
        if (Test-Path -LiteralPath $example) {
            # Re-written through our own writer so it ends up with Unix line
            # endings like everything else we put in this file.
            $text = [IO.File]::ReadAllText($example) -replace "`r`n", "`n"
            [IO.File]::WriteAllText($envPath, $text, (New-Object System.Text.UTF8Encoding($false)))
        } else {
            [IO.File]::WriteAllText($envPath, '', (New-Object System.Text.UTF8Encoding($false)))
        }
    }

    # --- database passwords. MariaDB stores the root password inside its data
    #     volume at first start; changing .env afterwards does not change it,
    #     so an existing value must be kept.
    $rootPw = Get-EnvValue $envPath 'M2_DB_ROOT_PASSWORD'
    $dbPw   = Get-EnvValue $envPath 'M2_DB_PASSWORD'
    # A database volume that already exists with NO password to go with it is
    # the one combination that cannot work -- and it fails unreadably. MariaDB
    # only runs its setup on an EMPTY data directory; find a populated one and
    # it keeps the passwords it was built with and ignores whatever we pass.
    # Generating fresh ones here hands the game and the panel credentials the
    # database has never heard of, and the only trace is "Access denied for
    # user 'metin2'" in a log nobody thinks to open.
    #
    # The route there is ordinary: install once, delete the install folder,
    # install again. The folder held .env; the volume did not go with it.
    if ((-not $dbPw) -or (-not $rootPw)) {
        $dbVol = "$(Get-StackProject)_db-data"
        if ((Invoke-Native 'docker' @('volume', 'inspect', $dbVol)).Code -eq 0) {
            Stop-Friendly @"
There is already a database here from an earlier install, but the
passwords that go with it are gone -- they lived in the .env file in
$($script:InstallDir), which is no longer there.

Nothing can recover them: they were never written anywhere else, on
purpose. So there are two ways forward.

Keep the characters and accounts, if you still have that old .env
somewhere (a backup, another folder), by putting it back and running
this again:

    copy C:\path\to\old\.env "$($script:InstallDir)\.env"

Or start the database over -- this DELETES every character and account
on this server, and cannot be undone:

    cd "$($script:InstallDir)"
    docker compose down -v

then run this installer again. Everything else -- the built images, the
downloaded server files, the client -- is kept either way.
"@
        }
    }
    if (-not $rootPw) { $rootPw = New-Secret; Write-Good 'database root password: generated' }
    else { Write-Info 'database root password: keeping the existing one' }
    if (-not $dbPw)   { $dbPw = New-Secret;   Write-Good 'database password: generated' }
    else { Write-Info 'database password: keeping the existing one' }

    # --- admin panel passphrase.
    #
    # The panel writes a PBKDF2 hash of this into m2panel.conf on its config
    # volume at first start and never regenerates it, because that would
    # invalidate every session cookie. So on a re-install we must report the
    # OLD password, not a shiny new one that would not work.
    $panelPw = Get-EnvValue $envPath 'M2_PANEL_PASSWORD'
    $confVolumeExists = ((Invoke-Native 'docker' @('volume', 'inspect', "$(Get-StackProject)_panel-conf")).Code -eq 0)

    # A passphrase chosen in the panel wins over the one in .env: the panel is
    # where it was last changed, and .env is only this script's note of it.
    # Writing it back below keeps the two in step, so every later run finds it
    # in .env without having to reach into a container at all.
    $chosen = Get-PanelChosenPassphrase

    if ($chosen) {
        $panelPw = $chosen
        $script:PanelPassword = $chosen
        $script:PanelPasswordKnown = $true
        $script:PanelPasswordNew = $false
        $script:PanelPasswordChosen = $true
        Write-Info 'admin panel password: the one you chose in the panel'
    } elseif ($panelPw) {
        $script:PanelPassword = $panelPw
        $script:PanelPasswordKnown = $true
        $script:PanelPasswordNew = $false
        Write-Info 'admin panel password: keeping the existing one'
    } elseif ($confVolumeExists) {
        $script:PanelPassword = ''
        $script:PanelPasswordKnown = $false
        Write-Warn 'There is an admin panel from an earlier install, but its'
        Write-Warn 'password is not recorded here, so it cannot be shown. The'
        Write-Warn 'summary at the end explains how to set a new one.'
    } else {
        $panelPw = New-Passphrase
        $script:PanelPassword = $panelPw
        $script:PanelPasswordKnown = $true
        Write-Good 'admin panel password: generated (shown at the end)'
    }

    Set-EnvValue $envPath 'M2_DB_ROOT_PASSWORD' $rootPw
    Set-EnvValue $envPath 'M2_DB_PASSWORD'      $dbPw
    if ($panelPw) { Set-EnvValue $envPath 'M2_PANEL_PASSWORD' $panelPw }

    # The password on the game cores' admin socket, which the panel's game
    # master ranks travel over. It used to be empty -- and an empty one does not
    # disable the socket, it makes an EMPTY line the correct password, which
    # grants SHUTDOWN and DC to anything that can reach a core. Only 127.0.0.1
    # can, here and on Linux both, but that is a narrow margin to leave lying
    # around. Generated once and then kept.
    $adminPw = Get-EnvValue $envPath 'M2_ADMINPAGE_PASSWORD'
    if (-not $adminPw) { $adminPw = New-Secret }
    Set-EnvValue $envPath 'M2_ADMINPAGE_PASSWORD' $adminPw

    # Everything points at this computer and nowhere else.
    Set-EnvValue $envPath 'M2_PUBLIC_ADDRESS'   '127.0.0.1'
    Set-EnvValue $envPath 'M2_CLIENT_ADDRESS'   '127.0.0.1'
    Set-EnvValue $envPath 'M2_HOST_BIND_ADDRESS' '127.0.0.1'
    Set-EnvValue $envPath 'M2_PANEL_BIND_ADDRESS' '127.0.0.1'
    # A Windows install is always local-only, so the panel's introduction should
    # say "nobody else can join" rather than "hand this address out". The panel
    # cannot work that out on its own -- a public Linux server behind nginx also
    # binds it to 127.0.0.1 -- so we state it here.
    Set-EnvValue $envPath 'M2_LOCAL_ONLY'       '1'
    # How this machine updates. On Windows that is simply this installer again:
    # it is idempotent, it pulls the published version, rebuilds and restarts,
    # and it keeps your database, your passwords and your settings. The panel
    # shows this line when a newer version appears.
    #
    # Deliberately NOT a background watcher. The Linux stack can run one as a
    # container, but on Windows it would have to be a scheduled task or a
    # PowerShell process living on the host -- something outside Docker,
    # quietly running, that nobody asked for. One line to paste when the panel
    # says there is something to get is a better trade.
    Set-EnvValue $envPath 'M2_UPDATE_COMMAND' `
        "irm $($script:SelfUrl) | iex"
    Set-EnvValue $envPath 'M2_AUTH_PORT'        "$($script:AuthPort)"
    Set-EnvValue $envPath 'M2_GAME_PORT_RANGE'  $script:GamePorts
    # Bind address and numeric published port are deliberately separate. Docker
    # Compose cannot parse two concatenated addresses such as
    # "127.0.0.1:127.0.0.1:7788:7788".
    Set-EnvValue $envPath 'M2_PANEL_PUBLIC_PORT' "$($script:PanelPort)"

    # --- the client that runs in a browser ----------------------------------
    #
    # Whatever you set stays set, and an install that has never heard of this
    # gets a 0. The installer does not switch it on by itself: the bridge is
    # only useful once a browser client has been put on the panel's volume, and
    # before that it would be a container running for nothing.
    #
    # A browser cannot open a TCP socket and the game speaks TCP, so a page
    # that wants to play needs the `wsbridge' service beside the server. It is
    # in a compose profile and is not started by `docker compose up -d'.
    #
    # Everything here is on 127.0.0.1, which is what a Windows install is. The
    # panel is plain HTTP on loopback, so the page uses ws:// -- and a browser
    # treats localhost as secure, so nothing is blocked as mixed content the
    # way it would be on an HTTPS site without a certificate for the bridge.
    #
    # The page is told the address in its own URL (?serverHost=&serverPort=),
    # which is how the browser client takes it; the panel builds that link.
    # Select-Clients has already decided, and it read the previous state out of
    # what is installed rather than out of this file -- so somebody who removed
    # the browser client by hand is not told it is still there.
    $browserPlay = if ($script:WantWeb) { '1' } else { '0' }
    $script:BrowserPlay = $browserPlay
    Set-EnvValue $envPath 'M2_BROWSER_PLAY' $browserPlay
    $sp = $script:MoveSpeedBonus
    if (-not $sp) { $sp = Get-EnvValue $envPath 'M2_MOVE_SPEED_BONUS' }
    if ($sp -notmatch '^[0-9]+$') { $sp = '0' }
    Set-EnvValue $envPath 'M2_MOVE_SPEED_BONUS' $sp

    # Written down so the next update replays the same server rather than a
    # slightly different one. This is the whole point of the switch: what a
    # server is running should be readable from the server, not remembered.
    Set-EnvValue $envPath 'M2_CUSTOM_EXPERIENCE' $(if ($script:CustomExperience) { '1' } else { '0' })
    $hr = $script:HighRisk
    if (-not $hr) { $hr = Get-EnvValue $envPath 'M2_HIGH_RISK' }
    if ($hr -notmatch '^[01]$') { $hr = '1' }
    Set-EnvValue $envPath 'M2_HIGH_RISK' $hr

    # Where the desktop client comes from: its own archive since the package was
    # split. Left empty, the builder looks for a client inside the server files,
    # which is where it used to be and no longer is. Not overwritten, so an
    # operator who pointed this at a client of their own keeps theirs.
    $oldClientUrl = Get-EnvValue $envPath 'M2_CLIENT_ARCHIVE_URL'
    if ($oldClientUrl -in @(
        'https://mega.nz/file/X7YT3BRB#uSVimr2N0y87gTrbdChLUQJWXOnZ49cIkqrc-wJU4MU',
        'https://dl.htpsoftware.com/Reference_Client.zip')) {
        Set-EnvValue $envPath 'M2_CLIENT_ARCHIVE_URL' ''
        $oldClientUrl = ''
    }
    if (-not $oldClientUrl) {
        $cu = Get-PointerLink 'src_client_url'
        if ($cu) {
            Set-EnvValue $envPath 'M2_CLIENT_ARCHIVE_URL' $cu
            $cs = Get-PointerValue 'src_client_sha256'
            if ($cs -match '^[0-9a-f]{64}$') {
                Set-EnvValue $envPath 'M2_CLIENT_ARCHIVE_SHA256' $cs
            }
        }
    }
    # The same archive somewhere else. Written on every run rather than only
    # when empty, so an installation follows whatever artifacts.json names now
    # instead of keeping a link that has since been retired.
    Set-EnvValue $envPath 'M2_CLIENT_ARCHIVE_URL_FALLBACK'  (Get-PointerLink 'src_client_url_fallback')
    Set-EnvValue $envPath 'M2_CLIENT_ARCHIVE_URL_FALLBACK2' (Get-PointerLink 'src_client_url_fallback2')

    $bridgePort = Get-EnvValue $envPath 'M2_BRIDGE_PORT'
    if (-not $bridgePort) { $bridgePort = '7789' }
    $script:BridgePort = $bridgePort
    Set-EnvValue $envPath 'M2_BRIDGE_PORT'         $bridgePort
    Set-EnvValue $envPath 'M2_BRIDGE_BIND_ADDRESS' '127.0.0.1'
    Set-EnvValue $envPath 'M2_BRIDGE_TRUST_PROXY'  '0'

    Write-Good "Settings written to $envPath"
}

# =============================================================================
#  The client that runs in a browser.
#
#  Only the bridge lives in the stack. The client itself -- built to
#  WebAssembly -- is put on the panel's volume by hand, because it is built
#  from game data that is not ours to hand out.
#
#  Switched off, this does nothing at all, except stop a bridge an earlier run
#  started. An install that has never heard of this never builds the image.
# =============================================================================
function Start-BrowserBridge {
    if ($script:DryRun) {
        Write-Info "[dry-run] browser bridge: M2_BROWSER_PLAY=$($script:BrowserPlay)"
        return
    }

    if ($script:BrowserPlay -ne '1') {
        if ((Invoke-Native 'docker' @('inspect', 'metin2-wsbridge')).Code -eq 0) {
            Write-Step 'Playing in the browser'
            Write-Say 'M2_BROWSER_PLAY is 0 now, so the WebSocket bridge is being stopped.'
            Invoke-ComposeQuiet @('--profile', 'browser', 'rm', '-sf', 'wsbridge') | Out-Null
            Write-Good 'The bridge is stopped. The game itself is untouched.'
        }
        return
    }

    Write-Step 'Playing in the browser'

    # wsbridge sits behind the "browser" profile, so a plain 'config --services'
    # does not list it -- the profile has to be asked for by name.
    $svc = Invoke-ComposeQuiet @('--profile', 'browser', 'config', '--services')
    $haveBridge = (($svc.Output -split "`n" | ForEach-Object { $_.Trim() }) -contains 'wsbridge')
    if (-not $haveBridge) {
        Write-Warn 'M2_BROWSER_PLAY=1, but these server files have no wsbridge service.'
        Write-Warn 'They are older than this feature. Nothing else is affected.'
        return
    }

    Write-Say 'Starting the WebSocket bridge, which is what lets a browser reach a'
    Write-Say 'game server that speaks TCP.'
    # Invoke-Compose hands back the exit code, so 0 is the good answer.
    if ((Invoke-Compose @('--profile', 'browser', 'up', '-d', '--build', 'wsbridge')) -eq 0) {
        Write-Good "The bridge is running on 127.0.0.1:$($script:BridgePort)."
        Write-Say ''
        Write-Say "The panel shows a 'Play in the browser' button once a browser"
        Write-Say 'client is on its volume, and not before -- there would be nothing'
        Write-Say 'behind it. This installer puts one there for you; if the button'
        Write-Say 'is missing, run it again and say yes to the browser client.'
    }
    else {
        Write-Warn 'The bridge did not start. Playing in the browser will not work;'
        Write-Warn 'everything else is unaffected. To see why:'
        Write-Warn "    docker compose --profile browser logs wsbridge"
    }
}

function Write-LoopbackOverride {
    # The game's published port range appears twice in the base compose file
    # ("${RANGE}:${RANGE}"), so an address cannot be put in front of it through
    # the environment -- the ports list has to be replaced outright.
    $path = Join-Path $script:InstallDir 'docker-compose.override.yml'

    if ($script:DryRun) { Write-Info "[dry-run] write $path"; return }

    if (-not (Test-ComposeOverrideSupported)) {
        Stop-Friendly @"
This Docker Compose is older than version 2.24, and the installer needs that
version to bind the game ports to this computer only.

Without it the server would be published to your whole network, which is not
what this installer promises.

What to do: open Docker Desktop, let it update itself (Settings -> Software
updates), then run this installer again.
"@
    }

    # Inside the container the channel cores always listen on 13000-13002 --
    # one channel, three ports, decided by the game and not by us; the auth
    # core is likewise always 11000. Only the host side of each mapping is the
    # operator's to choose. Writing $script:GamePorts on *both* sides of the
    # colon published a range that led nowhere: docker accepted the connection
    # and then had nothing behind it to hand the player to, which looks exactly
    # like a server that is up but broken.
    $containerGamePorts = '13000-13002'
    $span = $script:GamePorts.Split('-')
    if ($span.Count -ne 2 -or ([int]$span[1] - [int]$span[0]) -ne 2) {
        Stop-Friendly @"
-GamePorts has to be a range of exactly three ports, like 13000-13002.

This install runs one channel, and one channel occupies three consecutive
ports inside the server. '$($script:GamePorts)' is not three ports, so there would be
nothing behind part of the range.
"@
    }

    $yaml = @(
        '# Written by install.ps1 -- do not edit; re-run the installer instead.'
        '#'
        '# Everything binds 127.0.0.1: this server is reachable from this'
        '# computer and nowhere else. No other PC on your network can see it,'
        '# and neither can anyone on the internet.'
        'services:'
        '  game:'
        '    ports: !override'
        "      - `"127.0.0.1:$($script:AuthPort):11000/tcp`""
        "      - `"127.0.0.1:$($script:GamePorts):$containerGamePorts/tcp`""
        ''
    ) -join "`n"
    [IO.File]::WriteAllText($path, $yaml, (New-Object System.Text.UTF8Encoding($false)))
    Write-Good 'Loopback-only configuration written.'
}

# =============================================================================
#  Step 5 -- build and start
# =============================================================================

function Start-Stack {
    Write-Step 'Building and starting the server'

    if ($script:FreshInstall) {
        Write-Say 'The first run compiles the game server from its original source'
        Write-Say 'code -- about 195 files. Expect 15 to 30 minutes. It only'
        Write-Say 'happens once; starting it again later takes seconds.'
        Write-Say ''
        Write-Say 'You will see a lot of compiler output. That is normal.'
        Write-Say ''
    }

    if ($script:DryRun) {
        if (-not $script:FreshInstall) {
            Write-Info '[dry-run] stop game and panel before the Playerbot migration'
        }
        Write-Info "[dry-run] docker compose up -d --build in $($script:InstallDir)"
        return
    }

    # The container names are fixed in the compose file, so another copy of
    # this server would collide with a confusing error. Check first.
    foreach ($n in (Get-StackContainerNames)) {
        if ((Invoke-Native 'docker' @('inspect', $n)).Code -eq 0) {
            # The whole label map as JSON, rather than {{index .Config.Labels
            # "com.docker..."}}. Windows PowerShell rebuilds every argument it
            # hands to a native program and mangles the double quotes inside
            # that template, so docker received a template it could not parse
            # and answered on standard error -- which then read as the name of
            # a directory. This form needs no quotes at all.
            $proj = ''
            $labels = Invoke-Native 'docker' @('inspect', '-f', '{{json .Config.Labels}}', $n)
            if ($labels.Code -eq 0 -and $labels.Output) {
                try {
                    $proj = [string]($labels.Output | ConvertFrom-Json).'com.docker.compose.project.working_dir'
                } catch { $proj = '' }
            }
            if ($proj -and ($proj.Trim().TrimEnd('\','/') -ne $script:InstallDir.TrimEnd('\','/'))) {
                Stop-Friendly @"
A container called '$n' already exists and belongs to another copy of this
server, in:

    $proj

Only one Metin2 server can run per PC, because the container names are fixed.
Either use that one, or remove it first:

    cd "$proj"
    docker compose down
"@
            }
        }
    }

    # Compose dependency ordering only applies while containers are started.
    # Pause existing DB consumers so an update cannot migrate underneath them.
    if (-not $script:FreshInstall) {
        Write-Info 'Stopping game and panel while the Playerbot seed is checked.'
        if ((Invoke-Compose @('stop', 'game', 'panel')) -ne 0) {
            Stop-Friendly @"
The running game or panel could not be stopped. The Playerbot seed was not
started; fix the Docker error above and retry.
"@
        }
    }

    $code = Invoke-Compose @('up', '-d', '--build')
    if ($code -ne 0) {
        Invoke-Compose @('logs', '--no-color', '--tail', '80', 'playerbot-migrate') | Out-Null
        Stop-Friendly @"
The server did not build or did not start. The output above says why. The
usual causes, most common first:

  - Docker Desktop ran out of memory or disk during the build. Open its
    Settings -> Resources and give it at least 4 GB of memory.
  - No internet access from the build (it downloads Ubuntu packages).
  - Docker Desktop stopped part way through. Check its whale icon says
    "Engine running".

Nothing was left in a broken state. When you have fixed it, run the installer
again -- the parts that already built are cached, so it picks up where it
stopped rather than starting over.
"@
    }
    Write-Good 'Containers are up.'
}

function Get-ServiceHealth {
    param([string]$Service)
    $r = Invoke-ComposeQuiet @('ps', '-q', $Service)
    $cid = ($r.Output -split "`n" | Where-Object { $_.Trim() } | Select-Object -First 1)
    if (-not $cid) { return 'missing' }
    $i = Invoke-Native 'docker' @('inspect', '-f', '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}', $cid.Trim())
    # A failed inspect prints its complaint on standard error, which would read
    # as a health status of its own if it were passed straight through.
    if ($i.Code -ne 0 -or -not $i.Output) { return 'unknown' }
    return $i.Output.Trim()
}

function Wait-Healthy {
    Write-Step 'Waiting for the server to come up'

    if ($script:DryRun) { Write-Info '[dry-run] wait for the containers to report healthy'; return }

    Write-Say 'The database imports the shipped world on its very first start,'
    Write-Say 'and the game cores boot one after another. Give it two or three'
    Write-Say 'minutes.'
    Write-Say ''

    $deadline = (Get-Date).AddMinutes(7)
    $last = ''
    while ((Get-Date) -lt $deadline) {
        $db = Get-ServiceHealth 'mariadb'
        $gm = Get-ServiceHealth 'game'
        $pn = Get-ServiceHealth 'panel'
        $now = "db=$db game=$gm panel=$pn"
        if ($now -ne $last) { Write-Info $now; $last = $now }
        if (($gm -in @('healthy','running')) -and ($pn -in @('healthy','running'))) {
            Write-Good 'The server is up.'
            return
        }
        if ($gm -in @('exited','dead')) { break }
        Start-Sleep -Seconds 5
    }

    Write-Warn 'The server has not reported itself healthy yet. It may still be'
    Write-Warn 'starting. Check with:'
    Write-Warn "    cd `"$($script:InstallDir)`""
    Write-Warn '    docker compose ps'
    Write-Warn '    docker compose logs game --tail 50'
}

# =============================================================================
#  Step 6 -- the game client, and getting it onto this PC
#
#  Built by a separate compose service that downloads a large archive, patches
#  the address the client connects to (127.0.0.1 here), repacks it and leaves
#  client.zip where the panel serves it. It takes a long time, so it runs in
#  the background and we report honestly on where it got to.
#
#  On a Windows install that download page is a round trip to nowhere: the
#  client is being built on the very PC that is going to play it. So the same
#  background run carries on afterwards -- it copies the finished zip out of
#  the panel's volume, unpacks it here, and puts a shortcut on the Desktop.
#  Nobody has to open a browser to get the game they just built.
#
#  All of that happens AFTER this installer has printed its summary and gone.
#  The build is twenty to sixty minutes and nobody will sit and watch it, so
#  the copy-out, the unpack and the shortcut cannot live in this script's own
#  flow. They live in a small script written next to the server --
#  client-setup.ps1 -- which this one starts detached and then forgets about.
#  It is an ordinary file you can read, and running it again by hand is the
#  supported way to retry: it does nothing at all when the game is already
#  here.
#
#  WHERE THE GAME LANDS: <install dir>\client. Three reasons for that rather
#  than the Desktop or a temp folder:
#
#    * the install folder's length has already been measured against Windows'
#      260-character limit (Test-InstallPathLength); the Desktop's has not,
#      and the client tree is three levels deep with long pack names in it
#    * on a great many PCs the Desktop is redirected into OneDrive, and
#      unpacking 1.2 GB there would quietly begin uploading it to the cloud
#    * everything this installer made then sits in one folder, so "delete the
#      folder" stays the whole uninstall
#
#  It costs about 1.2 GB for the copied zip -- deleted the moment it is
#  unpacked -- and roughly twice that for the unpacked tree, both on the
#  install folder's drive. The obvious way to avoid the copy, bind-mounting a
#  Windows folder into the builder so it writes straight there, is much worse:
#  a gigabyte of small files through Docker Desktop's filesystem bridge takes
#  far longer than copying one finished archive out afterwards.
# =============================================================================

function Test-ClientZipPresent {
    $r = Invoke-ComposeQuiet @('exec', '-T', 'panel', 'test', '-f', '/usr/local/m2panel/client.zip')
    return ($r.Code -eq 0)
}

# The unpacked game, if it is here. client-setup.ps1 writes this one-line stamp
# INSIDE the folder before renaming it into place, so its presence means the
# unpack finished -- a folder that is still being written is called something
# else and has no stamp in it. That is the whole "already done" test, and it is
# why a half-finished unpack can never be mistaken for a playable game.
function Get-LocalClientExe {
    if (-not $script:ClientDir) { return '' }
    $stamp = Join-Path $script:ClientDir '.metin2-client'
    if (-not (Test-Path -LiteralPath $stamp)) { return '' }
    try {
        $exe = [IO.File]::ReadAllLines($stamp) |
               Where-Object { $_.Trim() } | Select-Object -First 1
    } catch { return '' }
    if ($exe -and (Test-Path -LiteralPath $exe.Trim())) { return $exe.Trim() }
    return ''
}

# --- writing values into a generated script ---------------------------------
#
# Everything client-setup.ps1 needs is baked into it as a PowerShell string
# literal rather than passed on a command line, and that is deliberate. The
# archive this stack downloads is published as "[40250] Reference
# Serverfile-....zip"; Start-Process -ArgumentList joins its elements with
# spaces and quotes nothing, so a path handed over that way arrives torn in
# half -- which is exactly how "no such service: Reference" happened once. A
# single-quoted literal inside a file has no such problem, whatever is in it.
function ConvertTo-PsLiteral {
    param([string]$Value)
    return "'" + (([string]$Value) -replace "'", "''") + "'"
}
function ConvertTo-PsArrayLiteral {
    param([string[]]$Values)
    if (-not $Values -or $Values.Count -eq 0) { return '@()' }
    return '@(' + (($Values | ForEach-Object { ConvertTo-PsLiteral $_ }) -join ', ') + ')'
}

# Write client-setup.ps1 into the install folder and hand back its path.
function Write-ClientSetupScript {
    param([string[]]$BuildArgs, [string]$ArchiveInContainer)

    $path = Join-Path $script:InstallDir 'client-setup.ps1'

    $head = @(
        '# ==========================================================================='
        '#  client-setup.ps1 -- written by install.ps1. Safe to run again at any time.'
        '#'
        '#  It finishes the job the installer could not wait for: it builds the game'
        '#  client if that has not happened yet, copies it out of the panel, unpacks'
        '#  it onto this PC and puts a shortcut on the Desktop. Everything it does is'
        '#  appended to the log named below, which is the one file to look at when'
        '#  the game did not appear.'
        '#'
        '#  If the game is already unpacked it does nothing and says so. So:'
        '#'
        "#      powershell -ExecutionPolicy Bypass -File `"$path`""
        '#'
        '#  ...is always a safe thing to run.'
        '# ==========================================================================='
        ''
        "`$InstallDir   = $(ConvertTo-PsLiteral $script:InstallDir)"
        "`$LogPath      = $(ConvertTo-PsLiteral $script:ClientLog)"
        "`$ClientDir    = $(ConvertTo-PsLiteral $script:ClientDir)"
        "`$ShortcutName = $(ConvertTo-PsLiteral $script:ShortcutName)"
        "`$PanelUrl     = $(ConvertTo-PsLiteral "http://127.0.0.1:$($script:PanelPort)")"
        "`$PanelZip     = '/usr/local/m2panel/client.zip'"
        "`$BuildArgs    = $(ConvertTo-PsArrayLiteral $BuildArgs)"
        "`$CachedArchive = $(ConvertTo-PsLiteral $ArchiveInContainer)"
        "`$UnpackHere   = `$$($script:LocalOnly.ToString().ToLower())"
        "`$SelfPath     = $(ConvertTo-PsLiteral $path)"
        "`$HostOut      = $(ConvertTo-PsLiteral $script:ClientHostOut)"
        "`$HostErr      = $(ConvertTo-PsLiteral $script:ClientHostErr)"
        ''
        ''
    ) -join "`n"

    # Single-quoted here-string: not one character of this is expanded here. It
    # is a program in its own right and its $variables are its own.
    $body = @'
$ErrorActionPreference = 'Stop'

$TmpZip   = Join-Path $InstallDir '.client.zip.part'
$TmpDir   = Join-Path $InstallDir '.client.new'
$LockPath = Join-Path $InstallDir '.client-setup.lock'
$Stamp    = '.metin2-client'

# --- the log ----------------------------------------------------------------
# One writer, opened in append mode, flushed on every line. The installer
# points the human at this file and nothing else, so everything below --
# docker's output included -- goes through here. Until it opens, and if it
# cannot be opened at all, messages fall through to standard output, which is
# the temp file folded back in at the bottom of this script.
$log = $null
try {
    $log = New-Object System.IO.StreamWriter($LogPath, $true, (New-Object System.Text.UTF8Encoding($false)))
    $log.AutoFlush = $true
} catch { $log = $null }

function Say {
    param([string]$Text)
    $line = '{0} [setup] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Text
    if ($log) { $log.WriteLine($line) } else { Write-Output $line }
}

function Show-Size {
    param([int64]$Bytes)
    if ($Bytes -ge 1GB) { return ('{0:N1} GB' -f ($Bytes / 1GB)) }
    if ($Bytes -ge 1MB) { return ('{0:N0} MB' -f ($Bytes / 1MB)) }
    return ('{0:N0} KB' -f ($Bytes / 1KB))
}

# Same reasoning as Invoke-Native in install.ps1: a native program's standard
# error becomes an error record the moment it is redirected, and under
# $ErrorActionPreference = 'Stop' that record is terminating. docker writes its
# whole build log there and means nothing by it.
function Invoke-Docker {
    param([string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & docker @Arguments 2>&1 | ForEach-Object {
            $t = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.ToString() } else { [string]$_ }
            if ($log) { $log.WriteLine($t) } else { Write-Output $t }
        }
        return $LASTEXITCODE
    } finally { $ErrorActionPreference = $previous }
}

function Invoke-DockerQuiet {
    param([string[]]$Arguments)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & docker @Arguments 2>&1
        return [pscustomobject]@{ Code = $LASTEXITCODE; Output = (($out | Out-String).Trim()) }
    } finally { $ErrorActionPreference = $previous }
}

# --- one at a time ----------------------------------------------------------
# Two of these running together would copy and unpack a gigabyte twice over
# each other. The check is a file holding a process id: not airtight against a
# perfectly timed pair, but the case it is for -- somebody running the
# installer again while the first build is still going -- is not perfectly
# timed.
$lockTaken = $false
function Get-SetupLock {
    if (Test-Path -LiteralPath $LockPath) {
        $other = $null
        try {
            $text = (Get-Content -LiteralPath $LockPath -TotalCount 1 -ErrorAction SilentlyContinue)
            if ("$text".Trim() -match '^\d+$') {
                $other = Get-Process -Id ([int]"$text".Trim()) -ErrorAction SilentlyContinue
            }
        } catch { $other = $null }
        if ($other -and $other.ProcessName -like 'powershell*') { return $false }
        Remove-Item -LiteralPath $LockPath -Force -ErrorAction SilentlyContinue
    }
    Set-Content -LiteralPath $LockPath -Value "$PID" -Encoding ASCII
    return $true
}

# --- finding the launcher ---------------------------------------------------
# The zip is built by pack_prepare_client(), which packs the client's own root
# folder, so Metin2Distribute.exe sits at the very top. The search below is for
# the day somebody hands the stack a differently-packed client: shallowest
# match wins, and the tools that live next to the game -- the pack editor, the
# screen-settings program, the VC++ redistributable -- are excluded by name so
# the shortcut can never point at one of them.
function Find-ClientExe {
    param([string]$Root)
    if (-not (Test-Path -LiteralPath $Root)) { return '' }
    $direct = Join-Path $Root 'Metin2Distribute.exe'
    if (Test-Path -LiteralPath $direct) { return $direct }
    $hit = Get-ChildItem -LiteralPath $Root -Recurse -Depth 2 -Filter '*.exe' -File -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -like 'Metin2*' -and
                          $_.Name -notmatch '(?i)eternexus|dump_proto|vcredist|config' } |
           Sort-Object { $_.FullName.Split('\').Count }, Name |
           Select-Object -First 1
    if ($hit) { return $hit.FullName }
    return ''
}

# --- the Desktop shortcut ---------------------------------------------------
# A .lnk through WScript.Shell, which is the only way to make a real one. The
# working directory matters as much as the target: the client loads its packs
# by relative path and starts into an error box if it is launched from
# anywhere else.
#
# A shortcut of this name that is ALREADY on the Desktop is never overwritten.
# Somebody may have moved it, renamed the game folder or pointed it somewhere
# on purpose, and silently replacing that would be rude. It is only reported.
function Set-DesktopShortcut {
    param([string]$Exe)

    $desktop = [Environment]::GetFolderPath('Desktop')
    if (-not $desktop -or -not (Test-Path -LiteralPath $desktop)) {
        Say "could not find your Desktop folder, so no shortcut was made."
        Say "the game is at $Exe"
        return $false
    }
    $lnk = Join-Path $desktop ($ShortcutName + '.lnk')

    $shell = $null
    try {
        $shell = New-Object -ComObject WScript.Shell

        if (Test-Path -LiteralPath $lnk) {
            $target = ''
            try { $target = [string]$shell.CreateShortcut($lnk).TargetPath } catch { $target = '' }
            if ([string]::Equals($target, $Exe, [StringComparison]::OrdinalIgnoreCase)) {
                Say "the Desktop shortcut '$ShortcutName' is already there and already points at the game -- left alone"
            } else {
                Say "a shortcut called '$ShortcutName' is already on your Desktop and points at"
                Say "  $target"
                Say "it was NOT changed. The game this installer unpacked is at"
                Say "  $Exe"
            }
            return $true
        }

        $s = $shell.CreateShortcut($lnk)
        $s.TargetPath       = $Exe
        $s.WorkingDirectory = (Split-Path -Parent $Exe)
        $s.IconLocation     = "$Exe,0"
        $s.Description      = 'Metin2 -- the private server on this PC'
        $s.Save()
        if (-not (Test-Path -LiteralPath $lnk)) {
            Say "the Desktop shortcut could not be written to $lnk"
            return $false
        }
        Say "Desktop shortcut created: $lnk"
        Say "  it starts   $Exe"
        Say "  from        $(Split-Path -Parent $Exe)"
        return $true
    } catch {
        Say "the Desktop shortcut could not be created: $($_.Exception.Message)"
        Say "the game itself is fine -- start it by double-clicking $Exe"
        return $false
    } finally {
        if ($shell) {
            try { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($shell) } catch { }
        }
    }
}

# ---------------------------------------------------------------------------
#  The whole job. Returns $true when the game is playable on this PC.
# ---------------------------------------------------------------------------
function Invoke-ClientSetup {

    # -- already done? --------------------------------------------------------
    $exe = ''
    $stampFile = Join-Path $ClientDir $Stamp
    if (Test-Path -LiteralPath $stampFile) {
        try {
            $exe = ([IO.File]::ReadAllLines($stampFile) |
                    Where-Object { $_.Trim() } | Select-Object -First 1)
        } catch { $exe = '' }
        if ($exe) { $exe = $exe.Trim() }
        if ($exe -and (Test-Path -LiteralPath $exe)) {
            Say "the game is already unpacked in $ClientDir -- nothing to do"
            [void](Set-DesktopShortcut $exe)
            return $true
        }
        Say "there is a stamp in $ClientDir but the game it names is gone"
    }

    # -- 1. the build ---------------------------------------------------------
    $havePanelZip = ((Invoke-DockerQuiet @('compose','exec','-T','panel','test','-f',$PanelZip)).Code -eq 0)
    if (-not $havePanelZip) {
        if ($BuildArgs.Count -eq 0) {
            throw "there is no client in the panel and no client builder in this release."
        }
        Say 'building the game client. This is the long part -- 20 to 60 minutes.'
        $code = Invoke-Docker $BuildArgs
        if ($code -ne 0) {
            throw "the client build failed: docker exited with $code. Its output is above this line."
        }
        $havePanelZip = ((Invoke-DockerQuiet @('compose','exec','-T','panel','test','-f',$PanelZip)).Code -eq 0)
        if (-not $havePanelZip) {
            throw "the client build finished but $PanelZip is not there."
        }
        Say 'the client is built and the panel is serving it.'
    } else {
        Say 'the panel already has a built client -- skipping the build'
    }

    if (-not $UnpackHere) {
        Say 'this install is not local-only, so the client stays on the download page.'
        return $true
    }

    # -- 2. is there room? ----------------------------------------------------
    # The zip is copied out whole and then unpacked beside itself, so both
    # exist at once. Roughly three times the zip, plus a margin. Running out
    # half way is the failure this is here to avoid: it wastes twenty minutes
    # and leaves a mess.
    $size = 0
    $probe = Invoke-DockerQuiet @('compose','exec','-T','panel','wc','-c',$PanelZip)
    if ($probe.Code -eq 0 -and $probe.Output -match '(\d+)') { $size = [int64]$Matches[1] }
    if ($size -gt 0) {
        Say "the client is $(Show-Size $size)"
        $need = ($size * 3) + 512MB
        $free = 0
        try { $free = (New-Object IO.DriveInfo([IO.Path]::GetPathRoot($InstallDir))).AvailableFreeSpace } catch { $free = 0 }
        if ($free -gt 0 -and $free -lt $need) {
            throw ("not enough disk space to unpack the game. About {0:N0} MB is needed on {1} and {2:N0} MB is free. Free some space and run this script again." -f `
                   [math]::Round($need / 1MB), [IO.Path]::GetPathRoot($InstallDir), [math]::Round($free / 1MB))
        }
    } else {
        Say 'could not measure the client, so the disk-space check was skipped'
    }

    # -- 3. copy it out of the panel's volume ---------------------------------
    Remove-Item -LiteralPath $TmpZip -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $TmpDir -Recurse -Force -ErrorAction SilentlyContinue
    Say "copying it out of the panel. A gigabyte -- a couple of minutes."
    $code = Invoke-Docker @('compose','cp',("panel:" + $PanelZip),$TmpZip)
    if ($code -ne 0 -or -not (Test-Path -LiteralPath $TmpZip)) {
        throw "the client could not be copied out of the panel (docker exited with $code)."
    }
    $got = (Get-Item -LiteralPath $TmpZip).Length
    if ($size -gt 0 -and $got -ne $size) {
        throw ("the copy came out {0:N0} bytes but the original is {1:N0} -- refusing to unpack it." -f $got, $size)
    }
    Say "copied $(Show-Size $got)"

    # -- 4. unpack under a temporary name -------------------------------------
    # The same rule the client builder uses when it publishes its zip: nothing
    # is called by its final name until it is finished. A folder called
    # "client" therefore always holds a complete game and never a partial one.
    Say "unpacking into $TmpDir"
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        [System.IO.Compression.ZipFile]::ExtractToDirectory($TmpZip, $TmpDir)
    } catch {
        $m = $_.Exception.Message
        if ($_.Exception -is [System.IO.PathTooLongException] -or $m -match '(?i)too long') {
            throw "the client could not be unpacked because the paths inside it are too long for Windows underneath $InstallDir. Install the server into a shorter folder -- C:\Metin2Server, say -- or switch on Windows' long-path support."
        }
        throw "the client could not be unpacked: $m"
    }

    # -- 5. is it actually a game? --------------------------------------------
    $exe = Find-ClientExe $TmpDir
    if (-not $exe) {
        throw "there is no Metin2 launcher in the client that came out of the panel, so it was thrown away rather than left here pretending to be a game. Whatever is in $PanelZip is not a playable client."
    }
    Say "found the launcher: $(Split-Path -Leaf $exe)"

    # The stamp goes in BEFORE the rename, so the folder is complete-and-marked
    # the instant it acquires its final name. The launcher's path is rebuilt
    # from the part below the temporary folder rather than by replacing text in
    # it -- a string replace is case-sensitive and would silently produce a
    # path that does not exist.
    $stampTarget = Join-Path $ClientDir ($exe.Substring($TmpDir.Length).TrimStart('\'))
    [IO.File]::WriteAllText((Join-Path $TmpDir $Stamp),
                            ($stampTarget + "`r`n"),
                            (New-Object System.Text.UTF8Encoding($false)))

    # -- 6. into place --------------------------------------------------------
    if (Test-Path -LiteralPath $ClientDir) {
        throw "there is already a folder at $ClientDir but no game in it. Move it out of the way and run this script again."
    }
    Move-Item -LiteralPath $TmpDir -Destination $ClientDir
    Remove-Item -LiteralPath $TmpZip -Force -ErrorAction SilentlyContinue
    Say "the game is in $ClientDir"

    if (-not (Test-Path -LiteralPath $stampTarget)) {
        throw "the game moved into $ClientDir but $stampTarget is not there."
    }

    # -- 7. the shortcut ------------------------------------------------------
    [void](Set-DesktopShortcut $stampTarget)
    return $true
}

# ---------------------------------------------------------------------------
$rc = 1
try {
    Set-Location -LiteralPath $InstallDir
    if ($CachedArchive) { $env:M2_CLIENT_ARCHIVE = $CachedArchive }

    Say ''
    Say '=================================================================='
    Say 'putting the game on this PC'

    $lockTaken = Get-SetupLock
    if (-not $lockTaken) {
        Say 'another copy of this script is already running -- leaving it to finish.'
        $rc = 0
    }
    elseif (Invoke-ClientSetup) {
        Say 'done. The game is ready to play.'
        $rc = 0
    }
}
catch {
    Say ''
    Say '!!  the game could not be put on this PC.'
    Say "    $($_.Exception.Message)"
    Say ''
    Say '    The server itself is untouched and still running -- this is only'
    Say '    about getting the game onto the Desktop.'
    Say "    You can still download the client from $PanelUrl/download"
    Say '    once the build has finished.'
    Say ''
    Say '    To try again once the reason above is dealt with:'
    Say "        powershell -ExecutionPolicy Bypass -File `"$SelfPath`""
    $rc = 1
}
finally {
    # Nothing half-finished is left lying about with a name that suggests it is
    # ready. After a successful run both of these are already gone.
    Remove-Item -LiteralPath $TmpZip -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $TmpDir -Recurse -Force -ErrorAction SilentlyContinue
    if ($lockTaken) { Remove-Item -LiteralPath $LockPath -Force -ErrorAction SilentlyContinue }

    # This script's own standard output and error are redirected into two files
    # in the temp folder by whoever started it, because there has to be
    # somewhere for PowerShell itself to complain when the script cannot even
    # be read. That is the one thing this log cannot capture, so anything in
    # them is folded in here and there is one file to look at, this one.
    #
    # Read with FileShare.ReadWrite because one of them is this process's own
    # standard output and Windows has it open. For the same reason the delete
    # below only succeeds when these belong to an EARLIER launch -- which is
    # the case when somebody runs this script by hand, and is why it is worth
    # asking. The pair from a launch that is still running is cleared away by
    # the next install instead.
    foreach ($sideFile in @($HostOut, $HostErr)) {
        if (-not $sideFile) { continue }
        try {
            if ((Test-Path -LiteralPath $sideFile) -and ((Get-Item -LiteralPath $sideFile).Length -gt 0)) {
                $fs = New-Object System.IO.FileStream($sideFile, [IO.FileMode]::Open,
                                                     [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
                try {
                    $sr = New-Object System.IO.StreamReader($fs)
                    $body = $sr.ReadToEnd()
                    $sr.Dispose()
                } finally { $fs.Dispose() }
                if ($body -and $body.Trim() -and $log) {
                    $log.WriteLine("--- $(Split-Path -Leaf $sideFile) ---")
                    $log.WriteLine($body.TrimEnd())
                }
            }
        } catch { }
        # Fails while whoever started us is still holding the handle -- that is
        # the case where they fold these in themselves, so it is not a problem.
        Remove-Item -LiteralPath $sideFile -Force -ErrorAction SilentlyContinue
    }

    if ($log) { try { $log.Flush(); $log.Dispose() } catch { } }
}
exit $rc
'@

    [IO.File]::WriteAllText($path, ($head + $body), (New-Object System.Text.UTF8Encoding($false)))
    return $path
}

# =============================================================================
#  Which clients this install offers
#
#  The Linux installer asks the same question in the same order; the two are
#  kept in step deliberately. See choose_clients() in install.sh.
#
#      desktop   the classic download, built from Reference_Client.zip
#      browser   a link: Reference_WebClientEngine.zip (17.6 MB) plus
#                Reference_WebClientData.zip (1.75 GB)
#
#  On a re-run nothing is asked when both are already there. When one is
#  missing it is offered -- that is the only moment somebody finds out the
#  other way exists.
# =============================================================================
function Test-InstalledClients {
    # Asked of the panel container: a Docker volume is a Docker object, and on
    # Windows the host cannot see inside one at all -- there is no path to guess
    # at. Both answers are simply "no" before the stack has ever run, which on a
    # first install is the truth.
    $script:HaveWeb     = $false
    $script:HaveDesktop = $false
    if ($script:DryRun) { return }
    # A first install has nothing installed by definition, and asking anyway
    # would BUILD the panel image just to be told so -- minutes of work for an
    # answer we already have.
    if ($script:FreshInstall) { return }
    if (-not (Test-Path (Join-Path $script:InstallDir 'docker-compose.yml'))) { return }

    $probe = @(
        '[ -f /usr/local/m2panel/browser/current/index.html ] && echo web'
        '[ -f /usr/local/m2panel/client.zip ] && echo desktop'
        'exit 0'
    ) -join '; '

    # Invoke-ComposeQuiet, which is this script's helper -- Invoke-DockerQuiet is
    # a function of the UPDATER, which lives in a here-string further up and is
    # not in scope here. -T because there is no terminal to attach.
    $seen = Invoke-ComposeQuiet @('run','--rm','-T','--no-deps','--entrypoint','sh','panel','-c',$probe)
    if ($seen.Code -eq 0 -and $seen.Output) {
        # Anchored to whole lines: compose prints its own progress into the same
        # stream, and a bare -match 'web' would find "webclient-fetcher" there
        # and report a browser client that is not installed.
        if ($seen.Output -match '(?m)^web$')     { $script:HaveWeb     = $true }
        if ($seen.Output -match '(?m)^desktop$') { $script:HaveDesktop = $true }
    }
}

# =============================================================================
#  The Custom Experience -- one question, everything behind it
# =============================================================================
#
#  Asked before anything large is downloaded, because the answer has to be known
#  before the server is assembled: most of what it turns on is patched into the
#  server tree between staging it and building the image from it, and an image
#  cannot be asked afterwards.
#
#  Where the answer comes from, in order, first one wins:
#
#      $env:M2_CUSTOM_EXPERIENCE          (unattended, first install)
#      -CustomExperience / -NoCustomExperience
#      what this install was set to last time   (.env, so an update keeps it)
#      the question, which defaults to no
#
function Select-CustomExperience {
    Write-Step 'Custom Experience'

    $envPath = Join-Path $script:InstallDir '.env'
    $prev = (Get-EnvValue $envPath 'M2_CUSTOM_EXPERIENCE') -in @('1', 'true', 'yes', 'on')

    $want = $null
    if ($env:M2_CUSTOM_EXPERIENCE) {
        $want = $env:M2_CUSTOM_EXPERIENCE -in @('1', 'true', 'yes', 'on')
        Write-Info 'taken from $env:M2_CUSTOM_EXPERIENCE'
    } elseif ($null -ne $script:CustomExperienceFlag) {
        $want = [bool]$script:CustomExperienceFlag
        Write-Info 'taken from the option you passed'
    } else {
        Write-Say 'This server can be left exactly as the original files play, or it'
        Write-Say 'can be set up the friendlier way this project has been using:'
        Write-Say ''
        Write-Say '  - items and Yang are picked up from twice as far away'
        Write-Say '  - calling your horse always works, instead of usually failing'
        Write-Say '  - the Horse Medal steps no longer make you wait until tomorrow'
        Write-Say '  - metin stones and bosses drop useful extras: blessing scrolls,'
        Write-Say '    reading potions, bravery capes and more'
        Write-Say '  - the General Store stocks the Musk Oil that one quest asks for'
        Write-Say '  - skill books for the same skill stack instead of filling the'
        Write-Say '    inventory one slot at a time'
        Write-Say '  - players may choose High Risk at level 15, and everyone walks'
        Write-Say '    and runs 20% faster'
        Write-Say ''
        Write-Say 'None of this changes rates, and none of it touches characters you'
        Write-Say 'already have. You can turn it off again later by running the'
        Write-Say 'installer with -NoCustomExperience.'
        Write-Say ''
        # Confirm-YesNo answers yes to everything under -Yes, which is right for
        # every other question in this file because every other question
        # defaults to yes. This one defaults to NO, so taking that answer would
        # switch the whole thing on for somebody who asked for nothing but
        # silence. -Yes means "don't ask me, take the default", and the default
        # here is no.
        if ($script:AssumeYes) {
            $want = $prev
            Write-Info "-Yes, so this keeps what is already set here: $(if ($prev) {'on'} else {'off'})"
        } else {
            $want = Confirm-YesNo 'Enable Custom Experience?' $prev
        }
    }

    $script:CustomExperience = [bool]$want

    # The two settings the Custom Experience carries that already had switches
    # of their own. Both are given a value here rather than left to
    # prepare-context.sh, because .env has to record what this build actually
    # used -- an .env that says 0 beside a server running at 20 is how weeks of
    # hand-made changes became impossible to account for.
    #
    # Turning it ON is what brings them with it, and only the first time: a
    # number the operator has since chosen is theirs, and every later update
    # carries it forward untouched.
    if ($script:CustomExperience) {
        # An explicit $env:M2_HIGH_RISK = '0' still wins -- somebody may want
        # everything here except a mode that lets players kill each other.
        $hr = $env:M2_HIGH_RISK
        if (-not $hr) { $hr = Get-EnvValue $envPath 'M2_HIGH_RISK' }
        $script:HighRisk = if ($hr -in @('0', 'false', 'no', 'off')) { '0' } else { '1' }

        # Ask what is already there FIRST, exactly as the High Risk lines above
        # do. Testing only $env: asks whether the ENVIRONMENT carries a number,
        # and on an update it never does -- the value lives in .env and is not
        # read until Get-Stack, further down. A server that had chosen 100 was
        # therefore handed 20 the first time the Custom Experience was switched
        # on, which is the opposite of what the comment above promises. The
        # Linux installer had the same hole; it was measured on a live server,
        # .env saying 100 going in and 20 coming out.
        $speed = $env:M2_MOVE_SPEED_BONUS
        if (-not $speed) { $speed = $script:MoveSpeedBonus }
        if (-not $speed) { $speed = Get-EnvValue $envPath 'M2_MOVE_SPEED_BONUS' }
        if ($speed) {
            $script:MoveSpeedBonus = $speed
        } elseif (-not $prev) {
            $script:MoveSpeedBonus = '20'
        }
    } else {
        $script:HighRisk = ''
    }

    if ($script:CustomExperience) {
        Write-Good 'Custom Experience: on.'
    } else {
        Write-Info 'Custom Experience: off -- the server files play as they shipped.'
    }
}

function Select-Clients {
    Write-Step 'Native Windows client'

    # The upstream WebClient was withdrawn by its author and is no longer
    # distributed or supported by this project. Keep the old switches accepted
    # so saved commands do not fail, but never fetch data or start its bridge.
    if ($script:WantWebFlag -eq $true) {
        Write-Warn '-WebClient is no longer available and will be ignored.'
        Write-Warn 'Use a compatible native Windows r40250 client.'
    }
    $script:WantWeb = $false
    $script:WantDesktop = -not $script:SkipClient
    if ($script:WantDesktop) {
        Write-Good 'Native client mode selected.'
        Write-Info 'The client files must be supplied locally; they are not in this repository.'
    } else {
        Write-Info 'Client preparation skipped (-NoClient). The server will still be installed.'
    }
    return

    if ($script:WantWebFlag -ne $null) {
        $script:WantWeb = $script:WantWebFlag
        Write-Info 'taken from the options you passed'
        return
    }

    Test-InstalledClients

    if ($script:HaveWeb -and $script:HaveDesktop) {
        $script:WantWeb = $true; $script:WantDesktop = $true
        Write-Good 'Both clients are installed -- keeping both up to date.'
        return
    }

    # -Yes must never block on a question. Keeping what is installed is the
    # answer that changes nothing, which is what an unattended run wants.
    if ($script:AssumeYes) {
        $script:WantWeb     = $script:HaveWeb
        $script:WantDesktop = $script:HaveDesktop -or -not $script:HaveWeb
        return
    }

    if ($script:HaveWeb -or $script:HaveDesktop) {
        $script:WantWeb     = $script:HaveWeb
        $script:WantDesktop = $script:HaveDesktop
        if (-not $script:HaveWeb) {
            # An install that predates the browser client lands here. It keeps
            # working untouched; this is only an offer.
            #
            # The default follows what was already asked for: somebody who set
            # M2_BROWSER_PLAY=1 by hand wanted this and is only missing the
            # files. Everyone else gets "no" -- 1.8 GB is not something to talk
            # anyone into.
            $prev = Get-EnvValue (Join-Path $script:InstallDir '.env') 'M2_BROWSER_PLAY'
            $def  = ($prev -in @('1','true','yes','on'))
            Write-Say 'This install offers the desktop client only.'
            Write-Say 'The browser client lets you play from a link -- no download, no'
            Write-Say 'install. It needs about 1.8 GB on this PC.'
            if ($def) { Write-Say '(M2_BROWSER_PLAY is already 1 here, so only the files are missing.)' }
            if (Confirm-YesNo 'Add the browser client?' $def) { $script:WantWeb = $true }
        }
        if (-not $script:HaveDesktop) {
            Write-Say 'This install offers the browser client only.'
            Write-Say 'The desktop client is the classic download behind the panel button.'
            if (Confirm-YesNo 'Add the desktop client as well?' $false) { $script:WantDesktop = $true }
        }
        return
    }

    Write-Say ''
    Write-Say '  1  Browser only    click a link and play              ~1.8 GB'
    Write-Say '  2  Desktop only    the classic download in the panel  ~1.3 GB'
    Write-Say '  3  Both                                               ~3.1 GB'
    Write-Say ''
    while ($true) {
        if (-not [Environment]::UserInteractive) { $pick = '3' }
        else {
            $pick = Read-Host '  Which one [3]'
            if ([string]::IsNullOrWhiteSpace($pick)) { $pick = '3' }
        }
        switch ($pick.Trim()) {
            '1' { $script:WantWeb = $true;  $script:WantDesktop = $false; return }
            '2' { $script:WantWeb = $false; $script:WantDesktop = $true;  return }
            '3' { $script:WantWeb = $true;  $script:WantDesktop = $true;  return }
            default { Write-Warn 'Type 1, 2 or 3.' }
        }
    }
}

# Put the browser client on the panel's volume. A task container: it runs,
# writes and exits. Only what is out of date is fetched -- an engine-only fix
# costs 17.6 MB, which is why the two archives are separate at all.
function Get-WebClient {
    if (-not $script:WantWeb) { return }
    Write-Step 'Fetching the browser client'

    if ($script:DryRun) {
        Write-Info '[dry-run] docker compose --profile webclient run --rm webclient-fetcher'
        return
    }

    $composeFile = Join-Path $script:InstallDir 'docker-compose.yml'
    if (-not (Select-String -Path $composeFile -Pattern 'webclient-fetcher' -Quiet -ErrorAction SilentlyContinue)) {
        Write-Warn 'These server files have no webclient-fetcher service.'
        Write-Warn 'Update the repository to get the browser client.'
        $script:WantWeb = $false
        return
    }

    # The fetcher's own exit codes, so this says what went wrong rather than
    # "it failed". The install is NOT aborted: a server whose game runs is worth
    # having even when the browser client could not be fetched.
    # Invoke-Compose, this script's helper, and its RETURN VALUE. $LASTEXITCODE
    # after calling a PowerShell function is whatever the last native command
    # inside it happened to leave behind, which is not the same thing and here
    # was not even the fetcher's.
    $rc = Invoke-Compose @('--profile','webclient','run','--rm','webclient-fetcher')

    # Ask whether a browser client is actually there, rather than believing the
    # exit code -- the same check install.sh makes, for the same reason: a
    # fetcher that started without its script once exited 0 while nothing
    # arrived, and the installer repeated that to the operator.
    $ok = Invoke-ComposeQuiet @('run','--rm','-T','--no-deps','--entrypoint','sh','panel','-c',
                                '[ -f /usr/local/m2panel/browser/current/index.html ]')
    if ($ok.Code -eq 0) {
        Write-Good 'Browser client installed.'
        $script:HaveWeb = $true
        return
    }
    switch ($rc) {
        3 { Write-Warn 'The fetcher is missing a tool it needs (unzip, curl, megatools).' }
        4 { Write-Warn 'A download did not finish. Check this PC''s connection, then run the installer again.' }
        5 { Write-Warn 'A downloaded archive had the wrong checksum -- damaged, or replaced.' }
        7 { Write-Warn 'Not enough disk space for the browser client (~1.8 GB unpacked).' }
        8 { Write-Warn 'The MEGA link for the browser client''s data has not been filled in yet.' }
        default { Write-Warn "Fetching the browser client failed (exit $rc)." }
    }
    Write-Warn 'The server itself is unaffected. Run the installer again to retry.'
    $script:WantWeb = $false
}

function Start-ClientBuild {
    Write-Step 'The game client'

    if (-not $script:WantDesktop) {
        Write-Info 'skipped (-NoClient)'
        $script:ClientState = 'skipped'
        return
    }

    $script:ClientDir = Join-Path $script:InstallDir 'client'
    $script:ClientLog = Join-Path $script:InstallDir 'client-build.log'

    # A name of their own per launch, so that starting a second copy while an
    # earlier one is still running cannot fail on a file the earlier one has
    # open -- which would have looked like "the client build could not be
    # started" and been nothing of the sort.
    #
    # Anything left from a previous install goes now. Windows holds these two
    # open for as long as the process it started them for is alive, and that
    # includes the process's own last breath: client-setup.ps1 cannot delete
    # the file that IS its standard output, however politely it asks. So the
    # next install does it. A pair still in use is locked and simply survives.
    $tempDir = [IO.Path]::GetTempPath()
    Get-ChildItem -LiteralPath $tempDir -Filter 'metin2-client-setup-*' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @('.out', '.err') } |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue }
    $tag = '{0}-{1}' -f (Get-Date -Format 'yyyyMMddHHmmss'), $PID
    $script:ClientHostOut = Join-Path $tempDir "metin2-client-setup-$tag.out"
    $script:ClientHostErr = Join-Path $tempDir "metin2-client-setup-$tag.err"

    # Always 1 on Windows -- Write-Configuration puts it there a few steps ago
    # -- but read back rather than assumed, so that somebody who edited .env by
    # hand gets the behaviour the file actually asks for.
    $envPath = Join-Path $script:InstallDir '.env'
    if (Test-Path -LiteralPath $envPath) {
        $script:LocalOnly = ((Get-EnvValue $envPath 'M2_LOCAL_ONLY') -eq '1')
    }

    if ($script:DryRun) {
        Write-Info '[dry-run] docker compose run --rm client-builder'
        if ($script:LocalOnly) {
            Write-Info "[dry-run] then unpack it into $($script:ClientDir)"
            Write-Info "[dry-run] and put a `"$($script:ShortcutName)`" shortcut on the Desktop"
            $script:ClientState = 'local-working'
        } else {
            $script:ClientState = 'building'
        }
        return
    }

    # -- the fast path: it is all already here --------------------------------
    if ($script:LocalOnly) {
        $script:ClientExe = Get-LocalClientExe
        if ($script:ClientExe) {
            Write-Good 'The game is already unpacked on this PC.'
            Write-Info $script:ClientExe
            $script:ClientState = 'local-ready'
            # One thing can still have gone missing on its own: people delete
            # things off their Desktop. client-setup.ps1 notices in a second or
            # two and puts it back -- and never touches one that is there. An
            # existing copy of it is left exactly as it is: it was written by
            # the install that built the client and knows how to build one
            # again, which a fresh one written here would not.
            $helper = Join-Path $script:InstallDir 'client-setup.ps1'
            if (-not (Test-Path -LiteralPath $helper)) {
                $helper = Write-ClientSetupScript -BuildArgs @() -ArchiveInContainer ''
            }
            # Waited for, unlike the real thing: with the game already here it
            # has nothing to do but look at a shortcut, and waiting is what
            # lets the two redirect files be tidied away afterwards.
            try {
                $p = Start-ClientSetupDetached $helper
                [void]$p.WaitForExit(30000)
            } catch { }
            Merge-ClientSideLogs
            return
        }
    } else {
        if (Test-ClientZipPresent) {
            Write-Good 'A patched client is already in place.'
            $script:ClientState = 'ready'
            return
        }
    }

    # client-builder sits behind the "client" compose profile, so a plain
    # 'config --services' does not list it at all. 'run' turns the profile on by
    # itself, but this check has to ask for it explicitly.
    $svc = Invoke-ComposeQuiet @('--profile', 'client', 'config', '--services')
    $haveBuilder = (($svc.Output -split "`n" | ForEach-Object { $_.Trim() }) -contains 'client-builder')
    # A release with no builder is only a dead end when there is also no client
    # in the panel: given one, everything after this -- copy out, unpack,
    # shortcut -- still works and is still worth doing.
    if (-not $haveBuilder -and -not (Test-ClientZipPresent)) {
        Write-Warn 'This release does not include the automatic client builder.'
        Write-Warn 'The server works; there is just nothing to play with yet.'
        Write-Warn ''
        Write-Warn 'To supply one yourself, put a client.zip whose serverinfo.py'
        Write-Warn 'points at 127.0.0.1 in place with:'
        Write-Warn "    cd `"$($script:InstallDir)`""
        Write-Warn '    docker compose cp .\client.zip panel:/usr/local/m2panel/client.zip'
        Write-Warn '    docker compose restart panel'
        if ($script:LocalOnly) {
            # Written even though there is nothing to build, so that the advice
            # in the summary -- run this afterwards and the game unpacks itself
            # onto the Desktop -- is true rather than aspirational.
            [void](Write-ClientSetupScript -BuildArgs @() -ArchiveInContainer '')
        }
        $script:ClientState = 'unavailable'
        return
    }

    # The archive the source fetch already downloaded is the SAME file the
    # client builder wants: one package containing both Server/ and Client/.
    # The two keep separate caches, so without this it fetches its own copy --
    # another 1.7 GB, another hour, and one more chance for the share to refuse.
    # The source cache is a Docker volume here rather than a host path, so we
    # mount it read-only and name the file instead of bind-mounting it.
    $reuseArgs = @()
    $cachedArchive = ''
    # If the operator handed us the unpacked server files, the client is already
    # sitting in them as Client\Client.zip -- around 1.2 GB that would otherwise
    # be fetched from MEGA a second time, on a share whose quota runs out
    # regularly. The builder scans its drop folder for a .zip/.rar/.7z, so
    # mounting that folder is all it takes; it filters ClientVS22.zip (the C++
    # source that sits beside it) out by name on its own.
    if ($haveBuilder -and $script:ClientArchive) {
        if (-not (Test-Path -LiteralPath $script:ClientArchive -PathType Leaf)) {
            Write-Warn "M2_CLIENT_ARCHIVE points at '$($script:ClientArchive)', which is not a file."
            $script:ClientState = 'unavailable'
            return
        }
        $clientPath = ConvertTo-MountPath $script:ClientArchive
        $clientLeaf = Split-Path -Leaf $clientPath
        $reuseArgs = @('-v', "$($clientPath):/archive/$($clientLeaf):ro")
        $cachedArchive = "/archive/$clientLeaf"
        Write-Info "the native client comes from $clientPath -- nothing to download"
    }
    if ($haveBuilder -and $reuseArgs.Count -eq 0 -and $script:SrcRefDir) {
        $refClient = Join-Path $script:SrcRefDir 'Client'
        if (Test-Path -LiteralPath $refClient -PathType Container) {
            $p = ConvertTo-MountPath $refClient
            $reuseArgs = @('-v', "$($p):/archive:ro")
            Write-Info "the client comes from $p -- nothing to download"
        }
    }
    if ($haveBuilder -and $reuseArgs.Count -eq 0 -and
        -not (Get-EnvValue $envPath 'M2_CLIENT_ARCHIVE_URL')) {
        Write-Warn 'No native client archive was supplied.'
        Write-Warn 'The server is ready; use your existing compatible client, or run again with:'
        Write-Warn "    -ClientArchive 'C:\path\to\Reference_Client.zip'"
        $script:ClientState = 'unavailable'
        return
    }

    $buildArgs = @()
    if ($haveBuilder) {
        # -T: no pseudo-terminal. Its output goes to a log file, not a console.
        $buildArgs = @('compose','run','--rm','-T') + $reuseArgs + @('client-builder')
    }

    Write-Say 'Starting the client build in the background.'
    if ($reuseArgs.Count -gt 0) {
        Write-Say 'It reuses the archive already downloaded for the server, so this'
        Write-Say 'is a repack rather than another download -- but repacking a'
        Write-Say 'gigabyte still takes a while, and longer on a slow disk.'
    } elseif ($haveBuilder) {
        Write-Say 'It downloads over a gigabyte and then repacks it, so it takes a'
        Write-Say 'while -- often 20 to 60 minutes depending on your connection.'
    }
    if ($script:LocalOnly) {
        Write-Say ''
        Write-Say 'When it is done it unpacks the game onto this PC by itself and'
        Write-Say "puts a `"$($script:ShortcutName)`" shortcut on your Desktop. That"
        Write-Say 'happens after this installer has finished, so there is nothing'
        Write-Say 'to wait for and nothing to download.'
    }
    Write-Say ''
    Write-Say 'You do not have to wait. The server is usable now.'

    '' | Set-Content -LiteralPath $script:ClientLog -Encoding ASCII
    # The builder bind-mounts ./client-archive so you can hand it a file rather
    # than have it download one. Create it here so Docker does not.
    New-Item -ItemType Directory -Force -Path (Join-Path $script:InstallDir 'client-archive') | Out-Null

    $helper = Write-ClientSetupScript -BuildArgs $buildArgs -ArchiveInContainer $cachedArchive
    try {
        $proc = Start-ClientSetupDetached $helper
    } catch {
        Write-Warn "The client build could not be started: $($_.Exception.Message)"
        $script:ClientState = 'failed'
        return
    }
    $script:ClientState = if ($script:LocalOnly) { 'local-working' } else { 'building' }

    # Watch it for a minute and a half before saying anything about it.
    #
    # The naive version -- sleep a few seconds, grep the log for "error" --
    # gets it wrong in both directions: it calls a slow but healthy build
    # broken because the image build printed a warning, and it calls a build
    # that died thirty seconds in "still running", so the operator waits an
    # hour for a game that was never coming. Watching the process itself
    # cannot be wrong about which of those happened.
    Write-Host ''
    Write-Host '  watching it for a moment to be sure it really started' -NoNewline
    $deadline = (Get-Date).AddSeconds(90)
    while ((Get-Date) -lt $deadline) {
        if ($proc.HasExited) {
            Write-Host ''
            Merge-ClientSideLogs
            if ($script:LocalOnly) { $script:ClientExe = Get-LocalClientExe }
            $done = if ($script:LocalOnly) { [bool]$script:ClientExe } else { (Test-ClientZipPresent) }
            if ($done) {
                if ($script:LocalOnly) {
                    Write-Good 'The game is unpacked and ready -- that was quick.'
                    Write-Info $script:ClientExe
                    $script:ClientState = 'local-ready'
                } else {
                    Write-Good 'The client is built and in place -- that was quick.'
                    $script:ClientState = 'ready'
                }
            } else {
                Write-Warn 'That stopped almost immediately, so there is no game'
                Write-Warn 'yet. The server itself is fine and everything else'
                Write-Warn 'below still applies.'
                Write-Warn ''
                Write-Warn 'The last few lines of the log:'
                if (Test-Path -LiteralPath $script:ClientLog) {
                    foreach ($l in (Get-Content -LiteralPath $script:ClientLog -Tail 10 -ErrorAction SilentlyContinue)) {
                        if ($l.Trim()) { Write-Warn "    $l" }
                    }
                }
                Write-Warn "Full log: $($script:ClientLog)"
                $script:ClientState = 'failed'
            }
            return
        }
        # On a local install a client.zip in the panel is only half the story --
        # it still has to be copied out and unpacked -- so the finished game on
        # disk is what is watched for here, not the zip.
        if ($script:LocalOnly) {
            $script:ClientExe = Get-LocalClientExe
            if ($script:ClientExe) {
                Write-Host ''
                Write-Good 'The game is unpacked and ready.'
                Write-Info $script:ClientExe
                $script:ClientState = 'local-ready'
                return
            }
        } elseif (Test-ClientZipPresent) {
            Write-Host ''
            Write-Good 'The client is built and in place.'
            $script:ClientState = 'ready'
            return
        }
        Write-Host '.' -NoNewline
        Start-Sleep -Seconds 5
    }
    Write-Host ''
    Write-Good 'Still going after 90 seconds, which is what a real build looks like.'
    Write-Info "Watch it with:  Get-Content -Wait `"$($script:ClientLog)`""
}

# Start client-setup.ps1 so that it outlives this installer.
#
# Start-Process makes an independent process rather than a child that dies with
# us, which is the whole point: the build is twenty to sixty minutes and this
# script is going to print its summary and return long before that.
#
# The script path is quoted by hand. -ArgumentList joins its elements with
# spaces and quotes nothing, so an install folder with a space in its name --
# "C:\Users\Anne Marie\Metin2Server" -- would otherwise arrive as two arguments
# and PowerShell would open a file called "C:\Users\Anne".
function Start-ClientSetupDetached {
    param([string]$ScriptPath)
    $ps = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    if (-not (Test-Path -LiteralPath $ps)) { $ps = 'powershell.exe' }
    return (Start-Process -FilePath $ps `
        -ArgumentList @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
                        '-File', ('"' + $ScriptPath + '"')) `
        -WorkingDirectory $script:InstallDir `
        -RedirectStandardOutput $script:ClientHostOut `
        -RedirectStandardError  $script:ClientHostErr `
        -WindowStyle Hidden -PassThru)
}

# client-setup.ps1 writes the client build log itself, one line at a time, so
# the two files it is started with are normally empty: they exist only to catch
# what PowerShell ITSELF says when the script cannot even be read -- a parse
# error, an execution policy that blocks it. That is the one thing a script
# cannot log about itself.
#
# They live in the temp folder rather than beside the log on purpose: the
# person is told to look at one file, and two empty ones next to it invite the
# question "which of these is it?". client-setup.ps1 folds them in and deletes
# them when it is done, which is every case except the one where it is still
# running and we are the ones who have to -- hence this.
function Merge-ClientSideLogs {
    foreach ($f in @($script:ClientHostOut, $script:ClientHostErr)) {
        if (-not $f) { continue }
        if ((Test-Path -LiteralPath $f) -and ((Get-Item -LiteralPath $f).Length -gt 0)) {
            Add-Content -LiteralPath $script:ClientLog -Value ''
            Add-Content -LiteralPath $script:ClientLog -Value "--- $(Split-Path -Leaf $f) ---"
            Get-Content -LiteralPath $f -ErrorAction SilentlyContinue |
                Add-Content -LiteralPath $script:ClientLog
        }
        Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue
    }
}

# =============================================================================
#  Step 7 -- the summary
#
#  Three things, and they have to be impossible to miss.
# =============================================================================

function Show-Summary {
    $url = "http://127.0.0.1:$($script:PanelPort)"
    # A local install has no passphrase -- the panel lets you straight in,
    # because it listens to this computer and nothing else. Printing a password
    # nobody is ever asked for only makes people think they have to keep it.
    # A local server never asks for the passphrase, so printing one only raises
    # the question of what it is for -- which is why this section is normally
    # skipped here. Once the operator has deliberately set one in the panel it
    # stops being noise and becomes the thing they want confirmed after every
    # update, so then it is shown.
    $showPassword = (-not $script:LocalOnly) -or $script:PanelPasswordChosen

    # Everything below is ordered so that the part people actually need is the
    # last thing on screen when the installer finishes. The warnings and the
    # reference material come first: they are worth reading once, and they
    # scroll away, which is right for a thing you read once.

    # --------------------------------------------------- the important caveat
    if ($script:LocalOnly) {
        Write-Host ''
        Write-Host ''
        Write-Host '  ================================================================' -ForegroundColor Yellow
        Write-Host '    THIS SERVER IS FOR YOU ALONE' -ForegroundColor Yellow
        Write-Host '  ================================================================' -ForegroundColor Yellow
        Write-Host ''
        Write-Host '  Everything installed here listens on 127.0.0.1, which means'
        Write-Host '  "this computer and nothing else".'
        Write-Host ''
        Write-Host '    - Nobody else can join. Not your friends over the internet,'
        Write-Host '      and not someone on the same Wi-Fi in the same room.'
        Write-Host '    - No port was opened. No firewall rule was created. Nothing'
        Write-Host '      about this PC is now reachable that was not before.'
        Write-Host '    - Your home IP address has not been given out to anyone.'
        Write-Host ''
        Write-Host '  That is on purpose. It lets you play, build your server and try'
        Write-Host '  things out with no risk at all.'
        Write-Host ''
        Write-Host '  If you later want friends to play on it:' -ForegroundColor White
        Write-Host ''
        Write-Host '    Do not open ports on your home router. A home connection means'
        Write-Host '    handing your home address to every player, your upload speed is'
        Write-Host '    the bottleneck, and the server disappears whenever the PC does.'
        Write-Host ''
        Write-Host '    Rent a small Linux VPS instead -- 4 GB of memory is about 5 EUR'
        Write-Host '    a month at Hetzner, Contabo or Netcup -- and run one line on it:'
        Write-Host ''
        Write-Host '        curl -fsSL https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.sh | sudo sh -s -- ' -ForegroundColor Cyan
        Write-Host '            --archive /path/Reference_Server.zip --no-client' -ForegroundColor Cyan
        Write-Host ''
        Write-Host '    That installer does the opposite of this one: it publishes the'
        Write-Host '    game to the internet, opens the firewall, and can put a real'
        Write-Host '    HTTPS certificate on the admin panel. Your characters here can'
        Write-Host '    be moved across with a database backup.'
        Write-Host ''
    }

    # ------------------------------------------------------------ day to day
    Write-Host '  Day to day' -ForegroundColor White
    Write-Host ''
    Write-Host "     cd `"$($script:InstallDir)`""
    Write-Host '     docker compose ps                 what is running'
    Write-Host '     docker compose logs -f game       watch the game log'
    Write-Host '     docker compose restart            restart everything'
    Write-Host '     docker compose down               stop (keeps all player data)'
    Write-Host '     docker compose up -d              start again'
    Write-Host ''
    Write-Host '     The one dangerous command is "docker compose down -v".' -ForegroundColor Yellow
    Write-Host '     The -v deletes every account, character and item, with no undo.'
    Write-Host ''
    if ($script:LocalOnly -and $script:ClientState -in @('local-ready','local-working')) {
        Write-Host "     The game lives in $($script:ClientDir)." -ForegroundColor White
        Write-Host '     If the Desktop shortcut goes missing, or the unpacking did not'
        Write-Host '     finish, this puts it right -- and does nothing when there is'
        Write-Host '     nothing to do:'
        Write-Host "         powershell -ExecutionPolicy Bypass -File `"$(Join-Path $script:InstallDir 'client-setup.ps1')`""
        Write-Host ''
    }
    Write-Host '     The original server files are kept in a Docker volume, so'
    Write-Host '     re-installing never downloads them twice. Once you are happy'
    Write-Host '     with the server you can have those few gigabytes back:'
    Write-Host "         docker volume rm $($script:SrcVolume)"
    Write-Host "         docker image rm $($script:FetcherImage)"
    Write-Host ''
    Write-Host '     Docker Desktop must be running for the server to be up. It'
    Write-Host '     starts with Windows by default, and the server comes back with'
    Write-Host '     it, so after a reboot the panel link just works again.'
    Write-Host ''
    Write-Host '     To update later, run this installer again. It keeps your'
    Write-Host '     accounts, characters and settings, and tells you what version'
    Write-Host '     you are on before it changes anything.'
    Write-Host ''

    # ------------------------------------------------- what you came here for
    Write-Host ''
    Write-Host '  ================================================================' -ForegroundColor Green
    Write-Host '    YOUR METIN2 SERVER IS INSTALLED' -ForegroundColor Green
    Write-Host '  ================================================================' -ForegroundColor Green
    Write-Host ''
    if ($showPassword) { Write-Host '  Write these three things down now.' }
    else               { Write-Host '  These two are what you need.' }
    Write-Host ''

    # ------------------------------------------------------------------- 1
    Write-Host '  1. THE GAME -- this is what you play with' -ForegroundColor White
    Write-Host ''
    switch ($script:ClientState) {
        # The game is on this PC and playable. No link, because there is
        # nothing to fetch: it was built here.
        'local-ready' {
            Write-Host "       Your Desktop -> `"$($script:ShortcutName)`"" -ForegroundColor Cyan
            Write-Host ''
            Write-Host '     Double-click it and you are in. It is already set up to'
            Write-Host '     connect to the server on this PC -- there is nothing to'
            Write-Host '     download and nothing to type in.'
            Write-Host ''
            Write-Host "     The game itself is in $($script:ClientDir)"
            if ($script:ClientExe) {
                Write-Host "     and starts from $(Split-Path -Leaf $script:ClientExe)."
            }
        }
        # Being built and then unpacked, right here, by itself. Saying
        # "download it from the panel" would be a lie on this machine.
        'local-working' {
            Write-Host "       Your Desktop -> `"$($script:ShortcutName)`"" -ForegroundColor Cyan
            Write-Host ''
            Write-Host '     It is not there yet.' -ForegroundColor Yellow
            Write-Host '     The game is still being put together in the background: over'
            Write-Host '     a gigabyte to repack, so give it 20 to 60 minutes. When it is'
            Write-Host "     done it unpacks itself into $($script:ClientDir)"
            Write-Host '     and that shortcut appears on your Desktop on its own.'
            Write-Host ''
            Write-Host '     You do not have to do anything, and you can close this window.'
            Write-Host '     Watch it if you like:'
            Write-Host "         Get-Content -Wait `"$($script:ClientLog)`""
            Write-Host ''
            Write-Host '     If the shortcut never turns up, that log says why -- and the'
            Write-Host "     panel keeps a copy you can fetch by hand at $url/download"
        }
        # A server other people connect to: the download page is the point.
        'ready' {
            Write-Host "       $url/download" -ForegroundColor Cyan
            Write-Host ''
            Write-Host '     Ready now. It is already set up to connect to the server'
            Write-Host '     on this PC.'
        }
        'building' {
            Write-Host "       $url/download" -ForegroundColor Cyan
            Write-Host ''
            Write-Host '     This link does NOT work yet.' -ForegroundColor Yellow
            Write-Host '     The client is still being built in the background -- it is a'
            Write-Host '     download of over a gigabyte that then has to be repacked, so'
            Write-Host '     give it 20 to 60 minutes.'
            Write-Host ''
            Write-Host '     Until it finishes the page politely says the download is not'
            Write-Host '     ready. Nothing is broken. Check on it with:'
            Write-Host "         Get-Content -Wait `"$($script:ClientLog)`""
        }
        'failed' {
            Write-Host '       (not available -- it stopped with an error)' -ForegroundColor Yellow
            Write-Host ''
            Write-Host '     The server itself is fine; only the game did not get built.'
            Write-Host '     The log says what happened:'
            Write-Host "         $($script:ClientLog)"
            Write-Host ''
            Write-Host '     When you have dealt with what it says, try again with:'
            Write-Host "         powershell -ExecutionPolicy Bypass -File `"$(Join-Path $script:InstallDir 'client-setup.ps1')`""
            Write-Host ''
            Write-Host "     If a client was built before it failed, $url/download"
            Write-Host '     still has it.'
        }
        'skipped' {
            Write-Host '       (client preparation skipped)' -ForegroundColor Yellow
            Write-Host ''
            Write-Host '     Use your existing compatible native Windows client and set:'
            Write-Host '       address: 127.0.0.1'
            Write-Host "       auth:    $($script:AuthPort)"
            Write-Host "       game:    $($script:GamePorts)"
        }
        default {
            Write-Host '       (no game yet)' -ForegroundColor Yellow
            Write-Host ''
            Write-Host '     This release has no automatic client builder. Put your own'
            Write-Host '     client.zip in place with:'
            Write-Host "         cd `"$($script:InstallDir)`""
            Write-Host '         docker compose cp .\client.zip panel:/usr/local/m2panel/client.zip'
            Write-Host '         docker compose restart panel'
            Write-Host ''
            Write-Host '     ...and then, to unpack it here and get a Desktop shortcut:'
            Write-Host "         powershell -ExecutionPolicy Bypass -File `"$(Join-Path $script:InstallDir 'client-setup.ps1')`""
        }
    }
    Write-Host ''
    Write-Rule
    Write-Host ''

    # ------------------------------------------------------------------- 2
    Write-Host '  2. YOUR ADMIN PANEL -- this is where you run the server' -ForegroundColor White
    Write-Host ''
    Write-Host "       $url" -ForegroundColor Cyan
    Write-Host ''
    Write-Host '     Open that in your browser. It only works on this PC.'
    if (-not $showPassword) {
        Write-Host '     There is no password to type: nobody else can reach it.'
    }
    Write-Host ''

    # ------------------------------------------------------------------- 3
    if ($showPassword) {
        Write-Rule
        Write-Host ''
        Write-Host '  3. YOUR ADMIN PANEL PASSWORD' -ForegroundColor White
        Write-Host ''
        if ($script:PanelPasswordKnown -and $script:PanelPassword) {
            Write-Host "       $($script:PanelPassword)" -ForegroundColor Cyan
            Write-Host ''
            if ($script:PanelPasswordChosen) {
                Write-Host '     This is the one you chose in the admin panel. Updating'
                Write-Host '     does not change it, and this line will keep showing it.'
            } elseif ($script:PanelPasswordNew) {
                Write-Host '     Generated on this PC just now, for this server only.'
                Write-Host '     You can pick your own in the panel; it is offered right'
                Write-Host '     under the introduction on the admin page.'
            } else {
                Write-Host '     This is the password from when the server was first'
                Write-Host '     installed. It has not been changed. You can pick your'
                Write-Host '     own in the panel, under the introduction.'
            }
            Write-Host "     It is also kept in $(Join-Path $script:InstallDir '.env')"
            Write-Host '     so you can look it up again.'
        } else {
            Write-Host '       (unknown -- this server was installed before)' -ForegroundColor Yellow
            Write-Host ''
            Write-Host '     The panel keeps only a one-way hash of its password, so it'
            Write-Host '     cannot be recovered. To set a new one:'
            Write-Host ''
            Write-Host "         cd `"$($script:InstallDir)`""
            Write-Host '         docker compose exec panel rm /usr/local/etc/m2panel.conf'
            Write-Host '         docker compose restart panel'
            Write-Host '         docker compose logs panel | Select-String -Context 0,4 "ADMIN PANEL PASSWORD"'
        }
        Write-Host ''
    }
    Write-Host '  ================================================================' -ForegroundColor Green
    Write-Host ''
}

# =============================================================================
#  main
# =============================================================================

function Invoke-Metin2Install {
    [CmdletBinding()]
    param(
        [switch]$DryRun,
        [switch]$Yes,
        [string]$InstallDir = '',
        [int]$AuthPort = 0,
        [string]$GamePorts = '',
        [int]$PanelPort = 0,
        [string]$RepoDir = '',
        [string]$RepoUrl = '',
        [string]$ReferenceDir = '',
        [string]$Archive = '',
        [string]$ClientArchive = '',
        [string]$LocalContext = '',
        # Given, these skip the question entirely -- for an unattended run that
        # knows what it wants. Not given, the installer asks once.
        [switch]$WebClient,
        [switch]$NoWebClient,
        [switch]$NoClient,
        # The same arrangement for the Custom Experience: given, the question is
        # skipped; not given, the installer asks once and defaults to no.
        [switch]$CustomExperience,
        [switch]$NoCustomExperience,
        [switch]$Help
    )

    if ($Help) {
        Write-Host @'

  Metin2 server installer (Windows)

    irm https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.ps1 | iex

  With options:

    iex "& { $(irm https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/installer/install.ps1) } -DryRun"

    -DryRun            show what would happen, change nothing
    -Yes               don't ask anything; accept every default
    -InstallDir PATH   where to install (default: %USERPROFILE%\Metin2Server)
    -AuthPort N        login port (default: 11000)
    -GamePorts A-B     channel ports (default: 13000-13002)
    -PanelPort N       admin panel port (default: 7788)
    -WebClient         deprecated compatibility switch; ignored (native only)
    -NoWebClient       compatibility switch; native-only is already the default
    -NoClient          install the server only; use an existing native client
    -CustomExperience  turn the Custom Experience on without asking: bigger
                       pick-up range, a horse that always comes when called,
                       no waiting between the Horse Medal steps, bonus drops
                       on metins and bosses, Musk Oil in the General Store,
                       skill books that stack, High Risk offered, and
                       everyone 20% faster on foot
    -NoCustomExperience
                       leave the Custom Experience off, and don't ask
    -Help              this text

  Where the server comes from:

    -ReferenceDir DIR  an already-unpacked "[40250] Reference Serverfile"
                       folder (the one with Server\ in it). Nothing is
                       downloaded when you give this.
    -Archive PATH      the server-file package as you downloaded it -- the
                       .zip/.rar/.7z, or metin2_server+src.tar.gz
    -ClientArchive PATH
                       a compatible native Windows client archive to configure
                       locally (not included or downloaded by this project)
    -RepoDir PATH      use this checkout of the project instead of cloning one
    -RepoUrl URL       clone the project from here
    -LocalContext DIR  skip all of the above: install from a Docker build
                       context that is already prepared

  Environment variables -- the same things, for when an option is awkward
  to pass through "irm ... | iex":

    $env:M2_REPO_URL           $env:M2_REPO_DIR
    $env:M2_SRC_REFERENCE_DIR  $env:M2_SRC_ARCHIVE   $env:M2_SRC_URL
    $env:M2_CLIENT_ARCHIVE
    $env:M2_LOCAL_CONTEXT      $env:M2_SRC_VOLUME

  Everything installs bound to 127.0.0.1. Nobody else can connect.

'@
        return
    }

    $script:DryRun    = [bool]$DryRun
    $script:AssumeYes = [bool]$Yes
    # Only set when a switch was actually given -- $null means "not asked yet",
    # which is what lets a re-run tell a deliberate no from a default.
    if     ($WebClient)   { $script:WantWebFlag = $true  }
    elseif ($NoWebClient) { $script:WantWebFlag = $false }
    $script:SkipClient = [bool]$NoClient
    if     ($CustomExperience)   { $script:CustomExperienceFlag = $true  }
    elseif ($NoCustomExperience) { $script:CustomExperienceFlag = $false }
    $script:InstallDir = if ($InstallDir) { $InstallDir }
                         elseif ($env:M2_INSTALL_DIR) { $env:M2_INSTALL_DIR }
                         else { Join-Path $env:USERPROFILE 'Metin2Server' }
    if ($AuthPort  -gt 0) { $script:AuthPort  = $AuthPort }
    if ($GamePorts)       { $script:GamePorts = $GamePorts }
    if ($PanelPort -gt 0) { $script:PanelPort = $PanelPort }
    # A command-line option beats the environment variable of the same name.
    if ($RepoDir)      { $script:RepoDir      = $RepoDir }
    if ($RepoUrl)      { $script:RepoUrl      = $RepoUrl }
    if ($ReferenceDir) { $script:SrcRefDir    = $ReferenceDir }
    if ($Archive)      { $script:SrcArchive   = $Archive }
    if ($ClientArchive) { $script:ClientArchive = $ClientArchive }
    if ($LocalContext) { $script:LocalContext = $LocalContext }

    Write-Host ''
    Write-Host '  Metin2 server installer' -ForegroundColor White
    Write-Host '  for Windows -- a private server on this PC' -ForegroundColor DarkGray
    Write-Host ''
    Write-Host '  Everything will be bound to 127.0.0.1: this PC and nothing else.'
    Write-Host '  Nobody else will be able to connect, and no port will be opened.'
    if ($script:DryRun) {
        Write-Host ''
        Write-Host '  ** DRY RUN -- nothing on this PC will be changed **' -ForegroundColor Yellow
    }

    try {
        Assert-ValidPortConfiguration
        Test-Machine
        Initialize-Docker
        # ASKED BEFORE ANYTHING LARGE IS DOWNLOADED.
        #
        # The answer decides what gets fetched -- the browser client's 1.75 GB
        # of data, the desktop client's 1.29 GB, or neither -- so asking after
        # the server files have already come down means a long wait before the
        # one question the operator actually has to answer. It also decides
        # M2_BROWSER_PLAY, so it still has to precede Write-Configuration: the
        # bridge must not be switched on for an install with no browser client
        # to serve.
        Test-ExistingInstall
        Select-Clients
        # Asked here for the same reason, and it must be before Get-Stack:
        # almost everything the answer turns on is patched into the server tree
        # between staging it and building the image from it, which happens in
        # there.
        Select-CustomExperience

        Get-Stack
        Write-Configuration
        Write-LoopbackOverride
        Start-Stack
        Wait-Healthy
        # Before the bridge -- the panel shows its Play button only once an
        # index.html is really on the volume.
        Get-WebClient
        Start-BrowserBridge
        Start-ClientBuild
        Show-Summary
    }
    catch [System.OperationCanceledException] {
        # Every message the user needs was already printed by whoever threw.
        return
    }
    catch {
        # The safety net. Nobody should ever see a raw .NET stack trace from
        # an installer aimed at people who have never opened PowerShell before.
        Write-Host ''
        Write-Host '  Something unexpected went wrong.' -ForegroundColor Red
        Write-Host ''
        Write-Host "  $($_.Exception.Message)"
        Write-Host ''
        Write-Host '  Nothing on this PC has been left half-finished in a way that'
        Write-Host '  stops you trying again -- running the same one line a second'
        Write-Host '  time is safe and picks up where this left off.'
        Write-Host ''
        Write-Host '  If it keeps happening, this is the detail to report:'
        Write-Host ''
        Write-Host "    $($_.InvocationInfo.PositionMessage)" -ForegroundColor DarkGray
        Write-Host "    $($_.CategoryInfo.Category) / $($_.FullyQualifiedErrorId)" -ForegroundColor DarkGray
        Write-Host ''
        return
    }
}

Invoke-Metin2Install @args
