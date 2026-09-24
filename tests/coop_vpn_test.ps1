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

Write-Host ("coop_vpn_test: {0} passed, {1} failed" -f $script:passed, $script:failed)
if ($script:failed) { exit 1 }
