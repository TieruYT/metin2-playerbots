# The launcher's half of "Instaluj na VPS" (launcher\Metin2Launcher.Vps.psm1)
# with no VPS: the pure parts on their own, and everything that talks to a
# server against a fake ssh.exe compiled here. The fake writes down its
# arguments and what came on its stdin, answers by a rule the test sets, and
# for a tunnel (-N) listens on the forwarded ports until it is killed.
# ssh-keygen.exe and tar.exe are the real ones Windows ships, run on files in
# a temporary folder whose name has a space in it. Nothing here touches the
# network, Docker or a game server. Run with Windows PowerShell 5.1 - the
# launcher's own engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\vps_launcher_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$repo = Split-Path -Parent $PSScriptRoot
$modulePath = Join-Path $repo 'launcher\Metin2Launcher.Vps.psm1'
$parserErrors = $null
[void][Management.Automation.Language.Parser]::ParseFile($modulePath, [ref]$null, [ref]$parserErrors)
if (@($parserErrors).Count -ne 0) { throw ('Metin2Launcher.Vps.psm1 does not parse: ' + ($parserErrors | Out-String)) }
Import-Module $modulePath -Force
Import-Module (Join-Path $repo 'launcher\Metin2Launcher.Coop.psm1') -Force
# For its log masking (Protect-M2LogContent), which Get-M2VpsLogs borrows.
Import-Module (Join-Path $repo 'launcher\Metin2Launcher.psm1') -Force

$script:failed = 0
$script:passed = 0
function Check {
    param([string]$Name, [bool]$Ok, [string]$Detail = '')
    if ($Ok) { $script:passed++ }
    else { $script:failed++; Write-Host ("FAIL {0} {1}" -f $Name, $Detail) }
}
function Get-ThrownMessage {
    # The message a block throws, '' when it throws nothing.
    param([scriptblock]$Block)
    try { & $Block | Out-Null; return '' } catch { return [string]$_.Exception.Message }
}
function Write-Utf8 { param([string]$Path, [string]$Text) [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding $false)) }

$work = Join-Path ([IO.Path]::GetTempPath()) ('m2 vps test ' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $work | Out-Null
$tunnels = @()
# A UTF-8 console with a byte order mark, as chcp 65001 or Windows' "UTF-8 for
# worldwide language support" gives one: .NET then puts EF BB BF at the head
# of every redirected stdin (Start-M2VpsProcess), and the archive and the
# probe below must arrive without it whatever console this runs in.
$consoleInput = $null
try { $consoleInput = [Console]::InputEncoding; [Console]::InputEncoding = [Text.Encoding]::UTF8 }
catch { $consoleInput = $null; Write-Host ('note: the console encoding could not be set here: ' + $_.Exception.Message) }
try {
    # ---------------------------------------------------------------- the fake ssh
    $fakeDir = Join-Path $work 'fake ssh'
    New-Item -ItemType Directory -Path $fakeDir | Out-Null
    $fakeExe = Join-Path $fakeDir 'ssh.exe'
    Add-Type -OutputAssembly $fakeExe -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

public static class FakeSsh {
    public static int Main(string[] args) {
        string dir = Environment.GetEnvironmentVariable("M2_FAKE_SSH_DIR");
        UTF8Encoding utf8 = new UTF8Encoding(false);
        File.WriteAllLines(Path.Combine(dir, "argv.txt"), args, utf8);
        if (Array.IndexOf(args, "-N") >= 0) {
            if (File.Exists(Path.Combine(dir, "tunnel-fail"))) { return 255; }
            List<TcpListener> held = new List<TcpListener>();
            for (int i = 0; i + 1 < args.Length; i++) {
                if (args[i] != "-L") { continue; }
                TcpListener listener = new TcpListener(IPAddress.Loopback, int.Parse(args[i + 1].Split(':')[0]));
                listener.Start();
                held.Add(listener);
            }
            Thread.Sleep(120000);
            GC.KeepAlive(held);
            return 0;
        }
        string command = args.Length > 0 ? args[args.Length - 1] : "";
        string calls = Path.Combine(dir, "calls.txt");
        int n = File.Exists(calls) ? File.ReadAllLines(calls).Length + 1 : 1;
        File.AppendAllText(calls, command.Replace("\r", " ").Replace("\n", " ") + "\n", utf8);
        using (Stream input = Console.OpenStandardInput())
        using (FileStream copy = File.Create(Path.Combine(dir, "stdin-" + n + ".bin"))) { input.CopyTo(copy); }
        int code = 0;
        string rules = Path.Combine(dir, "rules.txt");
        if (File.Exists(rules)) {
            foreach (string line in File.ReadAllLines(rules)) {
                string[] f = line.Split('\t');
                if (f.Length < 3 || !(f[0] == "*" || command.Contains(f[0]))) { continue; }
                int.TryParse(f[1], out code);
                Pass(Path.Combine(dir, f[2]), Console.OpenStandardOutput());
                if (f.Length > 3) { Pass(Path.Combine(dir, f[3]), Console.OpenStandardError()); }
                break;
            }
        }
        return code;
    }
    static void Pass(string file, Stream target) {
        if (!File.Exists(file)) { return; }
        byte[] bytes = File.ReadAllBytes(file);
        target.Write(bytes, 0, bytes.Length);
        target.Flush();
    }
}
'@
    $env:M2_FAKE_SSH_DIR = $fakeDir
    # The module asks Get-M2VpsTool for ssh; inside the module that is the
    # fake from here on. tar and ssh-keygen stay the real ones.
    & (Get-Module Metin2Launcher.Vps) {
        param($fake)
        $script:M2TestFakeSsh = $fake
        $script:M2TestRealTool = ${function:Get-M2VpsTool}
        Set-Item -Path function:script:Get-M2VpsTool -Value {
            param([Parameter(Mandatory = $true)][string]$Name)
            if ($Name -eq 'ssh') { return $script:M2TestFakeSsh }
            return (& $script:M2TestRealTool -Name $Name)
        }
    } $fakeExe
    function Set-Fake {
        # Rules: "substring of the remote command<TAB>exit code<TAB>stdout file[<TAB>stderr file]"; * is any.
        param([string[]]$Rules = @(), [hashtable]$Files = @{})
        foreach ($name in $Files.Keys) { Write-Utf8 (Join-Path $fakeDir $name) ([string]$Files[$name]) }
        Write-Utf8 (Join-Path $fakeDir 'rules.txt') (($Rules -join "`n") + "`n")
        foreach ($old in @(Get-ChildItem -LiteralPath $fakeDir -File | Where-Object { $_.Name -eq 'calls.txt' -or $_.Name -eq 'argv.txt' -or $_.Name -like 'stdin-*.bin' })) {
            Remove-Item -LiteralPath $old.FullName -Force
        }
    }
    function Get-FakeArgv { return @([IO.File]::ReadAllLines((Join-Path $fakeDir 'argv.txt'), [Text.Encoding]::UTF8)) }
    function Get-FakeCalls {
        $path = Join-Path $fakeDir 'calls.txt'
        if (-not (Test-Path -LiteralPath $path)) { return @() }
        return @([IO.File]::ReadAllLines($path, [Text.Encoding]::UTF8))
    }
    function Get-FakeStdin { param([int]$Call) return [IO.File]::ReadAllBytes((Join-Path $fakeDir ('stdin-{0}.bin' -f $Call))) }

    # ---------------------------------------------------------------- state
    $defaults = New-M2VpsStateObject
    Check 'defaults: root on port 22' ($defaults.user -eq 'root' -and $defaults.port -eq 22)
    Check 'defaults: /opt/metin2, the folder update.sh stages into' ($defaults.remoteDir -eq '/opt/metin2')
    Check 'defaults: the key in the profile, not in the server folder' ($defaults.keyPath -eq (Join-Path $env:USERPROFILE '.ssh\metin2_vps')) $defaults.keyPath
    $from = [pscustomobject]@{ host = ' 203.0.113.7 '; user = 'debian'; port = '2222'; password = 'hunter2'; rootPassword = 'hunter3'
        tunnelPorts = @([pscustomobject]@{ remote = 7788; local = 17788 }) }
    $state = New-M2VpsStateObject -From $from
    $names = @($state.PSObject.Properties.Name)
    Check 'a password never becomes a field' (-not ($names -contains 'password') -and -not ($names -contains 'rootPassword')) ($names -join ',')
    Check 'the host is trimmed and the port read from text' ($state.host -eq '203.0.113.7' -and $state.port -eq 2222)
    $root = Join-Path $work 'server root'
    New-Item -ItemType Directory -Path $root | Out-Null
    Save-M2VpsState -ServerRoot $root -State $state
    $again = Get-M2VpsState -ServerRoot $root
    $again.user = 'ubuntu'
    # The second save replaces an existing file: File.Replace with no backup,
    # which PowerShell's $null would have turned into an illegal "" path.
    $second = Get-ThrownMessage { Save-M2VpsState -ServerRoot $root -State $again }
    Check 'a second save replaces the file' ($second -eq '') $second
    $statePath = Get-M2VpsStatePath -ServerRoot $root
    $json = [IO.File]::ReadAllText($statePath)
    Check 'no password reaches the file' ($json -notmatch 'hunter|password')
    Check 'no temporary file is left' (-not (Test-Path -LiteralPath ($statePath + '.tmp')))
    $back = Get-M2VpsState -ServerRoot $root
    Check 'the round trip keeps host, user and port' ($back.host -eq '203.0.113.7' -and $back.user -eq 'ubuntu' -and $back.port -eq 2222)
    Check 'a single tunnel pair survives the round trip' (@($back.tunnelPorts).Count -eq 1 -and [int]@($back.tunnelPorts)[0].local -eq 17788)
    Write-Utf8 $statePath '{ this is not json'
    $broken = Get-M2VpsState -ServerRoot $root
    Check 'a broken state file reads as the defaults' ($broken.host -eq '' -and $broken.user -eq 'root')

    # ---------------------------------------------------------------- what may be typed
    foreach ($good in @('203.0.113.7', '1.2.3.4', 'vps.example.org', 'my-vps', 'VPS1.Example.com')) { Check ('a host: ' + $good) (Test-M2VpsHostName $good) }
    foreach ($bad in @('', '256.1.1.1', 'a b', 'host;reboot', "x'y", 'user@host', '-oProxyCommand=calc', 'x..y', '-vps', 'vps-')) {
        Check ('not a host: [' + $bad + ']') (-not (Test-M2VpsHostName $bad))
    }
    foreach ($good in @('root', 'debian', 'ubuntu', 'ec2-user', '_svc', 'a.b')) { Check ('a user: ' + $good) (Test-M2VpsUserName $good) }
    foreach ($bad in @('', 'Root', '1abc', 'a b', 'x;y', ('a' * 33))) { Check ('not a user: [' + $bad + ']') (-not (Test-M2VpsUserName $bad)) }
    foreach ($good in @('/opt/metin2', '/home/debian/metin2', '/srv/m2.world')) { Check ('a folder: ' + $good) (Test-M2VpsRemoteDir $good) }
    foreach ($bad in @('', '/', '/opt', 'opt/metin2', '/opt/../etc', '/opt/./x', '/opt/me tin2', "/opt/x'y", '/opt/x;y', '/opt/metin2/')) {
        Check ('not a folder: [' + $bad + ']') (-not (Test-M2VpsRemoteDir $bad))
    }
    foreach ($private in @('10.0.0.1', '127.0.0.1', '172.16.0.1', '172.31.255.255', '192.168.1.5', '100.64.0.1')) { Check ('private: ' + $private) (Test-M2VpsPrivateAddress $private) }
    foreach ($public in @('8.8.8.8', '172.32.0.1', '100.128.0.1', '203.0.113.7', 'vps.example.org')) { Check ('not private: ' + $public) (-not (Test-M2VpsPrivateAddress $public)) }
    $bad = New-M2VpsStateObject; $bad.host = '203.0.113.7'; $bad.port = 0
    Check 'port 0 is refused' ((Get-ThrownMessage { Assert-M2VpsState -State $bad }) -match 'Port SSH')
    $bad.port = 22; $bad.remoteDir = '/'
    Check 'the root folder is refused' ((Get-ThrownMessage { Assert-M2VpsState -State $bad }) -match '/opt/metin2')
    $bad.remoteDir = '/opt/metin2'; $bad.host = ''
    Check 'no host is refused' ((Get-ThrownMessage { Assert-M2VpsState -State $bad }) -match 'IPv4')

    # ---------------------------------------------------------------- command lines
    Add-Type -Namespace M2VpsTest -Name Argv -MemberDefinition @'
[DllImport("shell32.dll", SetLastError = true)]
private static extern IntPtr CommandLineToArgvW([MarshalAs(UnmanagedType.LPWStr)] string lpCmdLine, out int pNumArgs);
[DllImport("kernel32.dll")]
private static extern IntPtr LocalFree(IntPtr hMem);
public static string[] Split(string commandLine) {
    int count;
    IntPtr argv = CommandLineToArgvW("x.exe " + commandLine, out count);
    if (argv == IntPtr.Zero) { return new string[0]; }
    try {
        string[] result = new string[count - 1];
        for (int i = 1; i < count; i++) { result[i - 1] = Marshal.PtrToStringUni(Marshal.ReadIntPtr(argv, i * IntPtr.Size)); }
        return result;
    }
    finally { LocalFree(argv); }
}
'@
    Check 'an empty argument is kept as ""' ((ConvertTo-M2VpsArgument '') -eq '""')
    Check 'a plain argument is left alone' ((ConvertTo-M2VpsArgument 'BatchMode=yes') -eq 'BatchMode=yes')
    $tricky = @('', 'plain', 'with space', 'quote"inside', 'C:\Users\x y\.ssh\metin2_vps', 'C:\dir with space\', 'a\\"b c',
        "sh -c 'umask 022; mkdir -p /opt/metin2 && tar -xzf - -C /opt/metin2'", "printf '%s\n' x", 'tab	inside', '\\server\share x\')
    $parsed = @([M2VpsTest.Argv]::Split((ConvertTo-M2VpsCommandLine -Arguments $tricky)))
    Check 'every argument comes back as it was (CommandLineToArgvW)' ($parsed.Count -eq $tricky.Count) ('{0} of {1}' -f $parsed.Count, $tricky.Count)
    for ($i = 0; $i -lt [Math]::Min($parsed.Count, $tricky.Count); $i++) {
        Check ('argument {0} round trip' -f $i) ($parsed[$i] -ceq $tricky[$i]) ('[' + $tricky[$i] + '] -> [' + $parsed[$i] + ']')
    }

    # ---------------------------------------------------------------- ssh's arguments
    $vps = New-M2VpsStateObject
    $vps.host = '203.0.113.7'; $vps.user = 'debian'; $vps.port = 2222; $vps.keyPath = Join-Path $work 'key dir\metin2_vps'
    $a = @(Get-M2VpsSshArguments -State $vps -BatchMode -Command 'uptime')
    Check 'ssh: the port first' ($a[0] -eq '-p' -and $a[1] -eq '2222')
    Check 'ssh: the launcher key and only it' (($a -join ' ').Contains('-i ' + $vps.keyPath + ' -o IdentitiesOnly=yes'))
    Check 'ssh: batch mode, a new host key accepted, a changed one not' ($a -contains 'BatchMode=yes' -and $a -contains 'StrictHostKeyChecking=accept-new')
    Check 'ssh: user@host, then the command, last' ($a[-2] -eq 'debian@203.0.113.7' -and $a[-1] -eq 'uptime')
    $t = @(Get-M2VpsSshArguments -State $vps -BatchMode -NoCommand -Forward @('17788:127.0.0.1:7788', '7790:127.0.0.1:7790'))
    Check 'tunnel: -L for each port, ExitOnForwardFailure, -N, no command' (($t -join ' ').Contains('-L 17788:127.0.0.1:7788 -L 7790:127.0.0.1:7790 -o ExitOnForwardFailure=yes -N debian@203.0.113.7') -and $t[-1] -eq 'debian@203.0.113.7')
    Check 'no key: no -i' (-not (@(Get-M2VpsSshArguments -State $vps -NoKey) -contains '-i'))
    $rootVps = New-M2VpsStateObject -From $vps; $rootVps.user = 'root'
    Check 'root runs a command as it is' ((Get-M2VpsRemoteCommand -State $rootVps -Command 'id') -eq 'id')
    Check 'another user runs it through sudo -n' ((Get-M2VpsRemoteCommand -State $vps -Command 'id') -eq 'sudo -n id')
    Check 'the script command' ((Get-M2VpsScriptCommand -State $vps -Arguments 'status') -eq 'sudo -n sh /opt/metin2/linux-port/tools/vps-install.sh status')
    $unpack = Get-M2VpsUnpackCommand -State $vps
    Check 'unpack: no double quote on the way through Windows' (-not $unpack.Contains('"')) $unpack
    Check 'unpack: 644/755 and root-owned whatever the archive says' ($unpack -match 'umask 022' -and $unpack -match '--no-same-owner' -and $unpack -match '--no-same-permissions' -and $unpack -match '-C /opt/metin2') $unpack
    Check 'unpack: through sudo for a user that is not root' ($unpack.StartsWith('sudo -n sh -c ')) $unpack
    Check 'ssh error: a changed host key' ((Get-M2VpsSshError -Text 'WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!' -HostName 'h') -match 'ssh-keygen -R h')
    Check 'ssh error: the key refused' ((Get-M2VpsSshError -Text 'debian@h: Permission denied (publickey).') -match 'KLUCZ SSH')
    Check 'ssh error: an unknown name' ((Get-M2VpsSshError -Text 'ssh: Could not resolve hostname vps.exmaple.org: No such host is known.' -HostName 'vps.exmaple.org') -match 'vps\.exmaple\.org')
    Check 'ssh error: refused' ((Get-M2VpsSshError -Text 'ssh: connect to host h port 22: Connection refused' -HostName 'h') -match 'port')
    Check 'ssh error: timed out' ((Get-M2VpsSshError -Text 'ssh: connect to host h port 22: Connection timed out' -HostName 'h') -match 'nie odpowiada')
    Check 'ssh error: sudo wants a password' ((Get-M2VpsSshError -Text 'sudo: a password is required' -ExitCode 1) -match 'NOPASSWD')
    Check 'ssh error: the remote command failing is not an ssh error' ((Get-M2VpsSshError -Text 'BLAD: cos' -ExitCode 1) -eq '')

    # ---------------------------------------------------------------- the key
    $keygenPresent = $true
    try { [void](& (Get-Module Metin2Launcher.Vps) { Get-M2VpsTool 'ssh-keygen' }) } catch { $keygenPresent = $false }
    $public = 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGZha2VrZXlmb3J0aGV0ZXN0MDEyMzQ1Njc4OWFiY2RlZg metin2-launcher@pc'
    if ($keygenPresent) {
        # The real ssh-keygen: an empty passphrase has to arrive as an empty
        # argument, which Windows PowerShell 5.1's own quoting drops.
        $made = New-M2VpsKey -KeyPath $vps.keyPath
        Check 'ssh-keygen made an ed25519 key with no passphrase, into a folder with a space' ($made -match '^ssh-ed25519 ' -and (Test-Path -LiteralPath $vps.keyPath)) $made
        Check 'the key is made once' ((New-M2VpsKey -KeyPath $vps.keyPath) -eq $made)
        $public = $made
    }
    else { Write-Host 'skip: no ssh-keygen.exe on this machine (OpenSSH Client not installed)' }
    $cmd = Get-M2VpsKeyInstallScript -State $vps -PublicKey $public -SshPath $fakeExe
    Check 'key script: batch lines end in CRLF' ($cmd.Contains("`r`n") -and -not ($cmd -replace "`r`n", '').Contains("`n"))
    Check 'key script: every % is doubled for batch' (-not ($cmd -replace '%%', '').Contains('%'))
    $cmdPath = Join-Path $work 'key script.cmd'
    [IO.File]::WriteAllText($cmdPath, $cmd, [Text.Encoding]::ASCII)
    Set-Fake -Rules @()
    $ran = Invoke-M2VpsProcess -FilePath (Join-Path $env:SystemRoot 'System32\cmd.exe') -Arguments @('/d', '/c', $cmdPath) -TimeoutSeconds 60
    $argv = Get-FakeArgv
    $expected = "umask 077; mkdir -p ~/.ssh && touch ~/.ssh/authorized_keys && (grep -qxF '{0}' ~/.ssh/authorized_keys || printf '%s\n' '{0}' >> ~/.ssh/authorized_keys) && echo KLUCZ-DODANY" -f $public
    Check 'key script: ssh gets the port, accept-new and user@host' ($argv[0] -eq '-p' -and $argv[1] -eq '2222' -and ($argv -contains 'StrictHostKeyChecking=accept-new') -and $argv[-2] -eq 'debian@203.0.113.7') ($argv -join ' | ')
    Check 'key script: the remote command arrives whole, % undoubled' ($argv[-1] -ceq $expected) $argv[-1]
    Check 'key script: ends 0 when ssh does' ($ran.ExitCode -eq 0) ([string]$ran.ExitCode)

    # ---------------------------------------------------------------- the machine
    $probeText = "probe=1`narch=x86_64`nos_id=debian`nos_like=`nos_version=12`nos_name=Debian GNU/Linux 12 (bookworm)`nmem_kb=8039512`nswap_kb=0`ndisk_kb=77000000`nuid=0`nsudo=root`ndocker=0`ntar=1`nvirt=kvm`nscript=0`n"
    $probe = ConvertFrom-M2VpsProbe -Text $probeText
    Check 'probe: read' ($probe.Answered -and $probe.Arch -eq 'x86_64' -and $probe.MemMB -eq 7851 -and $probe.DiskMB -eq 75195 -and $probe.Sudo -eq 'root' -and -not $probe.Docker -and $probe.Tar)
    $verdict = Get-M2VpsMachineVerdict -Probe $probe
    Check 'a Debian 12 box of 8 GB: fine, 400 bots' ($verdict.Ok -and $verdict.Bots -eq 400 -and @($verdict.Warnings).Count -eq 0) (@($verdict.Warnings) -join ' / ')
    $arm = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'arch=x86_64', 'arch=aarch64')
    Check 'ARM is refused' (-not (Get-M2VpsMachineVerdict -Probe $arm).Ok -and ((@((Get-M2VpsMachineVerdict -Probe $arm).Blocking) -join ' ') -match 'ARM'))
    $nosudo = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'sudo=root', 'sudo=no')
    Check 'a user without sudo is refused' (-not (Get-M2VpsMachineVerdict -Probe $nosudo -User 'debian').Ok)
    $small = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'disk_kb=77000000', 'disk_kb=9000000')
    Check 'a small disk is refused' (-not (Get-M2VpsMachineVerdict -Probe $small).Ok)
    $fortyGb = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'disk_kb=77000000', 'disk_kb=30000000')
    $v40 = Get-M2VpsMachineVerdict -Probe $fortyGb
    Check '30 GB of disk: a warning, not a refusal' ($v40.Ok -and @($v40.Warnings).Count -eq 1)
    $twoGb = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'mem_kb=8039512', 'mem_kb=2000000')
    $v2 = Get-M2VpsMachineVerdict -Probe $twoGb
    Check '2 GB and no swap: a warning about the swap file, 150 bots' ($v2.Ok -and $v2.Bots -eq 150 -and ((@($v2.Warnings) -join ' ') -match 'wymiany'))
    $alpine = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'os_id=debian', 'os_id=alpine' -replace 'os_version=12', 'os_version=3.20')
    Check 'not Debian or Ubuntu and no Docker: refused' (-not (Get-M2VpsMachineVerdict -Probe $alpine).Ok)
    $alpineDocker = ConvertFrom-M2VpsProbe -Text (($probeText -replace 'os_id=debian', 'os_id=alpine') -replace 'docker=0', 'docker=1')
    Check 'not Debian or Ubuntu with Docker: a warning' ((Get-M2VpsMachineVerdict -Probe $alpineDocker).Ok)
    $trixie = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'os_version=12', 'os_version=13')
    Check 'Debian 13 is supported' ((Get-M2VpsMachineVerdict -Probe $trixie).Ok -and @((Get-M2VpsMachineVerdict -Probe $trixie).Warnings).Count -eq 0)
    $lxc = ConvertFrom-M2VpsProbe -Text ($probeText -replace 'virt=kvm', 'virt=openvz')
    Check 'an OpenVZ container: a warning' (((@((Get-M2VpsMachineVerdict -Probe $lxc).Warnings)) -join ' ') -match 'KVM')
    Check 'no answer is refused' (-not (Get-M2VpsMachineVerdict -Probe (ConvertFrom-M2VpsProbe -Text '')).Ok)
    Check 'bots by memory' ((Get-M2VpsBotCount -MemMB 3900) -eq 150 -and (Get-M2VpsBotCount -MemMB 7700) -eq 400 -and (Get-M2VpsBotCount -MemMB 15800) -eq 800)
    Set-Fake -Rules @("sh -s`t0`tprobe.out") -Files @{ 'probe.out' = $probeText }
    $machine = Test-M2VpsMachine -State $vps
    Check 'the probe runs as the user, with sh -s' ((Get-FakeArgv)[-1] -eq 'sh -s')
    # Byte for byte: a culture-aware StartsWith ignores a byte order mark.
    $sentBytes = Get-FakeStdin 1
    $probeBytes = (New-Object Text.UTF8Encoding $false).GetBytes((Get-M2VpsProbeScript -RemoteDir '/opt/metin2'))
    Check 'the probe goes on stdin exactly, no byte order mark' ([Convert]::ToBase64String($sentBytes) -eq [Convert]::ToBase64String($probeBytes)) (($sentBytes | Select-Object -First 4 | ForEach-Object { $_.ToString('x2') }) -join ' ')
    $sent = [Text.Encoding]::UTF8.GetString($sentBytes)
    Check 'the probe: LF only, the folder in it' (-not $sent.Contains("`r") -and $sent.Contains('/opt/metin2'))
    Check 'the probe answer is judged' ($machine.Verdict.Ok -and $machine.Probe.OsVersion -eq '12')
    $report = @(Format-M2VpsMachineReport -Machine $machine)
    Check 'the report ends with "Mozna instalowac"' ($report[-1] -match 'instalowa' -and $report[-1] -match '400') ($report -join ' / ')

    # ---------------------------------------------------------------- the upload
    foreach ($out in @('.env', '.env.last-good', '.env.bak-2.0.39', '.m2coop.json', '.m2vps.json', '.m2launcher.json', '.vps-install.log',
            'backups', 'launcher-logs', 'support-bundles', 'Klient', 'client', '.git', 'x.log', 'metin2-support-20260925.zip')) {
        Check ('not uploaded: ' + $out) (Test-M2VpsUploadExcluded -Name $out)
    }
    foreach ($in in @('.env.example', 'VERSION', 'linux-port', 'launcher', 'Metin2-Launcher.ps1', 'CHANGELOG.md')) { Check ('uploaded: ' + $in) (-not (Test-M2VpsUploadExcluded -Name $in)) }
    $tree = Join-Path $work 'tree with space'
    foreach ($dir in @('linux-port\docker\panel', 'linux-port\tools', 'linux-port\docker\client-archive', 'backups', 'Klient', 'launcher-logs')) {
        New-Item -ItemType Directory -Force -Path (Join-Path $tree $dir) | Out-Null
    }
    $files = @{
        'VERSION' = "2.2.12`n"; 'CHANGELOG.md' = "# 2.2.12`n"; '.env' = "SECRET=top`n"; '.env.last-good' = "SECRET=top`n"; '.m2vps.json' = '{}'
        'linux-port\docker\docker-compose.yml' = "name: metin2`n"; 'linux-port\docker\.env.example' = "M2_DB_PASSWORD=`n"
        'linux-port\docker\.env' = "M2_DB_PASSWORD=secret`n"; 'linux-port\docker\.env.bak-2.0.39' = "M2_DB_PASSWORD=old`n"
        'linux-port\docker\panel\app.py' = "print('panel')`n"; 'linux-port\tools\vps-install.sh' = "#!/bin/sh`n"
        'linux-port\docker\client-archive\big.bin' = 'x'; 'backups\world.zip' = 'x'; 'Klient\metin2client.exe' = 'x'; 'launcher-logs\a.log' = 'x'
    }
    foreach ($name in $files.Keys) { Write-Utf8 (Join-Path $tree $name) $files[$name] }
    $entries = @(Get-M2VpsUploadEntries -ServerRoot $tree)
    Check 'the top level that travels' (($entries -join ',') -eq '.env.example,CHANGELOG.md,linux-port,VERSION' -or ($entries -join ',') -eq 'CHANGELOG.md,linux-port,VERSION') ($entries -join ',')
    $tarArgs = @(Get-M2VpsTarArguments -ServerRoot $tree -Entries $entries)
    Check 'tar: gzip to stdout' ($tarArgs[0] -eq '-c' -and $tarArgs[1] -eq '-z' -and $tarArgs[2] -eq '-f' -and $tarArgs[3] -eq '-')
    Check 'tar: -C the server folder, then the entries' (($tarArgs -join '|').Contains('-C|' + $tree + '|' + $entries[0]))
    Check 'a folder without the VPS script is refused' ((Get-ThrownMessage { Get-M2VpsUploadEntries -ServerRoot $work }) -match 'VPS')
    # The whole upload through the fake ssh: the archive as it reached ssh's
    # stdin, listed by the same bsdtar.
    Set-Fake -Rules @("tar -xzf`t0`tempty.out") -Files @{ 'empty.out' = '' }
    [void](Send-M2VpsServer -State $vps -ServerRoot $tree)
    Check 'the upload runs the unpack command through sudo' ((Get-FakeArgv)[-1] -ceq $unpack)
    $archive = Join-Path $work 'uploaded.tgz'
    $uploaded = Get-FakeStdin 1
    Check 'the archive starts with gzip, not a byte order mark' ($uploaded.Length -gt 2 -and $uploaded[0] -eq 0x1f -and $uploaded[1] -eq 0x8b) (($uploaded | Select-Object -First 4 | ForEach-Object { $_.ToString('x2') }) -join ' ')
    [IO.File]::WriteAllBytes($archive, $uploaded)
    $listing = Invoke-M2VpsProcess -FilePath (& (Get-Module Metin2Launcher.Vps) { Get-M2VpsTool 'tar' }) -Arguments @('-tzf', $archive) -TimeoutSeconds 60
    $listed = @(([string]$listing.Output -split "`r?`n") | Where-Object { $_ } | ForEach-Object { $_.TrimEnd('/') })
    Check 'the archive arrived whole (bsdtar reads it)' ($listing.ExitCode -eq 0 -and $listed.Count -gt 0) ([string]$listing.Error)
    foreach ($must in @('VERSION', 'CHANGELOG.md', 'linux-port/docker/docker-compose.yml', 'linux-port/docker/.env.example', 'linux-port/docker/panel/app.py', 'linux-port/tools/vps-install.sh')) {
        Check ('in the archive: ' + $must) ($listed -contains $must) ($listed -join ',')
    }
    foreach ($never in @('.env', '.env.last-good', '.m2vps.json', 'linux-port/docker/.env', 'linux-port/docker/.env.bak-2.0.39', 'linux-port/docker/client-archive/big.bin', 'backups/world.zip', 'Klient/metin2client.exe', 'launcher-logs/a.log')) {
        Check ('not in the archive: ' + $never) (-not ($listed -contains $never))
    }

    # ---------------------------------------------------------------- status, versions
    $statusText = @"
state=done
kind=install
phase=done
pid=4242
started=2026-09-25T10:00:00
finished=2026-09-25T10:31:00
exit=0
message=gotowe
log_lines=812
version=2.2.12
public_address=203.0.113.7
auth_port=11000
game_port_range=13000-13012
ch2=1
panel_port=47788
seban_panel_port=47790
itemshop_port=47791
panel_bind=127.0.0.1
host_bind=0.0.0.0
bots=400
accounts_file=1
--- log ---
[10:31:00] etap: done
koniec: done
--- docker compose ps ---
NAME   IMAGE   STATUS
"@
    $status = ConvertFrom-M2VpsStatus -Text $statusText
    Check 'status: the job' ($status.State -eq 'done' -and $status.Kind -eq 'install' -and $status.LogLines -eq 812 -and $status.Version -eq '2.2.12')
    Check 'status: the ports' ($status.AuthPort -eq 11000 -and $status.GamePortRange -eq '13000-13012' -and $status.PanelPort -eq 47788 -and $status.SebanPanelPort -eq 47790 -and $status.ItemShopPort -eq 47791)
    Check 'status: the log and the containers apart' ($status.LogText -match 'koniec: done' -and $status.LogText -notmatch 'NAME' -and $status.PsText -match 'NAME')
    $empty = ConvertFrom-M2VpsStatus -Text ''
    Check 'an empty status: unknown, with the default ports' ($empty.State -eq 'unknown' -and $empty.AuthPort -eq 11000 -and $empty.PanelPort -eq 7788)
    Check 'versions compare as numbers' ((Compare-M2VpsVersion '2.2.12' '2.2.9') -eq 1 -and (Compare-M2VpsVersion '2.2.9' '2.2.12') -eq -1 -and (Compare-M2VpsVersion '2.2.12' '2.2.12') -eq 0 -and (Compare-M2VpsVersion 'x' '2.2.12') -eq 0)
    Check '--address for a public IPv4' ((Get-M2VpsInstallAddressArgument -HostName '203.0.113.7') -eq ' --address 203.0.113.7')
    Check '--address for a domain' ((Get-M2VpsInstallAddressArgument -HostName 'vps.example.org') -eq ' --address vps.example.org')
    Check 'no --address for a private one' ((Get-M2VpsInstallAddressArgument -HostName '192.168.1.5') -eq '')
    Set-Fake -Rules @("status`t0`tstatus.out") -Files @{ 'status.out' = $statusText }
    $read = Get-M2VpsStatus -State $vps -From 800
    Check 'status through ssh: sudo -n and --from' ((Get-FakeArgv)[-1] -eq 'sudo -n sh /opt/metin2/linux-port/tools/vps-install.sh status --from 800')
    Check 'status through ssh: parsed' ($read.State -eq 'done' -and $read.PublicAddress -eq '203.0.113.7')
    Set-Fake -Rules @("logs`t0`tlogs.out") -Files @{ 'logs.out' = "--- /opt/metin2/.vps-install.log ---`n[10:00:00] etap: build`nM2_DB_PASSWORD=abc123secret`nhaslo: xyz987secret`n" }
    $logs = Get-M2VpsLogs -State $vps -Lines 3
    Check 'logs: a count of lines kept in bounds' ((Get-FakeArgv)[-1] -eq 'sudo -n sh /opt/metin2/linux-port/tools/vps-install.sh logs 10') (Get-FakeArgv)[-1]
    Check 'logs: the launcher masks what the VPS left unmasked' ($logs -match 'etap: build' -and $logs -notmatch 'abc123secret' -and $logs -notmatch 'xyz987secret') $logs
    Set-Fake -Rules @("*`t255`tempty.out`tdenied.err") -Files @{ 'empty.out' = ''; 'denied.err' = "debian@203.0.113.7: Permission denied (publickey).`n" }
    Check 'a refused key says which button to press' ((Get-ThrownMessage { Get-M2VpsStatus -State $vps }) -match 'KLUCZ SSH')
    Set-Fake -Rules @("*`t2`tempty.out`tnofile.err") -Files @{ 'empty.out' = ''; 'nofile.err' = "sh: 0: cannot open /opt/metin2/linux-port/tools/vps-install.sh: No such file`n" }
    Check 'no server on the VPS yet says install first' ((Get-ThrownMessage { Get-M2VpsStatus -State $vps }) -match 'ZAINSTALUJ NA VPS')

    # ---------------------------------------------------------------- the whole install, through the fake
    $installRoot = Join-Path $work 'install root'
    Copy-Item -LiteralPath $tree -Destination $installRoot -Recurse
    Save-M2VpsState -ServerRoot $installRoot -State $vps
    Set-Fake -Rules @("sh -s`t0`tprobe.out", "tar -xzf`t0`tempty.out", "install --no-follow`t0`tstarted.out", "status`t0`tstatus.out") `
        -Files @{ 'probe.out' = $probeText; 'empty.out' = ''; 'started.out' = "Buduje w tle.`n"; 'status.out' = $statusText }
    $final = Install-M2Vps -State (Get-M2VpsState -ServerRoot $installRoot) -ServerRoot $installRoot
    $calls = @(Get-FakeCalls)
    Check 'install: probe, upload, start, status - in that order' ($calls.Count -eq 4 -and $calls[0] -eq 'sh -s' -and $calls[1] -match 'tar -xzf' -and $calls[2] -match 'install --no-follow' -and $calls[3] -match 'status') ($calls -join ' | ')
    Check 'install: the address the player typed goes to the script' ($calls[2] -eq 'sudo -n sh /opt/metin2/linux-port/tools/vps-install.sh install --no-follow --address 203.0.113.7') $calls[2]
    Check 'install: the build is read from the line it began at' ($calls[3] -match 'status --from 0$') $calls[3]
    Check 'install: done' ($final.State -eq 'done')
    $afterInstall = Get-M2VpsState -ServerRoot $installRoot
    Check 'install: the version and the time written down' ($afterInstall.lastVersion -eq '2.2.12' -and $afterInstall.lastInstall -ne '')
    $newer = $probeText -replace 'script=0', "script=1`ninstalled=2.3.0"
    Set-Fake -Rules @("sh -s`t0`tprobe.out", "status`t0`tstatus.out") -Files @{ 'probe.out' = $newer; 'status.out' = $statusText }
    Check 'install over a newer server is refused, nothing uploaded' (((Get-ThrownMessage { Install-M2Vps -State $vps -ServerRoot $installRoot }) -match '2\.3\.0') -and -not ((Get-FakeCalls) -match 'tar -xzf'))
    $busy = $statusText -replace 'state=done', 'state=running'
    Set-Fake -Rules @("sh -s`t0`tprobe.out", "status --from`t0`tdone.out", "status`t0`tbusy.out") -Files @{ 'probe.out' = ($probeText -replace 'script=0', 'script=1'); 'busy.out' = $busy; 'done.out' = $statusText }
    $waited = Install-M2Vps -State $vps -ServerRoot $installRoot
    Check 'a job under way is waited for, nothing uploaded' ($waited.State -eq 'done' -and -not ((Get-FakeCalls) -match 'tar -xzf')) ((Get-FakeCalls) -join ' | ')

    # ---------------------------------------------------------------- the tunnel
    $free = { param($port) $port -ne 7788 -and $port -ne 17788 }
    $plan = @(Get-M2VpsTunnelPlan -RemotePorts @(7788, 7790, 7791) -IsFree $free)
    Check 'tunnel plan: a taken port moves ten or twenty thousand up, the rest keep their numbers' ([int]$plan[0].local -eq 27788 -and [int]$plan[1].local -eq 7790 -and [int]$plan[2].local -eq 7791)
    Check 'tunnel plan: nothing free is an error' ((Get-ThrownMessage { Get-M2VpsTunnelPlan -RemotePorts @(7788) -IsFree { param($p) $false } }) -match '7788')
    $addresses = Get-M2VpsPanelAddresses -Plan $plan
    Check 'panel addresses: the map of the classic panel, through the plan' ($addresses.ClassicUrl -eq 'http://127.0.0.1:27788/map' -and $addresses.SebanUrl -eq 'http://127.0.0.1:7790/' -and $addresses.ClassicPort -eq 27788)
    $tunnelRoot = Join-Path $work 'tunnel root'
    New-Item -ItemType Directory -Path $tunnelRoot | Out-Null
    Save-M2VpsState -ServerRoot $tunnelRoot -State $vps
    Set-Fake -Rules @("status`t0`tstatus.out") -Files @{ 'status.out' = $statusText }
    $opened = Open-M2VpsPanel -State $vps -ServerRoot $tunnelRoot -WaitSeconds 20
    $tunnels += [int]$opened.Pid
    $saved = Get-M2VpsState -ServerRoot $tunnelRoot
    Check 'tunnel: open, the ports the VPS named' ($opened.ClassicUrl -match '^http://127\.0\.0\.1:\d+/map$' -and @($saved.tunnelPorts).Count -eq 3 -and [int]@($saved.tunnelPorts)[0].remote -eq 47788) $opened.ClassicUrl
    Check 'tunnel: its pid written down' ($saved.tunnelPid -eq [int]$opened.Pid)
    $argvTunnel = Get-FakeArgv
    Check 'tunnel: -N, three -L to 127.0.0.1 on the VPS' (($argvTunnel -contains '-N') -and @($argvTunnel | Where-Object { $_ -match '^\d+:127\.0\.0\.1:4779[01]$|^\d+:127\.0\.0\.1:47788$' }).Count -eq 3) ($argvTunnel -join ' ')
    Check 'tunnel: found again by pid and command line' ([bool](Get-M2VpsTunnelProcess -State $saved))
    Check 'tunnel: closed' (Close-M2VpsPanel -ServerRoot $tunnelRoot)
    $gone = $true
    try { $gone = (Get-Process -Id $opened.Pid -ErrorAction Stop).WaitForExit(5000) } catch { $gone = $true }
    Check 'tunnel: the ssh process is gone' $gone
    Check 'tunnel: nothing written down after closing' ((Get-M2VpsState -ServerRoot $tunnelRoot).tunnelPid -eq 0)
    Check 'tunnel: closing twice says it was not open' (-not (Close-M2VpsPanel -ServerRoot $tunnelRoot))
    Write-Utf8 (Join-Path $fakeDir 'tunnel-fail') 'x'
    Check 'a tunnel that does not come up is an error' ((Get-ThrownMessage { Open-M2VpsPanel -State $vps -ServerRoot $tunnelRoot -WaitSeconds 3 }) -match 'Tunel')
    Check 'and leaves no pid behind' ((Get-M2VpsState -ServerRoot $tunnelRoot).tunnelPid -eq 0)
    Remove-Item -LiteralPath (Join-Path $fakeDir 'tunnel-fail') -Force

    # ---------------------------------------------------------------- accounts, client, friends
    $accountsText = "# Metin2 SinglePlayer - konta gry na tym serwerze: login, haslo, opis.`nadmin Xy7pQ2mK9vRt konto GM (postacie GM w grze)`ntest Ab3dEf6gHj8k konto testowe`n`njanek Qx7kPm2vRt4z znajomy: Janek`nUWAGA: nie udalo sie sprawdzic hasel w bazie`nNie ma jeszcze /root/metin2-accounts.txt`n"
    $accounts = @(ConvertFrom-M2VpsAccounts -Text $accountsText)
    Check 'accounts: three, the header, a warning and a note are not accounts' ($accounts.Count -eq 3) (($accounts | ForEach-Object { $_.Login }) -join ',')
    Check 'accounts: a note with spaces kept whole' ($accounts[2].Login -eq 'janek' -and $accounts[2].Password -eq 'Qx7kPm2vRt4z' -and $accounts[2].Note -eq 'znajomy: Janek')
    Set-Fake -Rules @("passwords --raw`t0`taccounts.out") -Files @{ 'accounts.out' = $accountsText }
    $fetched = @(Get-M2VpsAccounts -State $vps)
    Check 'accounts through ssh' ($fetched.Count -eq 3 -and (Get-FakeArgv)[-1] -eq 'sudo -n sh /opt/metin2/linux-port/tools/vps-install.sh passwords --raw')
    Check 'game ports: one channel' ((@(Get-M2VpsGamePorts -AuthPort 11000 -GamePortRange '13000-13002') -join ',') -eq '11000,13000,13001,13002')
    Check 'game ports: two channels, ten apart' ((@(Get-M2VpsGamePorts -AuthPort 11000 -GamePortRange '13000-13012') -join ',') -eq '11000,13000,13001,13002,13010,13011,13012')
    Check 'game ports: a range it cannot read gives the default' ((@(Get-M2VpsGamePorts -AuthPort 11000 -GamePortRange 'x') -join ',') -eq '11000,13000,13001,13002')
    $polish = ([string][char]0x0141) + 'ukasz ' + ([string][char]0x017B) + ([string][char]0x00F3) + ([string][char]0x0142) + ([string][char]0x0107) + ' <3'
    Check 'a friend name in plain letters' ((ConvertTo-M2VpsAsciiName $polish) -eq 'Lukasz Zolc 3') (ConvertTo-M2VpsAsciiName $polish)
    Check 'players connect to the VPS .env address' ((Get-M2VpsWorldAddress -State $vps -Status $status) -eq '203.0.113.7')
    $named = ConvertFrom-M2VpsStatus -Text ($statusText -replace 'public_address=203.0.113.7', 'public_address=')
    Check 'else to the host typed' ((Get-M2VpsWorldAddress -State (New-M2VpsStateObject -From ([pscustomobject]@{ host = 'vps.example.org' })) -Status $named) -eq 'vps.example.org')
    $client = Join-Path $work 'client folder'
    New-Item -ItemType Directory -Path $client | Out-Null
    $entry = Write-M2VpsClientEntry -State $vps -ServerRoot $root -Status $status -ClientFolder $client
    $cfg = [IO.File]::ReadAllText($entry.Path)
    Check 'coop.cfg: the VPS world' ($cfg -match '(?m)^name=Serwer VPS\r?$' -and $cfg -match '(?m)^host=203\.0\.113\.7\r?$' -and $cfg -match '(?m)^auth=11000\r?$' -and $cfg -match '(?m)^channel=13000\r?$' -and $cfg -match '(?m)^channels=2\r?$') $cfg
    Check 'coop.cfg: nothing replaced in a fresh client' ($entry.Replaced -eq '')
    [IO.File]::WriteAllText($entry.Path, "name=Swiat Janka`r`nhost=1.2.3.4`r`nauth=11000`r`nchannel=13000`r`nchannels=1`r`n", [Text.Encoding]::ASCII)
    Check 'coop.cfg: a friend world there before is named' ((Write-M2VpsClientEntry -State $vps -ServerRoot $root -Status $status -ClientFolder $client).Replaced -eq 'Swiat Janka')
    $gate = Join-Path $work 'gate root'
    New-Item -ItemType Directory -Path $gate | Out-Null
    Check 'no invites without the COOP access' (-not (Test-M2VpsInviteAccess -ServerRoot $gate))
    Set-Fake -Rules @("add-account`t0`tfriend.out") -Files @{ 'friend.out' = "login=janek`npassword=Qx7kPm2vRt4z`nsocial_id=1234567`n" }
    Check 'a friend account needs the COOP access' ((Get-ThrownMessage { New-M2VpsFriend -State $vps -ServerRoot $gate -Name 'Janek' }) -match 'patroni')
    Check 'and nothing reached the VPS' (@(Get-FakeCalls).Count -eq 0)
    # The access as the module keeps it after the testers' password: its
    # digest in .m2coop.json (the password itself is in no file).
    $digest = & (Get-Module Metin2Launcher.Coop) { $script:CoopAccessDigest }
    Write-Utf8 (Join-Path $gate '.m2coop.json') ((@{ schema = 1; access = $digest }) | ConvertTo-Json)
    Check 'the COOP access opens the invites' (Test-M2VpsInviteAccess -ServerRoot $gate)
    $friend = New-M2VpsFriend -State $vps -ServerRoot $gate -Name 'Janek!'
    Check 'the friend account is made on the VPS' ((Get-FakeArgv)[-1] -ceq "sudo -n sh /opt/metin2/linux-port/tools/vps-install.sh add-account janek 'znajomy: Janek'") (Get-FakeArgv)[-1]
    Check 'its login and password come back' ($friend.login -eq 'janek' -and $friend.password -eq 'Qx7kPm2vRt4z')
    $code = Get-M2VpsFriendInvite -State $vps -ServerRoot $gate -Account $friend -Status $status
    $invite = Read-M2CoopInvite -Code $code
    Check 'the invite: the VPS address and ports' ($invite.host -eq '203.0.113.7' -and [int]$invite.auth -eq 11000 -and [int]$invite.channel -eq 13000 -and [int]$invite.channels -eq 2)
    Check 'the invite: the friend login, no home address' ($invite.login -eq 'janek' -and $invite.password -eq 'Qx7kPm2vRt4z' -and $invite.lan -eq '' -and $invite.vpn -eq '')
    $fromFile = Get-M2VpsFriendInvite -State $vps -ServerRoot $gate -Account $accounts[2] -Status $status
    Check 'an account read from the VPS file makes the same invite' ((Read-M2CoopInvite -Code $fromFile).login -eq 'janek')
}
finally {
    if ($consoleInput) { try { [Console]::InputEncoding = $consoleInput } catch { } }
    foreach ($tunnelPid in $tunnels) { try { Stop-Process -Id $tunnelPid -Force -ErrorAction Stop } catch { } }
    Remove-Item Env:\M2_FAKE_SSH_DIR -ErrorAction SilentlyContinue
    try { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction Stop } catch { Write-Host ('could not remove ' + $work + ': ' + $_.Exception.Message) }
}

Write-Host ("vps_launcher_test: {0} passed, {1} failed" -f $script:passed, $script:failed)
if ($script:failed) { exit 1 }
