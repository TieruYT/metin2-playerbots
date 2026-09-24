# COOP through a VPN (launcher\Metin2Launcher.Coop.psm1): which adapters are a
# VPN, which way a world is offered, and the invite code, on adapters of
# machines this never ran on. Nothing here touches the network, Docker or the
# router. Run with Windows PowerShell 5.1 - the launcher's own engine:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\coop_vpn_test.ps1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'launcher\Metin2Launcher.Coop.psm1') -Force

$script:failed = 0
$script:passed = 0
function Check {
    param([string]$Name, [bool]$Ok, [string]$Detail = '')
    if ($Ok) { $script:passed++ }
    else { $script:failed++; Write-Host ("FAIL {0} {1}" -f $Name, $Detail) }
}
function Adapter { param($Alias, $Description, $Status, [string[]]$Addresses) [pscustomobject]@{ Alias = $Alias; Description = $Description; Status = $Status; Addresses = $Addresses } }

# ---- adapters: the four products as they name themselves, and what is not one
$adapters = @(
    (Adapter 'Ethernet 2' 'Realtek PCIe 2.5GbE Family Controller' 'Up' @('192.168.1.16')),
    (Adapter 'Hamachi' 'LogMeIn Hamachi Virtual Ethernet Adapter' 'Up' @('25.1.2.3')),
    (Adapter 'ZeroTier One [8056c2e21c000001]' 'ZeroTier Virtual Port' 'Up' @('169.254.3.4', '10.147.17.5')),
    (Adapter 'Tailscale' 'Tailscale Tunnel' 'Up' @('100.101.102.103')),
    (Adapter 'Radmin VPN' 'Famatech Radmin VPN Ethernet Adapter' 'Up' @('26.10.20.30')),
    (Adapter 'Radmin VPN 2' 'Famatech Radmin VPN Ethernet Adapter #2' 'Disconnected' @('26.99.99.99')),
    (Adapter 'ZeroTier One [abc]' 'ZeroTier Virtual Port #2' 'Up' @('169.254.8.8')),
    (Adapter 'vEthernet (WSL)' 'Hyper-V Virtual Ethernet Adapter #2' 'Up' @('192.168.208.1'))
)
$found = @(Select-M2CoopVpnAdapters -Adapters $adapters)
Check 'four VPNs' ($found.Count -eq 4) ('count=' + $found.Count)
Check 'offered in the table order' ((($found | ForEach-Object { $_.Kind }) -join ',') -eq 'radmin,tailscale,zerotier,hamachi')
Check 'radmin address' ($found[0].Address -eq '26.10.20.30')
Check 'link-local skipped' ($found[2].Address -eq '10.147.17.5')
Check 'adapter name kept' ($found[1].Interface -eq 'Tailscale')
$one = @(Select-M2CoopVpnAdapters -Adapters @((Adapter 'Radmin VPN' 'Famatech Radmin VPN Ethernet Adapter' 'Up' @('26.1.1.1'))))
Check 'one VPN is still counted' ($one.Count -eq 1 -and $one[0].Kind -eq 'radmin')
Check 'no adapters' (@(Select-M2CoopVpnAdapters -Adapters @()).Count -eq 0)

# ---- an address that says which VPN it is
$addresses = [ordered]@{ '26.1.2.3' = 'radmin'; '25.1.2.3' = 'hamachi'; '100.64.0.1' = 'tailscale'; '100.127.255.254' = 'tailscale'
    '100.128.0.1' = ''; '192.168.1.2' = ''; 'example.com' = ''; '' = ''; '126.1.2.3' = '' }
foreach ($address in $addresses.Keys) {
    $kind = Get-M2CoopVpnKindForAddress $address
    Check ("address '{0}'" -f $address) ($kind -eq $addresses[$address]) $kind
}

# ---- the way a world is offered
function Report { param($Verdict, [object[]]$Vpns = @()) [pscustomobject]@{ Verdict = $Verdict; Vpns = @($Vpns) } }
$radmin = [pscustomobject]@{ Kind = 'radmin'; Name = 'Radmin VPN'; Address = '26.1.1.1'; Interface = 'Radmin VPN' }
$tailscale = [pscustomobject]@{ Kind = 'tailscale'; Name = 'Tailscale'; Address = '100.100.1.1'; Interface = 'Tailscale' }
$zerotier = [pscustomobject]@{ Kind = 'zerotier'; Name = 'ZeroTier'; Address = '10.147.17.5'; Interface = 'ZeroTier One' }
function Way {
    param($Report, $Requested)
    $way = Resolve-M2CoopHostingVia -Report $Report -Requested $Requested
    if ($way.Vpn) { return ($way.Mode + ':' + $way.Vpn.Kind) }
    return $way.Mode
}
$ways = @(
    @('public', @(), 'auto', 'internet'),
    @('public', @($radmin), 'auto', 'internet'),
    @('no-upnp', @($radmin), 'auto', 'internet'),
    @('mismatch', @($radmin), 'auto', 'internet'),
    @('cgnat', @($radmin), 'auto', 'vpn:radmin'),
    @('double-nat', @($tailscale, $zerotier), 'auto', 'vpn:tailscale'),
    @('cgnat', @(), 'auto', 'blocked'),
    @('cgnat', @($radmin), 'internet', 'blocked'),
    @('public', @($radmin), 'internet', 'internet'),
    @('public', @($radmin, $tailscale), 'radmin', 'vpn:radmin'),
    @('public', @($radmin, $tailscale), 'tailscale', 'vpn:tailscale'),
    @('public', @($zerotier), 'vpn', 'vpn:zerotier'),
    @('cgnat', @($zerotier), '', 'vpn:zerotier')
)
foreach ($w in $ways) {
    $got = Way (Report $w[0] $w[1]) $w[2]
    Check ("{0} with {1} VPN(s), asked {2}" -f $w[0], @($w[1]).Count, $w[2]) ($got -eq $w[3]) $got
}
$threw = ''
try { [void](Resolve-M2CoopHostingVia -Report (Report 'public') -Requested 'radmin') } catch { $threw = $_.Exception.Message }
Check 'a VPN asked for and not here is refused by name' ($threw -match 'Radmin VPN') $threw

# ---- after the router has been asked: nothing opened sends 'auto' to a VPN
$internetWay = [pscustomobject]@{ Mode = 'internet'; Vpn = $null }
$radminWay = [pscustomobject]@{ Mode = 'vpn'; Vpn = $radmin }
function After {
    param($Via, $Requested, [object[]]$Vpns, [int]$Mapped, [int]$Ports)
    $way = Resolve-M2CoopRouterFallback -Via $Via -Requested $Requested -Vpns $Vpns -Mapped $Mapped -Ports $Ports
    if ($way.Vpn) { return ($way.Mode + ':' + $way.Vpn.Kind) }
    return $way.Mode
}
$afters = @(
    @($internetWay, 'auto', @($radmin), 0, 7, 'vpn:radmin'),
    @($internetWay, '', @($tailscale, $radmin), 0, 7, 'vpn:tailscale'),
    @($internetWay, 'auto', @(), 0, 7, 'internet'),
    @($internetWay, 'auto', @($radmin), 3, 7, 'internet'),
    @($internetWay, 'auto', @($radmin), 7, 7, 'internet'),
    @($internetWay, 'internet', @($radmin), 0, 7, 'internet'),
    @($internetWay, 'auto', @($radmin), 0, 0, 'internet'),
    @($radminWay, 'auto', @($radmin), 0, 7, 'vpn:radmin')
)
foreach ($a in $afters) {
    $got = After $a[0] $a[1] $a[2] $a[3] $a[4]
    Check ("router fallback {0}, asked '{1}', {2} VPN(s), {3}/{4} open" -f $a[0].Mode, $a[1], @($a[2]).Count, $a[3], $a[4]) ($got -eq $a[5]) $got
}

# ---- a refusal in words, and what to do about a router
Check 'refusal 606' ((Get-M2CoopUpnpRefusal -Code 606 -HttpStatus 500) -match 'nie pozwala.*606')
Check 'refusal with a code' ((Get-M2CoopUpnpRefusal -Code 402 -HttpStatus 500) -eq 'router odmowil (kod 402)')
Check 'refusal with an HTTP status only' ((Get-M2CoopUpnpRefusal -Code -1 -HttpStatus 500) -eq 'router odmowil (HTTP 500)')
Check 'no answer at all' ((Get-M2CoopUpnpRefusal -Code -1 -HttpStatus 0) -eq 'router nie odpowiedzial')
$fritz = @(Get-M2CoopRouterHelp -Router 'FRITZ! GmbH FRITZ!Box 7530 AX' -LanAddress '192.168.178.54' -Ports @(11000, 13000))
Check 'a FRITZ!Box is told where to allow port sharing' ($fritz[0] -match 'Selbstst' -and $fritz[0] -match '192\.168\.178\.54') ($fritz -join ' | ')
Check 'a FRITZ!Box gets the ports for a manual share' ($fritz[1] -match '11000, 13000')
Check 'any router ends with the VPN' ($fritz[-1] -match 'Radmin VPN')
$other = @(Get-M2CoopRouterHelp -Router 'TP-Link Archer' -LanAddress '192.168.0.10' -Ports @(11000))
Check 'another router is told UPnP or a manual forward' ($other.Count -eq 2 -and $other[0] -match 'UPnP' -and $other[0] -match '11000 na 192\.168\.0\.10') ($other -join ' | ')

# ---- the invite code
$ports = @(11000, 13000, 13001, 13002)
function Decode { param($Code) $b = $Code.Substring(8).Replace('-', '+').Replace('_', '/'); switch ($b.Length % 4) { 2 { $b += '==' } 3 { $b += '=' } }; [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($b)) }
$internet = New-M2CoopInvite -HostAddress '83.10.20.30' -Ports $ports -WorldName 'Swiat' -Login 'kuba' -Password 'abc123XYZ'
Check 'an Internet invite is the 2.0.80 code' ((Decode $internet) -eq '{"v":1,"name":"Swiat","host":"83.10.20.30","auth":11000,"channel":13000,"channels":1,"login":"kuba","password":"abc123XYZ"}') (Decode $internet)
$vpnCode = New-M2CoopInvite -HostAddress '26.10.20.30' -Ports $ports -WorldName 'Swiat' -Login 'kuba' -Password 'abc123XYZ' -Vpn 'radmin'
$read = Read-M2CoopInvite -Code $vpnCode
Check 'a VPN invite names the VPN' ($read.vpn -eq 'radmin' -and $read.host -eq '26.10.20.30')
Check 'an Internet invite reads with an empty vpn' ((Read-M2CoopInvite -Code $internet).vpn -eq '')
$unknown = New-M2CoopInvite -HostAddress '10.0.0.1' -Ports $ports -Login 'a' -Password 'b' -Vpn 'nordvpn'
Check 'an unknown VPN reads as none' ((Read-M2CoopInvite -Code $unknown).vpn -eq '')

# ---- the host's home address in the code, and where a friend's client goes
Check 'an old code reads with an empty lan' ((Read-M2CoopInvite -Code $internet).lan -eq '')
$homeCode = New-M2CoopInvite -HostAddress '209.198.140.24' -Ports $ports -WorldName 'Swiat' -Login 'daro' -Password 'x1y2z3' -Lan '192.168.1.210'
Check 'a code carries the home address' ((Decode $homeCode) -match '"lan":"192\.168\.1\.210"') (Decode $homeCode)
Check 'and reads it back' ((Read-M2CoopInvite -Code $homeCode).lan -eq '192.168.1.210')
foreach ($bad in @('83.10.20.30', '127.0.0.1', '', '100.64.1.1', 'router.local')) {
    $code = New-M2CoopInvite -HostAddress '209.198.140.24' -Ports $ports -Login 'a' -Password 'b' -Lan $bad
    Check ("no lan field for '{0}'" -f $bad) (-not ((Decode $code) -match '"lan"')) (Decode $code)
}
$sameAsHost = New-M2CoopInvite -HostAddress '192.168.1.210' -Ports $ports -Login 'a' -Password 'b' -Lan '192.168.1.210'
Check 'no lan field when it is the host address itself' (-not ((Decode $sameAsHost) -match '"lan"'))

Check 'same /24' (Test-M2CoopSameNetwork -Address '192.168.1.210' -LocalAddresses @('10.5.0.2', '192.168.1.33'))
Check 'another /24' (-not (Test-M2CoopSameNetwork -Address '192.168.1.210' -LocalAddresses @('192.168.0.33', '26.1.1.1')))
Check 'no local addresses' (-not (Test-M2CoopSameNetwork -Address '192.168.1.210' -LocalAddresses @()))
Check 'not an address' (-not (Test-M2CoopSameNetwork -Address 'example.com' -LocalAddresses @('192.168.1.2')))

$script:asked = New-Object System.Collections.Generic.List[string]
# A closure runs in a module of its own, where $script: is not this file's:
# the list goes in as a captured local.
function Probe { param([string[]]$Answering) $log = $script:asked; return { param($address, $port) $log.Add(('{0}:{1}' -f $address, $port)); return ($Answering -contains $address) }.GetNewClosure() }
$invite = Read-M2CoopInvite -Code $homeCode
$script:asked.Clear()
$c = Select-M2CoopJoinHost -Invite $invite -LocalAddresses @('192.168.1.50') -Probe (Probe @('192.168.1.210'))
Check 'same house, home address answers: taken' ($c.Host -eq '192.168.1.210' -and $c.Lan -and $c.Answers) ($c | Out-String)
Check 'and the Internet one is not asked' ($script:asked.Count -eq 1 -and $script:asked[0] -eq '192.168.1.210:11000') ($script:asked -join ',')
$script:asked.Clear()
$c = Select-M2CoopJoinHost -Invite $invite -LocalAddresses @('192.168.1.50') -Probe (Probe @('209.198.140.24'))
Check 'same house, home address silent: the Internet one' ($c.Host -eq '209.198.140.24' -and -not $c.Lan -and $c.SameNetwork -and $c.Answers) ($c | Out-String)
$script:asked.Clear()
$c = Select-M2CoopJoinHost -Invite $invite -LocalAddresses @('192.168.0.7') -Probe (Probe @('192.168.1.210', '209.198.140.24'))
Check 'another network: the home address is never asked' ($c.Host -eq '209.198.140.24' -and -not $c.SameNetwork -and $script:asked.Count -eq 1) ($script:asked -join ',')
$script:asked.Clear()
$c = Select-M2CoopJoinHost -Invite (Read-M2CoopInvite -Code $internet) -LocalAddresses @('192.168.1.50') -Probe (Probe @())
Check 'an old code: the invite address, nothing answering' ($c.Host -eq '83.10.20.30' -and -not $c.Answers -and -not $c.SameNetwork) ($c | Out-String)
$notes = @(Get-M2CoopJoinNotes -Choice ([pscustomobject]@{ Host = '209.198.140.24'; Lan = $false; SameNetwork = $true; Answers = $false; LanAddress = '192.168.1.210' }))
Check 'same house, nothing answering: asks for the firewall' ($notes[0] -match 'zapory' -and $notes[0] -match '192\.168\.1\.210') ($notes -join ' | ')
$notes = @(Get-M2CoopJoinNotes -Choice ([pscustomobject]@{ Host = '192.168.1.210'; Lan = $true; SameNetwork = $true; Answers = $true; LanAddress = '192.168.1.210' }))
Check 'home address taken: says so' ($notes[0] -match 'sieci domowej' -and $notes[0] -match '192\.168\.1\.210') ($notes -join ' | ')

$folder = Join-Path ([IO.Path]::GetTempPath()) ('coop_test_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder | Out-Null
try {
    $cfg = Write-M2CoopClientConfig -ClientFolder $folder -Invite $invite -HostAddress '192.168.1.210'
    $text = [IO.File]::ReadAllText($cfg)
    Check 'coop.cfg takes the chosen address' ($text -match '(?m)^host=192\.168\.1\.210\r?$') $text
    $cfg = Write-M2CoopClientConfig -ClientFolder $folder -Invite $invite
    Check 'coop.cfg keeps the invite address by default' ([IO.File]::ReadAllText($cfg) -match '(?m)^host=209\.198\.140\.24\r?$')
}
finally { Remove-Item -LiteralPath $folder -Recurse -Force }

Write-Host ("coop_vpn_test: {0} passed, {1} failed" -f $script:passed, $script:failed)
if ($script:failed) { exit 1 }
