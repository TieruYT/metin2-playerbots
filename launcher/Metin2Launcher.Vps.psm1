#requires -Version 5.1
# This world on a rented Linux VPS ("Instaluj na VPS"): the launcher uploads
# the server folder over SSH and runs linux-port/tools/vps-install.sh there.
# Installing, updating and opening the panels are for everybody; invite codes
# for friends are behind the COOP testers' password (Test-M2CoopAccess), like
# the invites for a world hosted at home.
#
#   state     .m2vps.json beside the launcher's other state files: the host,
#             the user, the port, the key's path, the folder on the VPS.
#             Never a password - there is no field for one.
#   key       an ed25519 key without a passphrase in %USERPROFILE%\.ssh, not
#             in the server folder, where a full package or a support bundle
#             could take it along. The VPS password is typed once, into a
#             visible ssh window, which puts the key in authorized_keys; every
#             other call runs with BatchMode and the key.
#   upload    bsdtar (C:\Windows\System32\tar.exe) of the folder, streamed
#             into ssh by this process - never through a PowerShell pipeline,
#             which turns bytes into lines of text.
#   remote    vps-install.sh does the work on the VPS: Docker, a swap file,
#             .env with the panels on 127.0.0.1, the build detached, the
#             shipped admin/test passwords replaced. This side starts it and
#             reads `status' until the build is done.
#   panel     an ssh -L tunnel to the panels, which listen on the VPS's
#             loopback only.
#
# Windows 10/11 ship everything used here: OpenSSH (ssh, ssh-keygen) and
# bsdtar. Windows PowerShell 5.1, StrictMode 2.0, like the other modules.
Set-StrictMode -Version 2.0

$script:VpsStateFile = '.m2vps.json'
$script:VpsDefaultRemoteDir = '/opt/metin2'
$script:VpsDefaultWorldName = 'Serwer VPS'
$script:VpsScript = 'linux-port/tools/vps-install.sh'

# The sizes vps-install.sh judges a machine by, in MB of what the kernel
# reports (a VPS sold as 8 GB shows about 7.7 GiB). The two must agree.
$script:VpsMinMemMB = 3500
$script:VpsWarnMemMB = 7400
$script:VpsMinDiskMB = 12000
$script:VpsWarnDiskMB = 40000

# ---------------------------------------------------------------- state

function Get-M2VpsStatePath {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    return (Join-Path $ServerRoot $script:VpsStateFile)
}

function Get-M2VpsDefaultKeyPath {
    # Not $home: that is PowerShell's own read-only $HOME.
    $profileDir = $env:USERPROFILE
    if (-not $profileDir) { $profileDir = $env:HOME }
    return (Join-Path (Join-Path $profileDir '.ssh') 'metin2_vps')
}

function New-M2VpsStateObject {
    # The whole state, one field at a time from what was read: a field that
    # is not one of these is not kept, so a password never reaches the file
    # however it got into the object.
    param($From = $null)
    $state = [pscustomobject]@{
        schema = 1; host = ''; user = 'root'; port = 22; keyPath = (Get-M2VpsDefaultKeyPath)
        remoteDir = $script:VpsDefaultRemoteDir; worldName = $script:VpsDefaultWorldName
        tunnelPid = 0; tunnelPorts = @(); lastInstall = ''; lastVersion = ''
    }
    if ($From) {
        $names = @($From.PSObject.Properties.Name)
        foreach ($name in @('host', 'user', 'keyPath', 'remoteDir', 'worldName', 'lastInstall', 'lastVersion')) {
            if ($names -contains $name -and $null -ne $From.$name -and [string]$From.$name) { $state.$name = ([string]$From.$name).Trim() }
        }
        foreach ($name in @('port', 'tunnelPid')) {
            if ($names -contains $name) {
                $number = 0
                if ([int]::TryParse([string]$From.$name, [ref]$number)) { $state.$name = $number }
            }
        }
        if ($names -contains 'tunnelPorts' -and $From.tunnelPorts) {
            $ports = @()
            foreach ($pair in @($From.tunnelPorts)) {
                if ($pair -and (@($pair.PSObject.Properties.Name) -contains 'remote') -and (@($pair.PSObject.Properties.Name) -contains 'local')) {
                    $ports += [pscustomobject]@{ remote = [int]$pair.remote; local = [int]$pair.local }
                }
            }
            $state.tunnelPorts = $ports
        }
    }
    return $state
}

function Get-M2VpsState {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $path = Get-M2VpsStatePath -ServerRoot $ServerRoot
    $read = $null
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        try { $read = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json } catch { $read = $null }
    }
    return (New-M2VpsStateObject -From $read)
}

function Save-M2VpsState {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)]$State)
    $clean = New-M2VpsStateObject -From $State
    $json = $clean | ConvertTo-Json -Depth 4
    $path = Get-M2VpsStatePath -ServerRoot $ServerRoot
    $temp = $path + '.tmp'
    [IO.File]::WriteAllText($temp, $json, [Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        # [NullString]::Value, not $null: PowerShell hands a .NET string
        # parameter "" for $null, and File.Replace refuses "" as a backup
        # path - every save after the first threw. A file system that has
        # no Replace (a network share) gets the plain copy.
        try { [IO.File]::Replace($temp, $path, [NullString]::Value) }
        catch {
            [IO.File]::Copy($temp, $path, $true)
            Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
        }
    }
    else { [IO.File]::Move($temp, $path) }
}

# ---------------------------------------------------------------- checks

function Test-M2VpsHostName {
    # An IPv4 address or a DNS name; nothing that could close a quote.
    param([AllowEmptyString()][string]$HostName)
    if (-not $HostName) { return $false }
    if ($HostName -match '^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$') {
        foreach ($i in 1..4) { if ([int]$Matches[$i] -gt 255) { return $false } }
        return $true
    }
    return ($HostName.Length -le 253 -and $HostName -match '^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*$')
}

function Test-M2VpsUserName {
    param([AllowEmptyString()][string]$User)
    return ([bool]$User -and $User -cmatch '^[a-z_][a-z0-9_.-]{0,31}$')
}

function Test-M2VpsRemoteDir {
    # Absolute, two components at least, plain names only: the archive is
    # unpacked there as root, and / or /usr would be a disaster.
    param([AllowEmptyString()][string]$Path)
    if (-not $Path -or $Path -notmatch '^/[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)+$') { return $false }
    foreach ($part in $Path.Split('/')) { if ($part -eq '.' -or $part -eq '..') { return $false } }
    return $true
}

function Test-M2VpsPrivateAddress {
    param([AllowEmptyString()][string]$Address)
    if ($Address -notmatch '^(\d+)\.(\d+)\.\d+\.\d+$') { return $false }
    $a = [int]$Matches[1]; $b = [int]$Matches[2]
    return ($a -eq 10) -or ($a -eq 127) -or ($a -eq 172 -and $b -ge 16 -and $b -le 31) -or ($a -eq 192 -and $b -eq 168) -or ($a -eq 100 -and $b -ge 64 -and $b -le 127)
}

function Assert-M2VpsState {
    param([Parameter(Mandatory = $true)]$State)
    if (-not (Test-M2VpsHostName ([string]$State.host))) { throw 'Podaj adres VPS: IPv4 (np. 203.0.113.7) albo nazwę domeny.' }
    if (-not (Test-M2VpsUserName ([string]$State.user))) { throw 'Podaj użytkownika VPS (małe litery, np. root, debian, ubuntu).' }
    if ([int]$State.port -lt 1 -or [int]$State.port -gt 65535) { throw 'Port SSH: liczba 1-65535 (zwykle 22).' }
    if (-not (Test-M2VpsRemoteDir ([string]$State.remoteDir))) { throw 'Folder na VPS: ścieżka bezwzględna z co najmniej dwóch części, np. /opt/metin2.' }
}

# ---------------------------------------------------------------- processes

function Get-M2VpsTool {
    # ssh, ssh-keygen and tar as Windows 10/11 ship them. A 32-bit PowerShell
    # sees SysWOW64 under System32, where OpenSSH is not: Sysnative is the
    # real System32 from there.
    param([Parameter(Mandatory = $true)][ValidateSet('ssh', 'ssh-keygen', 'tar')][string]$Name)
    # Windows' own first, and for tar only Windows' own: Git for Windows with
    # its Unix tools on PATH puts GNU tar and an MSYS ssh ahead of them, and
    # GNU tar reads "C:\..." as a remote host ("Cannot connect to C: resolve
    # failed") - which is how the upload test failed when run from Git Bash.
    $candidates = @()
    $system = $env:SystemRoot
    if (-not $system) { $system = 'C:\Windows' }
    if ($Name -eq 'tar') { $candidates += @((Join-Path $system 'System32\tar.exe'), (Join-Path $system 'Sysnative\tar.exe')) }
    else {
        $candidates += @((Join-Path $system ('System32\OpenSSH\' + $Name + '.exe')), (Join-Path $system ('Sysnative\OpenSSH\' + $Name + '.exe')))
        $found = Get-Command ($Name + '.exe') -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $candidates += $found.Source }
    }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    if ($Name -eq 'tar') { throw 'Nie ma programu tar.exe (jest w Windows 10 od 2018 i w Windows 11) - zaktualizuj Windows.' }
    throw ('Nie ma programu {0}.exe. Włącz Klienta OpenSSH: Ustawienia > Aplikacje > Funkcje opcjonalne > Dodaj funkcję > Klient OpenSSH.' -f $Name)
}

function ConvertTo-M2VpsArgument {
    # One argument as the C runtime reads it back (CommandLineToArgvW): what
    # ssh.exe, ssh-keygen.exe and tar.exe parse. Windows PowerShell 5.1 does
    # this wrong for an empty string (it drops it) and for an embedded quote
    # (it does not escape it), so these processes are started with a command
    # line built here instead.
    param([AllowEmptyString()][string]$Value)
    if ($Value -eq '') { return '""' }
    if ($Value -notmatch '[\s"]') { return $Value }
    $builder = New-Object Text.StringBuilder
    [void]$builder.Append('"')
    $backslashes = 0
    foreach ($ch in $Value.ToCharArray()) {
        if ($ch -eq '\') { $backslashes++; continue }
        if ($ch -eq '"') {
            [void]$builder.Append(('\' * (2 * $backslashes + 1)) + '"')
        }
        else {
            [void]$builder.Append(('\' * $backslashes) + $ch)
        }
        $backslashes = 0
    }
    [void]$builder.Append(('\' * (2 * $backslashes)) + '"')
    return $builder.ToString()
}

function ConvertTo-M2VpsCommandLine {
    param([string[]]$Arguments = @())
    return ((@($Arguments) | ForEach-Object { ConvertTo-M2VpsArgument ([string]$_) }) -join ' ')
}

function Start-M2VpsProcess {
    # Process.Start with stdin redirected wraps the pipe in a StreamWriter in
    # Console.InputEncoding and flushes it on the spot. With a UTF-8 console
    # (chcp 65001, or Windows' "UTF-8 for worldwide language support") that
    # flush is a byte order mark at the head of the child's stdin before a
    # byte of ours: a gzip stream the VPS's tar refuses, a probe whose first
    # line sh cannot run. For the moment of the start the console's encoding
    # is the same code page without the mark, and then it is put back.
    param([Parameter(Mandatory = $true)][Diagnostics.ProcessStartInfo]$Info)
    $restore = $null
    if ($Info.RedirectStandardInput) {
        try {
            $current = [Console]::InputEncoding
            if ($current.CodePage -eq 65001 -and $current.GetPreamble().Length -gt 0) {
                [Console]::InputEncoding = New-Object Text.UTF8Encoding $false
                $restore = $current
            }
        }
        catch { $restore = $null }
    }
    try { return [Diagnostics.Process]::Start($Info) }
    finally {
        if ($restore) { try { [Console]::InputEncoding = $restore } catch { } }
    }
}

function Invoke-M2VpsProcess {
    # A process with its own command line, stdin written (LF only - it is a
    # shell script for Linux more often than not), stdout and stderr read
    # without the deadlock of reading one pipe while the other fills. With
    # -Stream every stdout line is written out as it comes, which is what the
    # window's action log shows while a remote install runs.
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$Arguments = @(),
        [AllowEmptyString()][string]$InputText = $null,
        [int]$TimeoutSeconds = 0,
        [switch]$Stream
    )
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $FilePath
    $info.Arguments = ConvertTo-M2VpsCommandLine -Arguments $Arguments
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $info.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    $process = Start-M2VpsProcess -Info $info
    try {
        $errorTask = $process.StandardError.ReadToEndAsync()
        if ($null -ne $InputText -and $InputText -ne '') {
            $bytes = [Text.UTF8Encoding]::new($false).GetBytes($InputText.Replace("`r`n", "`n"))
            try {
                $process.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
                $process.StandardInput.BaseStream.Flush()
            }
            catch { }
        }
        try { $process.StandardInput.Close() } catch { }
        $output = ''
        $timedOut = $false
        if ($Stream) {
            $builder = New-Object Text.StringBuilder
            while ($null -ne ($line = $process.StandardOutput.ReadLine())) {
                Write-Host $line
                [void]$builder.AppendLine($line)
            }
            $process.WaitForExit()
            $output = $builder.ToString()
        }
        else {
            $outputTask = $process.StandardOutput.ReadToEndAsync()
            if ($TimeoutSeconds -gt 0) {
                if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
                    $timedOut = $true
                    try { $process.Kill() } catch { }
                    $process.WaitForExit()
                }
            }
            else { $process.WaitForExit() }
            $output = $outputTask.Result
        }
        $errorText = $errorTask.Result
        return [pscustomobject]@{ ExitCode = $(if ($timedOut) { -1 } else { $process.ExitCode }); Output = $output; Error = $errorText; TimedOut = $timedOut }
    }
    finally { $process.Dispose() }
}

# ---------------------------------------------------------------- ssh

function Get-M2VpsSshArguments {
    # Pure: ssh's arguments for this VPS. BatchMode for everything the
    # launcher runs on its own - it never waits for a prompt nobody sees -
    # and only the key it made (IdentitiesOnly), so a player's other keys
    # cannot use up the server's attempts. A first contact takes the server's
    # key (accept-new); a changed one is refused, as it should be.
    param(
        [Parameter(Mandatory = $true)]$State,
        [switch]$BatchMode,
        [switch]$NoKey,
        [string[]]$Forward = @(),
        [switch]$NoCommand,
        [string]$Command = ''
    )
    $arguments = @('-p', [string][int]$State.port)
    if (-not $NoKey) { $arguments += @('-i', [string]$State.keyPath, '-o', 'IdentitiesOnly=yes') }
    $arguments += @('-o', 'StrictHostKeyChecking=accept-new', '-o', 'ConnectTimeout=20',
        '-o', 'ServerAliveInterval=30', '-o', 'ServerAliveCountMax=4', '-o', 'LogLevel=ERROR')
    if ($BatchMode) { $arguments += @('-o', 'BatchMode=yes') }
    foreach ($spec in @($Forward)) { if ($spec) { $arguments += @('-L', $spec) } }
    if (@($Forward).Count -gt 0) { $arguments += @('-o', 'ExitOnForwardFailure=yes') }
    if ($NoCommand) { $arguments += '-N' }
    $arguments += ('{0}@{1}' -f $State.user, $State.host)
    if ($Command) { $arguments += $Command }
    return $arguments
}

function Get-M2VpsRemoteCommand {
    # Pure: a command for the VPS as root - through sudo -n when the user is
    # not root (Debian's and Ubuntu's cloud images give "debian"/"ubuntu" with
    # sudo that asks nothing). -n: never a prompt nobody can answer.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$Command)
    if ([string]$State.user -eq 'root') { return $Command }
    return ('sudo -n ' + $Command)
}

function Get-M2VpsScriptCommand {
    # Pure: vps-install.sh on the VPS with one subcommand and its words.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$Arguments)
    return (Get-M2VpsRemoteCommand -State $State -Command ('sh {0}/{1} {2}' -f $State.remoteDir, $script:VpsScript, $Arguments))
}

function Get-M2VpsSshError {
    # Pure: what ssh's failure means, in words a player can act on. ssh ends
    # with 255 on its own failures; anything else is the remote command's.
    param([AllowEmptyString()][string]$Text = '', [int]$ExitCode = 255, [AllowEmptyString()][string]$HostName = '')
    if ($Text -match 'REMOTE HOST IDENTIFICATION HAS CHANGED|Host key verification failed') {
        return ('Serwer {0} przedstawia inny klucz niż zapamiętany (VPS postawiony od nowa?). Jeśli to ten sam serwer, usuń stary wpis: ssh-keygen -R {0}' -f $HostName)
    }
    if ($Text -match 'Permission denied') { return 'VPS nie wpuszcza klucza launchera - kliknij POŁĄCZ (KLUCZ SSH) i wpisz hasło do VPS.' }
    if ($Text -match 'Could not resolve hostname') { return ('Nie znam adresu {0} - sprawdź pisownię.' -f $HostName) }
    if ($Text -match 'Connection refused') { return ('VPS {0} odrzuca połączenie SSH - sprawdź port (zwykle 22) i czy serwer działa.' -f $HostName) }
    if ($Text -match 'timed out|Connection timed out|Network is unreachable|No route to host') { return ('VPS {0} nie odpowiada - sprawdź adres, internet i zaporę u dostawcy VPS.' -f $HostName) }
    if ($Text -match 'sudo: a password is required|sudo: a terminal is required') {
        return 'Ten użytkownik potrzebuje hasła do sudo. Zaloguj się jako root albo daj mu sudo bez hasła (NOPASSWD).'
    }
    if ($ExitCode -eq 255) {
        $first = (@($Text -split "`r?`n" | Where-Object { $_ }) | Select-Object -First 2) -join ' '
        return ('SSH nie połączył się z VPS: ' + $first)
    }
    return ''
}

function Invoke-M2Vps {
    # A command on the VPS with the launcher's key and no prompts. -Sudo runs
    # it as root. Returns the process result; throws only when ssh itself
    # could not connect, with the reason in words.
    param(
        [Parameter(Mandatory = $true)]$State,
        [Parameter(Mandatory = $true)][string]$Command,
        [switch]$Sudo,
        [AllowEmptyString()][string]$InputText = $null,
        [int]$TimeoutSeconds = 120,
        [switch]$Stream,
        [switch]$NoThrow
    )
    Assert-M2VpsState -State $State
    $remote = $(if ($Sudo) { Get-M2VpsRemoteCommand -State $State -Command $Command } else { $Command })
    $arguments = Get-M2VpsSshArguments -State $State -BatchMode -Command $remote
    $result = Invoke-M2VpsProcess -FilePath (Get-M2VpsTool 'ssh') -Arguments $arguments -InputText $InputText -TimeoutSeconds $TimeoutSeconds -Stream:$Stream
    if (-not $NoThrow) {
        if ($result.TimedOut) { throw ('VPS nie odpowiedział w {0} s.' -f $TimeoutSeconds) }
        if ($result.ExitCode -eq 255) { throw (Get-M2VpsSshError -Text ([string]$result.Error) -ExitCode 255 -HostName ([string]$State.host)) }
    }
    return $result
}

function New-M2VpsKey {
    # The launcher's own key, made once. The .ssh folder of the profile is
    # the user's alone, which is what ssh wants of a private key.
    param([string]$KeyPath = (Get-M2VpsDefaultKeyPath))
    if (-not (Test-Path -LiteralPath $KeyPath -PathType Leaf) -or -not (Test-Path -LiteralPath ($KeyPath + '.pub') -PathType Leaf)) {
        $folder = Split-Path -Parent $KeyPath
        if (-not (Test-Path -LiteralPath $folder -PathType Container)) { New-Item -ItemType Directory -Path $folder -Force | Out-Null }
        foreach ($stale in @($KeyPath, ($KeyPath + '.pub'))) { if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale -Force } }
        $computer = ($env:COMPUTERNAME -replace '[^A-Za-z0-9._-]', '')
        if (-not $computer) { $computer = 'pc' }
        $result = Invoke-M2VpsProcess -FilePath (Get-M2VpsTool 'ssh-keygen') -TimeoutSeconds 60 `
            -Arguments @('-q', '-t', 'ed25519', '-N', '', '-C', ('metin2-launcher@' + $computer), '-f', $KeyPath)
        if ($result.ExitCode -ne 0 -or -not (Test-Path -LiteralPath ($KeyPath + '.pub') -PathType Leaf)) {
            throw ('ssh-keygen nie utworzył klucza: ' + ([string]$result.Error).Trim())
        }
    }
    $public = ([IO.File]::ReadAllText($KeyPath + '.pub')).Trim()
    if ($public -notmatch '^ssh-ed25519 [A-Za-z0-9+/=]+( [A-Za-z0-9@._-]+)?$') { throw ('Klucz publiczny {0}.pub ma nieoczekiwaną postać.' -f $KeyPath) }
    return $public
}

function Get-M2VpsKeyInstallScript {
    # Pure: the .cmd the key is installed by. The one step that asks for the
    # VPS password: in a visible console, where ssh asks for it itself - the
    # launcher never sees it. The remote side appends the key once (grep -x)
    # under umask 077, so ~/.ssh and authorized_keys are the user's alone.
    # Batch doubles % and reads nothing special between double quotes, and the
    # key and the names are checked plain text, so the line needs no escaping.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$PublicKey, [Parameter(Mandatory = $true)][string]$SshPath)
    $remote = "umask 077; mkdir -p ~/.ssh && touch ~/.ssh/authorized_keys && (grep -qxF '{0}' ~/.ssh/authorized_keys || printf '%%s\n' '{0}' >> ~/.ssh/authorized_keys) && echo KLUCZ-DODANY" -f $PublicKey
    $lines = @(
        '@echo off',
        'title Metin2 SinglePlayer - klucz SSH dla VPS',
        'echo.',
        ('echo  Lacze sie z VPS {0} jako {1}.' -f $State.host, $State.user),
        'echo  Wpisz haslo do VPS (znakow nie widac) i nacisnij Enter.',
        'echo  To jedyny raz: potem launcher laczy sie kluczem, bez hasla.',
        'echo.',
        ('"{0}" -p {1} -o StrictHostKeyChecking=accept-new -o ConnectTimeout=20 {2}@{3} "{4}"' -f $SshPath, [int]$State.port, $State.user, $State.host, $remote),
        'if errorlevel 1 goto failed',
        'echo.',
        'echo  Klucz dodany. To okno zamknie sie samo.',
        'timeout /t 3 >nul',
        'exit /b 0',
        ':failed',
        'echo.',
        'echo  Nie udalo sie - sprawdz adres, uzytkownika i haslo, i sprobuj jeszcze raz.',
        'pause',
        'exit /b 1'
    )
    return (($lines -join "`r`n") + "`r`n")
}

function Install-M2VpsKey {
    # Opens the window, waits for it, and answers whether the key works now.
    param([Parameter(Mandatory = $true)]$State)
    Assert-M2VpsState -State $State
    $public = New-M2VpsKey -KeyPath ([string]$State.keyPath)
    $script = Get-M2VpsKeyInstallScript -State $State -PublicKey $public -SshPath (Get-M2VpsTool 'ssh')
    $path = Join-Path ([IO.Path]::GetTempPath()) ('m2-vps-key-{0}.cmd' -f [Guid]::NewGuid().ToString('N').Substring(0, 8))
    [IO.File]::WriteAllText($path, $script, [Text.Encoding]::ASCII)
    try {
        $process = Start-Process -FilePath $path -Wait -PassThru
        [void]$process
    }
    finally { Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue }
    $check = Invoke-M2Vps -State $State -Command 'echo m2-key-ok' -TimeoutSeconds 40 -NoThrow
    return ($check.ExitCode -eq 0 -and ([string]$check.Output) -match 'm2-key-ok')
}

# ---------------------------------------------------------------- machine

function Get-M2VpsProbeScript {
    # Pure: what the launcher asks the VPS before anything is uploaded. Plain
    # sh, sent on stdin (sh -s) so nothing in it is quoted through ssh.
    param([string]$RemoteDir = $script:VpsDefaultRemoteDir)
    return (@(
        'echo probe=1',
        'echo "arch=$(uname -m 2>/dev/null)"',
        'if [ -r /etc/os-release ]; then . /etc/os-release; fi',
        'echo "os_id=${ID:-}"',
        'echo "os_like=${ID_LIKE:-}"',
        'echo "os_version=${VERSION_ID:-}"',
        'echo "os_name=${PRETTY_NAME:-}"',
        'awk ''/^MemTotal:/ { print "mem_kb=" $2 } /^SwapTotal:/ { print "swap_kb=" $2 }'' /proc/meminfo 2>/dev/null',
        ('d=' + $RemoteDir + '; while [ ! -d "$d" ]; do d=$(dirname "$d"); done'),
        'echo "disk_kb=$(df -Pk "$d" 2>/dev/null | awk ''NR == 2 { print $4 }'')"',
        'echo "uid=$(id -u)"',
        'if [ "$(id -u)" = 0 ]; then echo sudo=root; elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then echo sudo=nopasswd; else echo sudo=no; fi',
        'if command -v docker >/dev/null 2>&1; then echo docker=1; else echo docker=0; fi',
        'if command -v tar >/dev/null 2>&1; then echo tar=1; else echo tar=0; fi',
        'echo "virt=$(systemd-detect-virt 2>/dev/null)"',
        ('if [ -f ' + $RemoteDir + '/VERSION ]; then echo "installed=$(tr -d '' \r\n'' < ' + $RemoteDir + '/VERSION)"; fi'),
        ('if [ -f ' + $RemoteDir + '/' + $script:VpsScript + ' ]; then echo script=1; else echo script=0; fi')
    ) -join "`n") + "`n"
}

function ConvertFrom-M2VpsKeyValue {
    # Pure: key=value lines up to the first line starting with ---.
    param([AllowEmptyString()][string]$Text = '')
    $values = [ordered]@{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ($line.StartsWith('---')) { break }
        if ($line -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') { $values[$Matches[1]] = $Matches[2].Trim() }
    }
    return $values
}

function Get-M2VpsKbAsMB {
    param($Values, [string]$Key)
    $number = [long]0
    if ($Values.Contains($Key) -and [long]::TryParse([string]$Values[$Key], [ref]$number)) { return [int]($number / 1024) }
    return -1
}

function ConvertFrom-M2VpsProbe {
    # Pure: the probe's answer as an object. A size it could not read is -1.
    param([AllowEmptyString()][string]$Text = '')
    $v = ConvertFrom-M2VpsKeyValue -Text $Text
    $get = { param($key) if ($v.Contains($key)) { [string]$v[$key] } else { '' } }
    return [pscustomobject]@{
        Answered  = ((& $get 'probe') -eq '1')
        Arch      = (& $get 'arch')
        OsId      = (& $get 'os_id')
        OsLike    = (& $get 'os_like')
        OsVersion = (& $get 'os_version')
        OsName    = (& $get 'os_name')
        MemMB     = (Get-M2VpsKbAsMB $v 'mem_kb')
        SwapMB    = (Get-M2VpsKbAsMB $v 'swap_kb')
        DiskMB    = (Get-M2VpsKbAsMB $v 'disk_kb')
        Sudo      = (& $get 'sudo')
        Docker    = ((& $get 'docker') -eq '1')
        Tar       = ((& $get 'tar') -eq '1')
        Virt      = (& $get 'virt')
        Installed = (& $get 'installed')
        Script    = ((& $get 'script') -eq '1')
    }
}

function Get-M2VpsBotCount {
    # The same numbers vps-install.sh writes into .env.
    param([int]$MemMB)
    if ($MemMB -lt 6000) { return 150 }
    if ($MemMB -lt 14000) { return 400 }
    return 800
}

function Get-M2VpsMachineVerdict {
    # Pure: what stops an install and what only deserves a word. The same
    # lines vps-install.sh draws, so the launcher does not upload 300 MB to a
    # machine the script is going to refuse.
    param([Parameter(Mandatory = $true)]$Probe, [string]$User = 'root')
    $blocking = @()
    $warnings = @()
    if (-not $Probe.Answered) { $blocking += 'VPS nie odpowiedział na sprawdzenie (to nie jest zwykły Linux z sh?).' }
    if ($Probe.Arch -and @('x86_64', 'amd64') -notcontains $Probe.Arch) {
        $blocking += ('Procesor {0}: rdzeń gry jest budowany jako 32-bitowy x86 i na ARM nie ruszy. Potrzebny VPS z procesorem Intel/AMD (x86_64).' -f $Probe.Arch)
    }
    if (@('root', 'nopasswd') -notcontains $Probe.Sudo) {
        $blocking += ('Użytkownik {0} nie jest rootem i nie ma sudo bez hasła - zaloguj się jako root albo daj mu sudo z NOPASSWD.' -f $User)
    }
    if (-not $Probe.Tar) { $blocking += 'Na VPS nie ma programu tar.' }
    if ($Probe.DiskMB -ge 0 -and $Probe.DiskMB -lt $script:VpsMinDiskMB) {
        $blocking += ('Na dysku VPS jest wolne {0} MB - obrazy i pierwsza budowa potrzebują co najmniej {1} MB (zalecane 60-80 GB).' -f $Probe.DiskMB, $script:VpsMinDiskMB)
    }
    elseif ($Probe.DiskMB -ge 0 -and $Probe.DiskMB -lt $script:VpsWarnDiskMB) {
        $warnings += ('Wolne {0} MB na dysku to mało - zalecane 60-80 GB.' -f $Probe.DiskMB)
    }
    $supported = @('debian:12', 'debian:13', 'ubuntu:22.04', 'ubuntu:24.04') -contains ('{0}:{1}' -f $Probe.OsId, $Probe.OsVersion)
    $aptFamily = (' {0} {1} ' -f $Probe.OsId, $Probe.OsLike) -match ' (debian|ubuntu) '
    if (-not $supported) {
        if (-not $aptFamily -and -not $Probe.Docker) {
            $blocking += ('System {0} nie jest Debianem ani Ubuntu i nie ma Dockera - zainstaluj Docker sam albo weź Debian 12/13 lub Ubuntu 22.04/24.04.' -f $Probe.OsName)
        }
        else { $warnings += ('System {0} nie był testowany (sprawdzone: Debian 12/13, Ubuntu 22.04/24.04).' -f $Probe.OsName) }
    }
    if ($Probe.MemMB -ge 0 -and $Probe.MemMB -lt $script:VpsWarnMemMB) {
        if ($Probe.MemMB -lt $script:VpsMinMemMB -and $Probe.SwapMB -le 0) {
            $warnings += ('Pamięci jest {0} MB - instalator założy plik wymiany 4 GB, a jeśli system na to nie pozwoli, odmówi (potrzebne co najmniej 4 GB, zalecane 8 GB).' -f $Probe.MemMB)
        }
        else { $warnings += ('Pamięci jest {0} MB - zalecane 8 GB; przy 4 GB mniej botów (instalator ustawi {1}).' -f $Probe.MemMB, (Get-M2VpsBotCount -MemMB $Probe.MemMB)) }
    }
    if (@('openvz', 'lxc', 'lxc-libvirt') -contains $Probe.Virt) {
        $warnings += ('VPS jest kontenerem ({0}) - Docker może w nim nie działać; najlepszy jest VPS KVM.' -f $Probe.Virt)
    }
    $bots = $(if ($Probe.MemMB -ge 0) { Get-M2VpsBotCount -MemMB $Probe.MemMB } else { 0 })
    return [pscustomobject]@{ Ok = ($blocking.Count -eq 0); Blocking = $blocking; Warnings = $warnings; Bots = $bots }
}

function Test-M2VpsMachine {
    param([Parameter(Mandatory = $true)]$State)
    $result = Invoke-M2Vps -State $State -Command 'sh -s' -InputText (Get-M2VpsProbeScript -RemoteDir ([string]$State.remoteDir)) -TimeoutSeconds 60
    $probe = ConvertFrom-M2VpsProbe -Text ([string]$result.Output)
    return [pscustomobject]@{ Probe = $probe; Verdict = (Get-M2VpsMachineVerdict -Probe $probe -User ([string]$State.user)) }
}

function Format-M2VpsMachineReport {
    # Pure: the check in lines, for the console and the window alike.
    param([Parameter(Mandatory = $true)]$Machine)
    $p = $Machine.Probe
    $lines = @()
    $lines += ('System: {0} ({1})' -f $(if ($p.OsName) { $p.OsName } else { 'nieznany' }), $p.Arch)
    $lines += ('Pamięć: {0} MB, plik wymiany: {1} MB, wolne na dysku: {2} MB' -f $p.MemMB, $p.SwapMB, $p.DiskMB)
    $lines += ('Uprawnienia: {0}, Docker: {1}' -f $(switch ($p.Sudo) { 'root' { 'root' } 'nopasswd' { 'sudo bez hasła' } default { 'BRAK' } }), $(if ($p.Docker) { 'jest' } else { 'brak (instalator go zainstaluje)' }))
    if ($p.Installed) { $lines += ('Na VPS jest już serwer w wersji {0}.' -f $p.Installed) }
    foreach ($b in @($Machine.Verdict.Blocking)) { $lines += ('BŁĄD: ' + $b) }
    foreach ($w in @($Machine.Verdict.Warnings)) { $lines += ('UWAGA: ' + $w) }
    if ($Machine.Verdict.Ok) { $lines += ('Można instalować. Botów na start: {0}.' -f $Machine.Verdict.Bots) }
    return $lines
}

# ---------------------------------------------------------------- upload

function Test-M2VpsUploadExcluded {
    # Pure: a top-level entry of the server folder that does not travel. The
    # database's and the panels' passwords (.env and every copy the launcher
    # leaves of it), the launcher's own state - .m2coop.json carries the COOP
    # friends' passwords - the backups, the logs, the support bundles and the
    # client. bsdtar's --exclude matches at any depth, so the top level is
    # decided here, where a name means the one entry.
    param([Parameter(Mandatory = $true)][string]$Name)
    $lower = $Name.ToLowerInvariant()
    if ($lower -like '.env*' -and $lower -ne '.env.example') { return $true }
    if ($lower -like '.m2*') { return $true }
    if ($lower -like '.vps-install*') { return $true }
    if (@('.git', '.vs', '.vscode', 'backups', 'launcher-logs', 'logs', 'support-bundles', 'klient', 'client',
            'client-archive', 'thumbs.db', 'desktop.ini') -contains $lower) { return $true }
    if ($lower -like '*.log' -or $lower -like 'metin2-support-*.zip') { return $true }
    return $false
}

function Get-M2VpsNestedExcludes {
    # Pure: what bsdtar leaves out below the top level: .env and its copies
    # wherever they are (the pattern keeps .env.example), and a client archive.
    return @('.env', '.env.[!e]*', 'client-archive')
}

function Get-M2VpsUploadEntries {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    foreach ($must in @('VERSION', 'linux-port\docker\docker-compose.yml', 'linux-port\docker\.env.example', ($script:VpsScript.Replace('/', '\')))) {
        if (-not (Test-Path -LiteralPath (Join-Path $ServerRoot $must) -PathType Leaf)) {
            throw ('W folderze serwera nie ma {0} - ta instalacja nie nadaje się na VPS (zaktualizuj serwer do wersji z opcją VPS).' -f $must)
        }
    }
    return @(Get-ChildItem -LiteralPath $ServerRoot -Force | Where-Object { -not (Test-M2VpsUploadExcluded -Name $_.Name) } |
        Sort-Object Name | ForEach-Object { $_.Name })
}

function Get-M2VpsTarArguments {
    # Pure: bsdtar's arguments - gzip to stdout, the nested exclusions, then
    # the chosen top-level entries from the server folder.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string[]]$Entries)
    $arguments = @('-c', '-z', '-f', '-')
    foreach ($pattern in (Get-M2VpsNestedExcludes)) { $arguments += @('--exclude', $pattern) }
    $arguments += @('-C', $ServerRoot)
    $arguments += @($Entries)
    return $arguments
}

function Get-M2VpsUnpackCommand {
    # Pure: the receiving end. bsdtar on Windows stores every file as 0666 and
    # every folder as 0777, and GNU tar running as root keeps what the archive
    # says - a server folder anybody on the VPS could write, scripts that run
    # as root included. --no-same-permissions under umask 022 makes them 0644
    # and 0755; --no-same-owner makes them root's.
    param([Parameter(Mandatory = $true)]$State)
    $dir = [string]$State.remoteDir
    return (Get-M2VpsRemoteCommand -State $State -Command ("sh -c 'umask 022; mkdir -p {0} && tar -xzf - --no-same-owner --no-same-permissions -C {0}'" -f $dir))
}

function Send-M2VpsServer {
    # tar.exe's stdout copied into ssh's stdin by this process, block by
    # block: a PowerShell pipeline between two programs is text, and would
    # break the archive.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$ServerRoot)
    Assert-M2VpsState -State $State
    $entries = Get-M2VpsUploadEntries -ServerRoot $ServerRoot
    Write-Host ('Wysyłam folder serwera na VPS ({0}): {1}' -f $State.remoteDir, ($entries -join ', '))
    $tarInfo = New-Object Diagnostics.ProcessStartInfo
    $tarInfo.FileName = Get-M2VpsTool 'tar'
    $tarInfo.Arguments = ConvertTo-M2VpsCommandLine -Arguments (Get-M2VpsTarArguments -ServerRoot $ServerRoot -Entries $entries)
    $tarInfo.UseShellExecute = $false; $tarInfo.CreateNoWindow = $true
    $tarInfo.RedirectStandardOutput = $true; $tarInfo.RedirectStandardError = $true
    $sshInfo = New-Object Diagnostics.ProcessStartInfo
    $sshInfo.FileName = Get-M2VpsTool 'ssh'
    $sshInfo.Arguments = ConvertTo-M2VpsCommandLine -Arguments (Get-M2VpsSshArguments -State $State -BatchMode -Command (Get-M2VpsUnpackCommand -State $State))
    $sshInfo.UseShellExecute = $false; $sshInfo.CreateNoWindow = $true
    $sshInfo.RedirectStandardInput = $true; $sshInfo.RedirectStandardOutput = $true; $sshInfo.RedirectStandardError = $true
    $ssh = Start-M2VpsProcess -Info $sshInfo
    $tar = $null
    try {
        $sshOut = $ssh.StandardOutput.ReadToEndAsync()
        $sshErr = $ssh.StandardError.ReadToEndAsync()
        $tar = [Diagnostics.Process]::Start($tarInfo)
        $tarErr = $tar.StandardError.ReadToEndAsync()
        $source = $tar.StandardOutput.BaseStream
        $target = $ssh.StandardInput.BaseStream
        $buffer = New-Object byte[] 65536
        $sent = [long]0
        $nextReport = [long]10MB
        $broken = ''
        while (($read = $source.Read($buffer, 0, $buffer.Length)) -gt 0) {
            try { $target.Write($buffer, 0, $read) }
            catch { $broken = $_.Exception.Message; break }
            $sent += $read
            if ($sent -ge $nextReport) {
                Write-Host ('  wysłano {0} MB' -f [Math]::Round($sent / 1MB))
                $nextReport += 10MB
            }
        }
        try { $target.Flush(); $ssh.StandardInput.Close() } catch { }
        if ($broken) { try { $tar.Kill() } catch { } }
        $tar.WaitForExit()
        $ssh.WaitForExit()
        $tarText = [string]$tarErr.Result
        $sshText = ([string]$sshErr.Result) + ([string]$sshOut.Result)
        if ($ssh.ExitCode -eq 255) { throw (Get-M2VpsSshError -Text $sshText -ExitCode 255 -HostName ([string]$State.host)) }
        if ($ssh.ExitCode -ne 0) { throw ('Rozpakowanie na VPS nie powiodło się (kod {0}): {1}' -f $ssh.ExitCode, $sshText.Trim()) }
        if ($broken) { throw ('Połączenie z VPS zerwało się w trakcie wysyłania: ' + $broken) }
        # bsdtar ends with 1 when a file could not be read (open in another
        # program, say) and still writes the rest; that is a warning here.
        if ($tar.ExitCode -ne 0) { Write-Host ('UWAGA: tar zgłosił problem (kod {0}): {1}' -f $tar.ExitCode, $tarText.Trim()) -ForegroundColor Yellow }
        Write-Host ('Wysłano {0} MB (spakowane).' -f [Math]::Round($sent / 1MB, 1)) -ForegroundColor Green
        return $sent
    }
    finally {
        if ($tar) { $tar.Dispose() }
        $ssh.Dispose()
    }
}

# ---------------------------------------------------------------- status

function ConvertFrom-M2VpsStatus {
    # Pure: `vps-install.sh status' as an object. LogText is what came after
    # "--- log ---", PsText what came after "--- docker compose ps ---".
    param([AllowEmptyString()][string]$Text = '')
    $v = ConvertFrom-M2VpsKeyValue -Text $Text
    $get = { param($key, $default = '') if ($v.Contains($key) -and [string]$v[$key]) { [string]$v[$key] } else { $default } }
    $number = { param($key, $default) $n = 0; if ([int]::TryParse((& $get $key ''), [ref]$n)) { $n } else { $default } }
    $section = ''
    $log = New-Object Text.StringBuilder
    $ps = New-Object Text.StringBuilder
    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -eq '--- log ---') { $section = 'log'; continue }
        if ($line -eq '--- docker compose ps ---') { $section = 'ps'; continue }
        if ($section -eq 'log') { [void]$log.AppendLine($line) }
        elseif ($section -eq 'ps') { [void]$ps.AppendLine($line) }
    }
    return [pscustomobject]@{
        State = (& $get 'state' 'unknown'); Kind = (& $get 'kind'); Phase = (& $get 'phase'); Message = (& $get 'message')
        Started = (& $get 'started'); Finished = (& $get 'finished'); Exit = (& $get 'exit')
        LogLines = (& $number 'log_lines' 0); Version = (& $get 'version')
        PublicAddress = (& $get 'public_address'); AuthPort = (& $number 'auth_port' 11000)
        GamePortRange = (& $get 'game_port_range' '13000-13002'); Ch2 = ((& $get 'ch2') -eq '1')
        PanelPort = (& $number 'panel_port' 7788); SebanPanelPort = (& $number 'seban_panel_port' 7790)
        ItemShopPort = (& $number 'itemshop_port' 7791); PanelBind = (& $get 'panel_bind'); HostBind = (& $get 'host_bind')
        Bots = (& $get 'bots'); AccountsFile = ((& $get 'accounts_file') -eq '1')
        LogText = $log.ToString().TrimEnd(); PsText = $ps.ToString().TrimEnd()
    }
}

function Get-M2VpsStatus {
    param([Parameter(Mandatory = $true)]$State, [int]$From = -1)
    $arguments = $(if ($From -ge 0) { 'status --from {0}' -f $From } else { 'status' })
    $result = Invoke-M2Vps -State $State -Command (Get-M2VpsScriptCommand -State $State -Arguments $arguments) -TimeoutSeconds 60
    if ($result.ExitCode -ne 0) {
        $why = ([string]$result.Error).Trim()
        if ($why -match 'No such file|nie ma|can.t open') { throw 'Na VPS nie ma jeszcze serwera z tego launchera - najpierw ZAINSTALUJ NA VPS.' }
        throw ('VPS nie podał stanu: ' + $why)
    }
    return (ConvertFrom-M2VpsStatus -Text ([string]$result.Output))
}

function Compare-M2VpsVersion {
    # Pure: -1, 0 or 1 for two versions like 2.2.12; 0 when either is not one.
    param([AllowEmptyString()][string]$Left, [AllowEmptyString()][string]$Right)
    $a = $null; $b = $null
    if (-not [Version]::TryParse(([string]$Left).Trim(), [ref]$a) -or -not [Version]::TryParse(([string]$Right).Trim(), [ref]$b)) { return 0 }
    return $a.CompareTo($b)
}

function Wait-M2VpsJob {
    # The build on the VPS, read every few seconds: the new lines of its log
    # as they come, until it is done, failed or interrupted. A connection that
    # drops is tried again - the build does not depend on it.
    param([Parameter(Mandatory = $true)]$State, [int]$TimeoutMinutes = 120, [int]$PollSeconds = 10, [int]$From = -1)
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $failures = 0
    $status = $null
    $seen = $From
    while ((Get-Date) -lt $deadline) {
        try {
            $status = Get-M2VpsStatus -State $State -From $(if ($seen -ge 0) { $seen } else { 0 })
            $failures = 0
        }
        catch {
            $failures++
            Write-Host ('Połączenie z VPS nie wyszło ({0}) - budowa na VPS trwa dalej, próbuję znowu.' -f $_.Exception.Message) -ForegroundColor Yellow
            if ($failures -ge 30) { throw 'VPS nie odpowiada od kilku minut. Budowa mogła się dokończyć - sprawdź STAN VPS za chwilę.' }
            Start-Sleep -Seconds $PollSeconds
            continue
        }
        if ($status.LogText) { foreach ($line in ($status.LogText -split "`r?`n")) { Write-Host ('  VPS: ' + $line) } }
        $seen = $status.LogLines
        if (@('done', 'failed', 'interrupted', 'none') -contains $status.State) { return $status }
        Start-Sleep -Seconds $PollSeconds
    }
    throw ('Budowa na VPS trwa dłużej niż {0} minut - sprawdź STAN VPS później.' -f $TimeoutMinutes)
}

function Get-M2VpsInstallAddressArgument {
    # Pure: --address for vps-install.sh when the host the player typed is
    # what players will use - a public IPv4 or a domain. A private address
    # (a box at home, a VPN) is left for the script to read the public one.
    param([Parameter(Mandatory = $true)][string]$HostName)
    if (-not (Test-M2VpsHostName $HostName)) { return '' }
    if (Test-M2VpsPrivateAddress $HostName) { return '' }
    return (' --address ' + $HostName)
}

function Install-M2Vps {
    # The whole install: the machine checked, the folder uploaded, the script
    # started, and its build read until it ends. The passwords it makes are
    # not printed here: the output of an action goes to launcher-logs, which
    # support bundles carry (Get-M2VpsAccounts shows them in the window).
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$ServerRoot)
    Assert-M2VpsState -State $State
    Write-Host ('VPS: {0}@{1}:{2}, folder {3}' -f $State.user, $State.host, $State.port, $State.remoteDir)
    $machine = Test-M2VpsMachine -State $State
    foreach ($line in (Format-M2VpsMachineReport -Machine $machine)) { Write-Host $line }
    if (-not $machine.Verdict.Ok) { throw 'VPS nie spełnia wymagań - szczegóły wyżej. Nic nie zostało wysłane.' }
    $local = ''
    $versionFile = Join-Path $ServerRoot 'VERSION'
    if (Test-Path -LiteralPath $versionFile -PathType Leaf) { $local = ([IO.File]::ReadAllText($versionFile)).Trim() }
    # The VPS's log keeps every run; this one's lines start after what is
    # there now.
    $fromLine = 0
    if ($machine.Probe.Script) {
        $status = Get-M2VpsStatus -State $State
        if ($status.State -eq 'running') {
            Write-Host ('Na VPS trwa już zadanie "{0}" - nie wysyłam plików, czekam na jego koniec.' -f $status.Kind) -ForegroundColor Yellow
            $final = Wait-M2VpsJob -State $State -From $status.LogLines
            return $final
        }
        $fromLine = [int]$status.LogLines
    }
    if ($machine.Probe.Installed -and (Compare-M2VpsVersion $machine.Probe.Installed $local) -gt 0) {
        throw ('Na VPS jest nowsza wersja ({0}) niż ta instalacja ({1}) - wysłanie cofnęłoby serwer. Użyj AKTUALIZUJ VPS albo najpierw zaktualizuj ten launcher.' -f $machine.Probe.Installed, $local)
    }
    [void](Send-M2VpsServer -State $State -ServerRoot $ServerRoot)
    Write-Host 'Uruchamiam instalator na VPS (Docker, plik wymiany, .env, budowa w tle)...'
    $arguments = 'install --no-follow' + (Get-M2VpsInstallAddressArgument -HostName ([string]$State.host))
    $start = Invoke-M2Vps -State $State -Command (Get-M2VpsScriptCommand -State $State -Arguments $arguments) -Stream -TimeoutSeconds 0
    if ($start.ExitCode -ne 0) {
        $why = ([string]$start.Error).Trim()
        throw ('Instalator na VPS przerwał (kod {0}){1}' -f $start.ExitCode, $(if ($why) { ': ' + $why } else { ' - powód jest wyżej.' }))
    }
    Write-Host 'Budowa trwa na VPS (pierwszy raz 15-40 minut). Postęp poniżej; zamknięcie launchera jej nie przerywa.'
    $final = Wait-M2VpsJob -State $State -From $fromLine
    $State.lastInstall = (Get-Date).ToString('s')
    $State.lastVersion = [string]$final.Version
    Save-M2VpsState -ServerRoot $ServerRoot -State $State
    return $final
}

function Update-M2Vps {
    # update.sh on the VPS, the package from GitHub, in the background.
    param([Parameter(Mandatory = $true)]$State, [string]$ServerRoot = '')
    $status = Get-M2VpsStatus -State $State
    if ($status.State -eq 'running') {
        Write-Host ('Na VPS trwa już zadanie "{0}" - czekam na jego koniec.' -f $status.Kind) -ForegroundColor Yellow
        return (Wait-M2VpsJob -State $State -From $status.LogLines)
    }
    Write-Host ('Na VPS jest wersja {0}. Uruchamiam aktualizację (linux-port/tools/update.sh) w tle...' -f $status.Version)
    $start = Invoke-M2Vps -State $State -Command (Get-M2VpsScriptCommand -State $State -Arguments 'update --no-follow') -Stream -TimeoutSeconds 0
    if ($start.ExitCode -ne 0) { throw ('Aktualizacja na VPS nie wystartowała (kod {0}): {1}' -f $start.ExitCode, ([string]$start.Error).Trim()) }
    $final = Wait-M2VpsJob -State $State -From $status.LogLines
    if ($ServerRoot) {
        $State.lastVersion = [string]$final.Version
        Save-M2VpsState -ServerRoot $ServerRoot -State $State
    }
    return $final
}

# ---------------------------------------------------------------- panels

function Test-M2VpsLocalPortFree {
    param([Parameter(Mandatory = $true)][int]$Port)
    $listener = $null
    try {
        $listener = New-Object Net.Sockets.TcpListener ([Net.IPAddress]::Loopback, $Port)
        $listener.Start()
        return $true
    }
    catch { return $false }
    finally { if ($listener) { try { $listener.Stop() } catch { } } }
}

function Get-M2VpsTunnelPlan {
    # Pure but for the probe: a local port for every remote panel port - the
    # same number where it is free, which keeps the panels' links to each
    # other (they name 127.0.0.1:7788), else ten or twenty thousand higher,
    # where a server running on this PC already holds 7788.
    param([Parameter(Mandatory = $true)][int[]]$RemotePorts, [Parameter(Mandatory = $true)][scriptblock]$IsFree)
    $taken = @()
    $plan = @()
    foreach ($remote in $RemotePorts) {
        $chosen = 0
        foreach ($candidate in @($remote, ($remote + 10000), ($remote + 20000))) {
            if ($candidate -gt 65535 -or $taken -contains $candidate) { continue }
            if (& $IsFree $candidate) { $chosen = $candidate; break }
        }
        if ($chosen -eq 0) { throw ('Nie ma wolnego portu na tym komputerze dla panelu VPS {0}.' -f $remote) }
        $taken += $chosen
        $plan += [pscustomobject]@{ remote = $remote; local = $chosen }
    }
    return $plan
}

function Get-M2VpsTunnelProcess {
    # The tunnel this launcher started, if it still runs: the saved pid, and
    # an ssh.exe whose command line names this host - a pid reused by
    # something else after a restart is not taken for it.
    param([Parameter(Mandatory = $true)]$State)
    $tunnelPid = [int]$State.tunnelPid
    if ($tunnelPid -le 0) { return $null }
    $process = Get-Process -Id $tunnelPid -ErrorAction SilentlyContinue
    if (-not $process -or $process.ProcessName -ne 'ssh') { return $null }
    try {
        $wmi = Get-CimInstance Win32_Process -Filter ('ProcessId={0}' -f $tunnelPid) -ErrorAction Stop
        $line = [string]$wmi.CommandLine
        if ($line -and ($line -notmatch [regex]::Escape([string]$State.host) -or $line -notmatch ' -N ')) { return $null }
    }
    catch { }
    return $process
}

function Get-M2VpsPanelAddresses {
    # Pure: the panels' addresses on this PC through a tunnel plan.
    param([Parameter(Mandatory = $true)][object[]]$Plan, [int]$PanelPort = 7788, [int]$SebanPort = 7790, [int]$ItemShopPort = 7791)
    $local = @{}
    foreach ($pair in $Plan) { $local[[int]$pair.remote] = [int]$pair.local }
    $classic = $(if ($local.ContainsKey($PanelPort)) { $local[$PanelPort] } else { $PanelPort })
    $seban = $(if ($local.ContainsKey($SebanPort)) { $local[$SebanPort] } else { $SebanPort })
    $shop = $(if ($local.ContainsKey($ItemShopPort)) { $local[$ItemShopPort] } else { $ItemShopPort })
    return [pscustomobject]@{
        ClassicUrl = ('http://127.0.0.1:{0}/map' -f $classic); SebanUrl = ('http://127.0.0.1:{0}/' -f $seban)
        ItemShopUrl = ('http://127.0.0.1:{0}/' -f $shop); ClassicPort = $classic; SebanPort = $seban; ItemShopPort = $shop
    }
}

function Close-M2VpsPanel {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $state = Get-M2VpsState -ServerRoot $ServerRoot
    $process = Get-M2VpsTunnelProcess -State $state
    $closed = $false
    if ($process) { try { $process.Kill(); $closed = $true } catch { } }
    $state.tunnelPid = 0
    $state.tunnelPorts = @()
    Save-M2VpsState -ServerRoot $ServerRoot -State $state
    return $closed
}

function Open-M2VpsPanel {
    # ssh -N -L for the three panels, hidden, and outliving the launcher: the
    # browser keeps using it after the window is closed. It ends when the
    # connection does (ServerAlive), or by Close-M2VpsPanel.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$ServerRoot, [int]$WaitSeconds = 25)
    Assert-M2VpsState -State $State
    $remotePorts = @(7788, 7790, 7791)
    try {
        $status = Get-M2VpsStatus -State $State
        $remotePorts = @([int]$status.PanelPort, [int]$status.SebanPanelPort, [int]$status.ItemShopPort)
    }
    catch { $status = $null }
    [void](Close-M2VpsPanel -ServerRoot $ServerRoot)
    $plan = Get-M2VpsTunnelPlan -RemotePorts $remotePorts -IsFree { param($port) Test-M2VpsLocalPortFree -Port $port }
    $forwards = @($plan | ForEach-Object { '{0}:127.0.0.1:{1}' -f $_.local, $_.remote })
    $line = ConvertTo-M2VpsCommandLine -Arguments (Get-M2VpsSshArguments -State $State -BatchMode -NoCommand -Forward $forwards)
    $process = Start-Process -FilePath (Get-M2VpsTool 'ssh') -ArgumentList $line -WindowStyle Hidden -PassThru
    try { $null = $process.Handle } catch { }
    $first = [int]$plan[0].local
    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    $ready = $false
    while ((Get-Date) -lt $deadline) {
        if ($process.HasExited) { break }
        if (-not (Test-M2VpsLocalPortFree -Port $first)) { $ready = $true; break }
        Start-Sleep -Milliseconds 400
    }
    if (-not $ready) {
        if (-not $process.HasExited) { try { $process.Kill() } catch { } }
        throw 'Tunel do paneli nie wstał - sprawdź połączenie (SPRAWDŹ VPS) i czy klucz działa.'
    }
    $saved = Get-M2VpsState -ServerRoot $ServerRoot
    $saved.tunnelPid = $process.Id
    $saved.tunnelPorts = @($plan)
    Save-M2VpsState -ServerRoot $ServerRoot -State $saved
    $panel = 7788; $seban = 7790; $shop = 7791
    if ($status) { $panel = [int]$status.PanelPort; $seban = [int]$status.SebanPanelPort; $shop = [int]$status.ItemShopPort }
    $addresses = Get-M2VpsPanelAddresses -Plan $plan -PanelPort $panel -SebanPort $seban -ItemShopPort $shop
    $addresses | Add-Member -NotePropertyName Pid -NotePropertyValue $process.Id
    $addresses | Add-Member -NotePropertyName Plan -NotePropertyValue $plan
    return $addresses
}

# ---------------------------------------------------------------- logs, accounts

function Get-M2VpsLogs {
    # The VPS's install log and the game's containers' logs, masked on the
    # VPS and once more here with the launcher's own redaction.
    param([Parameter(Mandatory = $true)]$State, [int]$Lines = 200)
    $result = Invoke-M2Vps -State $State -Command (Get-M2VpsScriptCommand -State $State -Arguments ('logs {0}' -f [Math]::Max(10, [Math]::Min(5000, $Lines)))) -TimeoutSeconds 120
    $text = ([string]$result.Output) + $(if ($result.Error) { "`n" + [string]$result.Error } else { '' })
    # Protect-M2LogContent is the launcher module's own and not exported, so
    # it is asked for inside that module (a Get-Command from here never found
    # it, and the second masking silently did nothing).
    $launcher = @(Get-Module -Name 'Metin2Launcher') | Select-Object -First 1
    if ($launcher) {
        try { $text = [string](& $launcher { param($t) Protect-M2LogContent -Text $t } $text) } catch { }
    }
    return $text
}

function ConvertFrom-M2VpsAccounts {
    # Pure: /root/metin2-accounts.txt as `passwords --raw' prints it.
    param([AllowEmptyString()][string]$Text = '')
    $accounts = @()
    foreach ($line in ($Text -split "`r?`n")) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith('#')) { continue }
        $parts = $trimmed -split '\s+', 3
        # A login as the script makes one (lower case, digits): a note or a
        # warning that reached stdout is not taken for an account.
        if ($parts.Count -lt 2 -or $parts[0] -cnotmatch '^[a-z0-9_]{2,30}$') { continue }
        $accounts += [pscustomobject]@{ Login = $parts[0]; Password = $parts[1]; Note = $(if ($parts.Count -ge 3) { $parts[2] } else { '' }) }
    }
    return $accounts
}

function Get-M2VpsAccounts {
    # In-process only - the window and the console show them, no log does.
    param([Parameter(Mandatory = $true)]$State)
    $result = Invoke-M2Vps -State $State -Command (Get-M2VpsScriptCommand -State $State -Arguments 'passwords --raw') -TimeoutSeconds 60
    if ($result.ExitCode -ne 0) { throw ('VPS nie podał haseł: ' + ([string]$result.Error).Trim()) }
    return @(ConvertFrom-M2VpsAccounts -Text ([string]$result.Output))
}

# ---------------------------------------------------------------- client, friends

function Get-M2VpsGamePorts {
    # Pure: the auth port and every game core's port of a range - three a
    # channel, ten apart - the order an invite code carries them in.
    param([int]$AuthPort = 11000, [AllowEmptyString()][string]$GamePortRange = '13000-13002')
    $ports = @($AuthPort)
    if ($GamePortRange -match '^\s*(\d+)\s*-\s*(\d+)\s*$') {
        $from = [int]$Matches[1]; $to = [int]$Matches[2]
        if ($to -ge $from -and ($to - $from) -le 40) {
            foreach ($p in $from..$to) { if ((($p - $from) % 10) -le 2) { $ports += $p } }
        }
    }
    if ($ports.Count -eq 1) { $ports += @(13000, 13001, 13002) }
    return $ports
}

function Get-M2VpsWorldAddress {
    # Where players connect: what the VPS's .env names, else the host.
    param([Parameter(Mandatory = $true)]$State, $Status = $null)
    if ($Status -and [string]$Status.PublicAddress) { return [string]$Status.PublicAddress }
    return [string]$State.host
}

function Write-M2VpsClientEntry {
    # The VPS as the second server on the client's list: coop.cfg, the file a
    # COOP invite writes (Write-M2CoopClientConfig). The client has one such
    # entry, so a friend's world written there before is replaced - the
    # caller is told which.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$ServerRoot, $Status = $null, [string]$ClientFolder = '')
    if (-not (Get-Command Write-M2CoopClientConfig -ErrorAction SilentlyContinue)) { throw 'Brak modułu COOP (launcher\Metin2Launcher.Coop.psm1) - on zapisuje serwer w kliencie.' }
    if (-not $ClientFolder) { $ClientFolder = Get-M2CoopClientFolder -ServerRoot $ServerRoot }
    if (-not $ClientFolder) { throw 'Nie wiem, gdzie jest klient - wskaż go przyciskiem WYBIERZ KLIENTA.' }
    if (-not $Status) { $Status = Get-M2VpsStatus -State $State }
    $ports = Get-M2VpsGamePorts -AuthPort ([int]$Status.AuthPort) -GamePortRange ([string]$Status.GamePortRange)
    $game = @($ports | Select-Object -Skip 1)
    $previous = ''
    $cfg = Join-Path $ClientFolder 'coop.cfg'
    if (Test-Path -LiteralPath $cfg -PathType Leaf) {
        $text = [IO.File]::ReadAllText($cfg)
        if ($text -match '(?m)^name=(.*)$') { $previous = $Matches[1].Trim() }
    }
    $entry = [pscustomobject]@{
        name = [string]$State.worldName; host = (Get-M2VpsWorldAddress -State $State -Status $Status); auth = [int]$ports[0]
        channel = [int]$game[0]; channels = [Math]::Max(1, [int][Math]::Ceiling($game.Count / 3.0))
    }
    $path = Write-M2CoopClientConfig -ClientFolder $ClientFolder -Invite $entry
    return [pscustomobject]@{ Path = $path; Name = $entry.name; Host = $entry.host; Replaced = $(if ($previous -and $previous -ne $entry.name) { $previous } else { '' }) }
}

function Test-M2VpsInviteAccess {
    # Invites to the VPS world are the COOP testers', like every invite.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    if (-not (Get-Command Test-M2CoopAccess -ErrorAction SilentlyContinue)) { return $false }
    return [bool](Test-M2CoopAccess -ServerRoot $ServerRoot)
}

function Assert-M2VpsInviteAccess {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    if (-not (Test-M2VpsInviteAccess -ServerRoot $ServerRoot)) {
        throw 'Zaproszenia dla znajomych (także na VPS) testują na razie patroni: odblokuj COOP ich hasłem (okno COOP albo menu tekstowe).'
    }
}

function ConvertTo-M2VpsAsciiName {
    # Pure: a friend's name as the VPS's account file keeps it - plain
    # letters, "Łukasz" as "Lukasz" rather than "ukasz".
    param([AllowEmptyString()][string]$Name = '')
    $plain = $Name
    $pairs = @{ [char]0x0105 = 'a'; [char]0x0107 = 'c'; [char]0x0119 = 'e'; [char]0x0142 = 'l'; [char]0x0144 = 'n'; [char]0x00F3 = 'o'
        [char]0x015B = 's'; [char]0x017A = 'z'; [char]0x017C = 'z'; [char]0x0104 = 'A'; [char]0x0106 = 'C'; [char]0x0118 = 'E'
        [char]0x0141 = 'L'; [char]0x0143 = 'N'; [char]0x00D3 = 'O'; [char]0x015A = 'S'; [char]0x0179 = 'Z'; [char]0x017B = 'Z' }
    foreach ($k in $pairs.Keys) { $plain = $plain.Replace([string]$k, $pairs[$k]) }
    return (($plain -replace '[^A-Za-z0-9 ._-]', '').Trim())
}

function New-M2VpsFriend {
    # A game account on the VPS for a friend, made there (vps-install.sh
    # add-account), its password kept only in the VPS's root-only file.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Name)
    Assert-M2VpsInviteAccess -ServerRoot $ServerRoot
    $plain = ConvertTo-M2VpsAsciiName -Name $Name
    $base = ($plain.ToLowerInvariant() -replace '[^a-z0-9]', '')
    if ($base.Length -lt 2) { throw 'Imię znajomego: co najmniej dwie litery lub cyfry.' }
    if ($base.Length -gt 12) { $base = $base.Substring(0, 12) }
    $note = ('znajomy: ' + $plain)
    if ($note.Length -gt 40) { $note = $note.Substring(0, 40) }
    $result = Invoke-M2Vps -State $State -Command (Get-M2VpsScriptCommand -State $State -Arguments ("add-account {0} '{1}'" -f $base, $note)) -TimeoutSeconds 60
    if ($result.ExitCode -ne 0) { throw ('VPS nie założył konta: ' + ([string]$result.Error).Trim()) }
    $v = ConvertFrom-M2VpsKeyValue -Text ([string]$result.Output)
    if (-not ($v.Contains('login') -and $v.Contains('password'))) { throw 'VPS nie podał loginu i hasła nowego konta.' }
    return [pscustomobject]@{ name = $plain; login = [string]$v['login']; password = [string]$v['password']; socialId = $(if ($v.Contains('social_id')) { [string]$v['social_id'] } else { '' }) }
}

function Get-M2VpsFriendInvite {
    # A COOP invite code to the VPS world (New-M2CoopInvite): the friend
    # pastes it in the COOP window's joining tab or in Dolacz.bat.
    param([Parameter(Mandatory = $true)]$State, [Parameter(Mandatory = $true)][string]$ServerRoot,
        [Parameter(Mandatory = $true)]$Account, $Status = $null)
    Assert-M2VpsInviteAccess -ServerRoot $ServerRoot
    if (-not (Get-Command New-M2CoopInvite -ErrorAction SilentlyContinue)) { throw 'Brak modułu COOP (launcher\Metin2Launcher.Coop.psm1).' }
    if (-not $Status) { $Status = Get-M2VpsStatus -State $State }
    $ports = Get-M2VpsGamePorts -AuthPort ([int]$Status.AuthPort) -GamePortRange ([string]$Status.GamePortRange)
    $login = $(if (@($Account.PSObject.Properties.Name) -contains 'login') { [string]$Account.login } else { [string]$Account.Login })
    $password = $(if (@($Account.PSObject.Properties.Name) -contains 'password') { [string]$Account.password } else { [string]$Account.Password })
    return (New-M2CoopInvite -HostAddress (Get-M2VpsWorldAddress -State $State -Status $Status) -Ports ([int[]]$ports) `
            -WorldName ([string]$State.worldName) -Login $login -Password $password)
}

Export-ModuleMember -Function Get-M2VpsStatePath, Get-M2VpsDefaultKeyPath, New-M2VpsStateObject, Get-M2VpsState, Save-M2VpsState,
    Test-M2VpsHostName, Test-M2VpsUserName, Test-M2VpsRemoteDir, Test-M2VpsPrivateAddress, Assert-M2VpsState,
    Get-M2VpsTool, ConvertTo-M2VpsArgument, ConvertTo-M2VpsCommandLine, Start-M2VpsProcess, Invoke-M2VpsProcess,
    Get-M2VpsSshArguments, Get-M2VpsRemoteCommand, Get-M2VpsScriptCommand, Get-M2VpsSshError, Invoke-M2Vps,
    New-M2VpsKey, Get-M2VpsKeyInstallScript, Install-M2VpsKey,
    Get-M2VpsProbeScript, ConvertFrom-M2VpsKeyValue, ConvertFrom-M2VpsProbe, Get-M2VpsBotCount, Get-M2VpsMachineVerdict,
    Test-M2VpsMachine, Format-M2VpsMachineReport,
    Test-M2VpsUploadExcluded, Get-M2VpsNestedExcludes, Get-M2VpsUploadEntries, Get-M2VpsTarArguments, Get-M2VpsUnpackCommand, Send-M2VpsServer,
    ConvertFrom-M2VpsStatus, Get-M2VpsStatus, Compare-M2VpsVersion, Wait-M2VpsJob, Get-M2VpsInstallAddressArgument, Install-M2Vps, Update-M2Vps,
    Test-M2VpsLocalPortFree, Get-M2VpsTunnelPlan, Get-M2VpsTunnelProcess, Get-M2VpsPanelAddresses, Close-M2VpsPanel, Open-M2VpsPanel,
    Get-M2VpsLogs, ConvertFrom-M2VpsAccounts, Get-M2VpsAccounts,
    Get-M2VpsGamePorts, Get-M2VpsWorldAddress, Write-M2VpsClientEntry, Test-M2VpsInviteAccess, Assert-M2VpsInviteAccess,
    ConvertTo-M2VpsAsciiName, New-M2VpsFriend, Get-M2VpsFriendInvite
