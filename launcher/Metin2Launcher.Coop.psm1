#requires -Version 5.1
# Co-op over the Internet (experimental; since 2.0.80 hosting is open to the
# Patreon testers behind a password): the host's PC keeps the world, a friend
# runs only the client. What this module does:
#
#   network   the LAN interface with the default route, the address the
#             Internet sees, the router's UPnP gateway and its WAN address -
#             the two addresses differing is how CGNAT and double NAT show;
#   router    TCP mappings of our own, described and leased, for the auth
#             port and every game core's port; a port somebody else mapped
#             is refused, never overwritten or deleted;
#   vpn       Radmin VPN, Tailscale, ZeroTier or Hamachi found on this
#             machine, for a host the Internet cannot reach (CGNAT, a second
#             router): the world is offered at the VPN address instead, and
#             the router is left alone;
#   firewall  one inbound rule for exactly those ports, added only through an
#             administrator's consent (UAC);
#   accounts  a game account for each friend on the host's world, the default
#             admin/test passwords found and refused;
#   invite    one code carrying the host, the ports, the login and password;
#   client    coop.cfg beside the client, which serverinfo.py turns into the
#             second server on the list.
#
# The server itself needs no change: the client enters the world and follows
# every warp at the host the player logged in through (clientify.py), so the
# one address the server names (PROXY_IP) no longer has to fit everybody.
Set-StrictMode -Version 2.0

$script:CoopDescription = 'Metin2 SinglePlayer COOP'
$script:CoopFirewallRule = 'Metin2 SinglePlayer COOP'
$script:CoopLeaseSeconds = 14400
$script:CoopInvitePrefix = 'M2COOP1:'

# Hosting is tried by the Patreon testers first (2.0.80): the COOP window asks
# for their password once and keeps the proof in .m2coop.json, which no update
# touches. Only a digest lives here. It is a gate for testers, not a lock - this
# file is plain text - and a new digest re-locks every install. Joining a
# friend's world needs no password: the invite code is the friend's own key,
# and only somebody who can host can hand one out.
$script:CoopAccessSalt = '7efd8b1a3ea2fc99'
$script:CoopAccessDigest = '75b8d736837c3268d3109b68010047a71e6e841972087efb5ab83f3e13d7f11e'

# ---------------------------------------------------------------- paths/env

function Get-M2CoopStatePath {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    return (Join-Path $ServerRoot '.m2coop.json')
}

function Read-M2CoopState {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $path = Get-M2CoopStatePath -ServerRoot $ServerRoot
    $state = $null
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        try { $state = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json } catch { $state = $null }
    }
    if (-not $state) {
        $state = [pscustomobject]@{ schema = 1; worldName = ''; friends = @(); hosting = $null }
    }
    foreach ($name in @('worldName', 'friends', 'hosting')) {
        if (-not ($state.PSObject.Properties.Name -contains $name)) {
            $state | Add-Member -NotePropertyName $name -NotePropertyValue $null
        }
    }
    if ($null -eq $state.friends) { $state.friends = @() }
    $state.friends = @($state.friends)
    return $state
}

function Save-M2CoopState {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)]$State)
    $path = Get-M2CoopStatePath -ServerRoot $ServerRoot
    $json = $State | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
}

function Get-M2CoopAccessDigest {
    # Spaces and dashes dropped and the case folded, so the password reads the
    # same whether it was typed, pasted from a post or copied with a space.
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Password)
    $normal = ($Password -replace '[\s-]', '').ToLowerInvariant()
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $bytes = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($script:CoopAccessSalt + ':' + $normal)) }
    finally { $sha.Dispose() }
    return -join ($bytes | ForEach-Object { $_.ToString('x2') })
}

function Test-M2CoopAccess {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $state = Read-M2CoopState -ServerRoot $ServerRoot
    if (-not ($state.PSObject.Properties.Name -contains 'access')) { return $false }
    return ([string]$state.access -eq $script:CoopAccessDigest)
}

function Grant-M2CoopAccess {
    # True and remembered when the password is the testers' one; false and
    # nothing written otherwise. The password itself is never stored.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Password)
    if ((Get-M2CoopAccessDigest -Password $Password) -ne $script:CoopAccessDigest) { return $false }
    $state = Read-M2CoopState -ServerRoot $ServerRoot
    if ($state.PSObject.Properties.Name -contains 'access') { $state.access = $script:CoopAccessDigest }
    else { $state | Add-Member -NotePropertyName access -NotePropertyValue $script:CoopAccessDigest }
    Save-M2CoopState -ServerRoot $ServerRoot -State $state
    return $true
}

function Get-M2CoopEnvValue {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Name, [string]$Default = '')
    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { return $Default }
    $value = $Default
    foreach ($line in [IO.File]::ReadAllLines($envPath)) {
        if ($line -match ('^\s*' + [regex]::Escape($Name) + '\s*=(.*)$')) { $value = $Matches[1].Trim().Trim('"') }
    }
    return $value
}

function Get-M2CoopContainerPrefix {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $prefix = Get-M2CoopEnvValue -ServerRoot $ServerRoot -Name 'M2_CONTAINER_PREFIX'
    if (-not $prefix) { $prefix = Get-M2CoopEnvValue -ServerRoot $ServerRoot -Name 'M2_COMPOSE_PROJECT_NAME' -Default 'metin2' }
    return $prefix
}

# The ports a friend's client talks to: auth, then every game core the plan
# runs - three a channel, from M2_GAME_PORT_RANGE (a second channel widens it).
function Get-M2CoopGamePorts {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $auth = [int](Get-M2CoopEnvValue -ServerRoot $ServerRoot -Name 'M2_AUTH_PORT' -Default '11000')
    $range = Get-M2CoopEnvValue -ServerRoot $ServerRoot -Name 'M2_GAME_PORT_RANGE' -Default '13000-13002'
    $ports = New-Object System.Collections.Generic.List[int]
    $ports.Add($auth)
    if ($range -match '^\s*(\d+)\s*-\s*(\d+)\s*$') {
        $from = [int]$Matches[1]; $to = [int]$Matches[2]
        if ($to -ge $from -and ($to - $from) -le 20) {
            foreach ($p in $from..$to) {
                # 13003-13009 sit between two channels' cores and nothing listens there.
                if ((($p - $from) % 10) -le 2) { $ports.Add($p) }
            }
        }
    }
    else { foreach ($p in 13000..13002) { $ports.Add($p) } }
    return @($ports)
}

# ---------------------------------------------------------------- network

function Get-M2CoopLanAddress {
    # WSL, Hyper-V and Docker add adapters of their own, and an SSDP search
    # sent on one of them never reaches the router: on the first machine it was
    # tried on, the router answered only once the socket was bound to the
    # adapter holding the default route.
    $configs = @(Get-NetIPConfiguration -ErrorAction SilentlyContinue | Where-Object {
        $_.IPv4DefaultGateway -and $_.IPv4Address -and $_.NetAdapter -and $_.NetAdapter.Status -eq 'Up'
    })
    # Tailscale's adapter is "Tailscale Tunnel", which none of the older words
    # catch, and with an exit node it carries the default route.
    $physical = @($configs | Where-Object { $_.NetAdapter.InterfaceDescription -notmatch 'Hyper-V|Virtual|WSL|vEthernet|Docker|VPN|TAP|Loopback|Tailscale|ZeroTier|Hamachi|Radmin|WireGuard|Wintun' })
    $pick = $null
    if ($physical.Count -gt 0) { $pick = $physical[0] } elseif ($configs.Count -gt 0) { $pick = $configs[0] }
    if (-not $pick) { return $null }
    return [pscustomobject]@{
        Address   = [string](@($pick.IPv4Address)[0].IPAddress)
        Gateway   = [string](@($pick.IPv4DefaultGateway)[0].NextHop)
        Interface = [string]$pick.InterfaceAlias
    }
}

function Get-M2CoopPublicAddress {
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    } catch { }
    foreach ($url in @('https://api.ipify.org', 'https://ifconfig.me/ip', 'https://icanhazip.com')) {
        try {
            $text = [string](Invoke-RestMethod -Uri $url -TimeoutSec 8 -UserAgent 'curl/8.0' -ErrorAction Stop)
            $text = $text.Trim()
            if ($text -match '^\d{1,3}(\.\d{1,3}){3}$') { return $text }
        } catch { }
    }
    return ''
}

function Test-M2CoopPrivateAddress {
    param([string]$Address)
    if ($Address -notmatch '^(\d+)\.(\d+)\.(\d+)\.(\d+)$') { return $false }
    $a = [int]$Matches[1]; $b = [int]$Matches[2]
    return ($a -eq 10) -or ($a -eq 172 -and $b -ge 16 -and $b -le 31) -or ($a -eq 192 -and $b -eq 168) -or ($a -eq 127)
}

function Test-M2CoopCgnatAddress {
    param([string]$Address)
    if ($Address -notmatch '^(\d+)\.(\d+)\.') { return $false }
    return ([int]$Matches[1] -eq 100 -and [int]$Matches[2] -ge 64 -and [int]$Matches[2] -le 127)
}

function Find-M2CoopGateway {
    param([Parameter(Mandatory = $true)][string]$LanAddress, [int]$TimeoutMs = 3000)
    $locations = New-Object System.Collections.Generic.List[string]
    $local = [Net.IPAddress]::Parse($LanAddress)
    $client = New-Object Net.Sockets.UdpClient (New-Object Net.IPEndPoint ($local, 0))
    try {
        $client.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastInterface, $local.GetAddressBytes())
        $client.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]::MulticastTimeToLive, 2)
        $client.Client.ReceiveTimeout = 700
        $target = New-Object Net.IPEndPoint ([Net.IPAddress]::Parse('239.255.255.250'), 1900)
        foreach ($st in @('urn:schemas-upnp-org:device:InternetGatewayDevice:2', 'urn:schemas-upnp-org:device:InternetGatewayDevice:1')) {
            $msg = "M-SEARCH * HTTP/1.1`r`nHOST: 239.255.255.250:1900`r`nMAN: `"ssdp:discover`"`r`nMX: 2`r`nST: $st`r`n`r`n"
            $bytes = [Text.Encoding]::ASCII.GetBytes($msg)
            [void]$client.Send($bytes, $bytes.Length, $target)
        }
        $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
        while ([DateTime]::UtcNow -lt $deadline) {
            $remote = New-Object Net.IPEndPoint ([Net.IPAddress]::Any, 0)
            try { $data = $client.Receive([ref]$remote) } catch { continue }
            $text = [Text.Encoding]::ASCII.GetString($data)
            if ($text -match '(?im)^location:\s*(\S+)') {
                if (-not $locations.Contains($Matches[1])) { $locations.Add($Matches[1]) }
            }
        }
    }
    finally { $client.Close() }

    foreach ($location in $locations) {
        try { $xml = [string](Invoke-WebRequest -Uri $location -UseBasicParsing -TimeoutSec 5 -ErrorAction Stop).Content } catch { continue }
        $maker = ''; $model = ''
        if ($xml -match '<manufacturer>([^<]*)</manufacturer>') { $maker = $Matches[1] }
        if ($xml -match '<modelName>([^<]*)</modelName>') { $model = $Matches[1] }
        foreach ($service in @('WANIPConnection:2', 'WANIPConnection:1', 'WANPPPConnection:1')) {
            $pattern = '<serviceType>urn:schemas-upnp-org:service:' + [regex]::Escape($service) + '</serviceType>.*?<controlURL>([^<]*)</controlURL>'
            $m = [regex]::Match($xml, $pattern, [Text.RegularExpressions.RegexOptions]::Singleline)
            if ($m.Success) {
                $control = [Uri]::new([Uri]$location, $m.Groups[1].Value).AbsoluteUri
                return [pscustomobject]@{
                    Location     = $location
                    ControlUrl   = $control
                    ServiceType  = 'urn:schemas-upnp-org:service:' + $service
                    Manufacturer = $maker
                    Model        = $model
                }
            }
        }
    }
    return $null
}

function Invoke-M2CoopUpnp {
    param([Parameter(Mandatory = $true)]$Gateway, [Parameter(Mandatory = $true)][string]$Action, [string]$Arguments = '')
    $body = '<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" ' +
        's:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>' +
        '<u:' + $Action + ' xmlns:u="' + $Gateway.ServiceType + '">' + $Arguments + '</u:' + $Action + '>' +
        '</s:Body></s:Envelope>'
    $headers = @{ SOAPAction = ('"{0}#{1}"' -f $Gateway.ServiceType, $Action) }
    try {
        $response = Invoke-WebRequest -Uri $Gateway.ControlUrl -Method Post -Body $body -ContentType 'text/xml; charset="utf-8"' `
            -Headers $headers -UseBasicParsing -TimeoutSec 8 -ErrorAction Stop
        return [pscustomobject]@{ Ok = $true; Code = 0; HttpStatus = 200; Body = [string]$response.Content }
    }
    catch {
        $text = ''
        $status = 0
        $webResponse = $null
        try { $webResponse = $_.Exception.Response } catch { }
        if ($webResponse) {
            try { $status = [int]$webResponse.StatusCode } catch { }
            try {
                $reader = New-Object IO.StreamReader ($webResponse.GetResponseStream())
                $text = $reader.ReadToEnd()
                $reader.Close()
            } catch { }
        }
        $code = -1
        if ($text -match '<errorCode>(\d+)</errorCode>') { $code = [int]$Matches[1] }
        return [pscustomobject]@{ Ok = $false; Code = $code; HttpStatus = $status; Body = $text; Error = $_.Exception.Message }
    }
}

function Get-M2CoopUpnpRefusal {
    # A refused AddPortMapping in words. "kod -1" said only that no UPnP error
    # code came back, and a FRITZ!Box that let this machine do nothing looked
    # the same as one that had not answered at all (Sudak, 24 September).
    param([int]$Code = -1, [int]$HttpStatus = 0)
    switch ($Code) {
        606 { return 'router nie pozwala temu komputerowi na przekierowania (kod 606)' }
        718 { return 'router ma juz ten port dla innego przekierowania (kod 718)' }
        728 { return 'router nie ma miejsca na kolejne przekierowania (kod 728)' }
        729 { return 'router odmowil - konflikt z inna usluga routera (kod 729)' }
    }
    if ($Code -ge 0) { return ('router odmowil (kod {0})' -f $Code) }
    if ($HttpStatus -gt 0) { return ('router odmowil (HTTP {0})' -f $HttpStatus) }
    return 'router nie odpowiedzial'
}

function Get-M2CoopExternalAddress {
    param([Parameter(Mandatory = $true)]$Gateway)
    $r = Invoke-M2CoopUpnp -Gateway $Gateway -Action 'GetExternalIPAddress'
    if ($r.Ok -and $r.Body -match '<NewExternalIPAddress>([^<]*)</NewExternalIPAddress>') { return $Matches[1] }
    return ''
}

function Get-M2CoopPortMapping {
    param([Parameter(Mandatory = $true)]$Gateway, [Parameter(Mandatory = $true)][int]$Port)
    $soapArgs = '<NewRemoteHost></NewRemoteHost><NewExternalPort>' + $Port + '</NewExternalPort><NewProtocol>TCP</NewProtocol>'
    $r = Invoke-M2CoopUpnp -Gateway $Gateway -Action 'GetSpecificPortMappingEntry' -Arguments $soapArgs
    if (-not $r.Ok) { return $null }
    $client = ''; $description = ''; $internal = 0
    if ($r.Body -match '<NewInternalClient>([^<]*)</NewInternalClient>') { $client = $Matches[1] }
    if ($r.Body -match '<NewPortMappingDescription>([^<]*)</NewPortMappingDescription>') { $description = $Matches[1] }
    if ($r.Body -match '<NewInternalPort>(\d+)</NewInternalPort>') { $internal = [int]$Matches[1] }
    return [pscustomobject]@{ Port = $Port; InternalClient = $client; InternalPort = $internal; Description = $description }
}

function Add-M2CoopPortMapping {
    param([Parameter(Mandatory = $true)]$Gateway, [Parameter(Mandatory = $true)][int]$Port,
        [Parameter(Mandatory = $true)][string]$LanAddress, [int]$LeaseSeconds = $script:CoopLeaseSeconds)
    # A port the router already maps for somebody else is left alone: another
    # game, a camera, a second PC. Ours is recognised by its description and
    # renewed.
    $existing = Get-M2CoopPortMapping -Gateway $Gateway -Port $Port
    if ($existing -and -not ($existing.Description -eq $script:CoopDescription -and $existing.InternalClient -eq $LanAddress)) {
        return [pscustomobject]@{ Port = $Port; Ok = $false; Lease = 0; Reason = ('port zajety przez inne przekierowanie ({0} -> {1})' -f $existing.Description, $existing.InternalClient) }
    }
    $lastCode = -1
    $lastStatus = 0
    foreach ($lease in @($LeaseSeconds, 0)) {
        $soapArgs = '<NewRemoteHost></NewRemoteHost><NewExternalPort>' + $Port + '</NewExternalPort><NewProtocol>TCP</NewProtocol>' +
            '<NewInternalPort>' + $Port + '</NewInternalPort><NewInternalClient>' + $LanAddress + '</NewInternalClient>' +
            '<NewEnabled>1</NewEnabled><NewPortMappingDescription>' + $script:CoopDescription + '</NewPortMappingDescription>' +
            '<NewLeaseDuration>' + $lease + '</NewLeaseDuration>'
        $r = Invoke-M2CoopUpnp -Gateway $Gateway -Action 'AddPortMapping' -Arguments $soapArgs
        if ($r.Ok) { return [pscustomobject]@{ Port = $Port; Ok = $true; Lease = $lease; Reason = '' } }
        # 725 OnlyPermanentLeasesSupported is how the specification says "no
        # expiry, please"; routers say it with 402 or 501 just as often, so any
        # refusal of a lease is tried once more without one. A mapping with no
        # expiry is removed by "Zakoncz hostowanie" - or stays until then.
        $lastCode = $r.Code
        $lastStatus = $r.HttpStatus
    }
    return [pscustomobject]@{ Port = $Port; Ok = $false; Lease = 0; Reason = (Get-M2CoopUpnpRefusal -Code $lastCode -HttpStatus $lastStatus) }
}

function Remove-M2CoopPortMapping {
    param([Parameter(Mandatory = $true)]$Gateway, [Parameter(Mandatory = $true)][int]$Port, [string]$LanAddress = '')
    $existing = Get-M2CoopPortMapping -Gateway $Gateway -Port $Port
    if (-not $existing) { return $true }
    # Only our own: the description we write, and our own machine.
    if ($existing.Description -ne $script:CoopDescription) { return $false }
    if ($LanAddress -and $existing.InternalClient -ne $LanAddress) { return $false }
    $soapArgs = '<NewRemoteHost></NewRemoteHost><NewExternalPort>' + $Port + '</NewExternalPort><NewProtocol>TCP</NewProtocol>'
    return [bool](Invoke-M2CoopUpnp -Gateway $Gateway -Action 'DeletePortMapping' -Arguments $soapArgs).Ok
}

function Get-M2CoopNetworkReport {
    $lan = Get-M2CoopLanAddress
    $public = Get-M2CoopPublicAddress
    $gateway = $null; $wan = ''
    if ($lan) { $gateway = Find-M2CoopGateway -LanAddress $lan.Address }
    if ($gateway) { $wan = Get-M2CoopExternalAddress -Gateway $gateway }
    $verdict = 'unknown'
    $text = ''
    if (-not $lan) { $verdict = 'no-lan'; $text = 'Nie znaleziono karty sieciowej z bramą domyślną.' }
    elseif (-not $public) { $verdict = 'offline'; $text = 'Nie udało się odczytać adresu publicznego (brak internetu?).' }
    elseif (Test-M2CoopCgnatAddress $public) { $verdict = 'cgnat'; $text = 'Operator daje adres CGNAT - znajomi nie połączą się bezpośrednio.' }
    elseif (-not $gateway) { $verdict = 'no-upnp'; $text = 'Router nie odpowiada na UPnP - porty trzeba przekierować ręcznie w routerze.' }
    # A router that answers the search and then tells nothing - no address of
    # its own - is either one that lets this machine do nothing (a FRITZ!Box
    # before "Selbstständige Portfreigaben" is allowed for it) or a line with
    # no IPv4 of its own (DS-Lite). Both are the Sudak case of 24 September,
    # where "powinno działać" was said and not one port was opened.
    elseif (-not $wan -or $wan -eq '0.0.0.0') {
        $verdict = 'no-wan'
        $text = 'Router odpowiada na UPnP, ale nie podał swojego adresu w internecie - może nie pozwalać temu komputerowi na przekierowania albo łącze nie ma własnego IPv4 (DS-Lite).'
    }
    elseif (Test-M2CoopCgnatAddress $wan) { $verdict = 'cgnat'; $text = "Router ma adres CGNAT ($wan) - znajomi nie połączą się bezpośrednio." }
    elseif (Test-M2CoopPrivateAddress $wan) { $verdict = 'double-nat'; $text = "Router ma adres prywatny ($wan): przed nim jest drugi router (podwójny NAT)." }
    elseif ($wan -and $wan -ne $public) { $verdict = 'mismatch'; $text = "Router widzi $wan, a internet $public - możliwy CGNAT albo drugi router." }
    else { $verdict = 'public'; $text = 'Publiczny adres IPv4 i UPnP w routerze - hostowanie powinno działać.' }
    return [pscustomobject]@{
        LanAddress    = $(if ($lan) { $lan.Address } else { '' })
        Gateway       = $(if ($lan) { $lan.Gateway } else { '' })
        Interface     = $(if ($lan) { $lan.Interface } else { '' })
        PublicAddress = $public
        RouterWan     = $wan
        Router        = $(if ($gateway) { ('{0} {1}' -f $gateway.Manufacturer, $gateway.Model).Trim() } else { '' })
        GatewayInfo   = $gateway
        Verdict       = $verdict
        Text          = $text
        Vpns          = @(Get-M2CoopVpnAdapters)
    }
}

# ---------------------------------------------------------------- vpn
# A host whose operator hands out no public address (CGNAT - most mobile
# Internet and part of the fibre) or who sits behind a second router cannot be
# reached from the Internet at all, and no setting in the host's own router
# changes that. What does work is a VPN both players join: each of these puts
# a virtual adapter with an address of its own on every member's machine, and
# a friend's client connects to the host's VPN address exactly as it would to
# a public one. Nothing in the router is opened for it, and the tunnel
# encrypts what the game's fixed XTEA key does not.
#
# The adapters as the products name them: "Famatech Radmin VPN Ethernet
# Adapter" (26.x), "Tailscale Tunnel" (100.64.0.0/10), "ZeroTier Virtual
# Port" (whatever the network assigns) and "LogMeIn Hamachi Virtual Ethernet
# Adapter" (25.x). The order is the order they are offered in: Radmin VPN is
# what players here already use for LAN games, Hamachi's free tier takes five.
$script:CoopVpnProducts = @(
    [pscustomobject]@{ Kind = 'radmin'; Name = 'Radmin VPN'; Pattern = 'Radmin' }
    [pscustomobject]@{ Kind = 'tailscale'; Name = 'Tailscale'; Pattern = 'Tailscale' }
    [pscustomobject]@{ Kind = 'zerotier'; Name = 'ZeroTier'; Pattern = 'ZeroTier' }
    [pscustomobject]@{ Kind = 'hamachi'; Name = 'Hamachi'; Pattern = 'Hamachi' }
)

function Get-M2CoopVpnProduct {
    param([AllowEmptyString()][string]$Kind)
    foreach ($product in $script:CoopVpnProducts) { if ($product.Kind -eq $Kind) { return $product } }
    return $null
}

function Select-M2CoopVpnAdapters {
    # Pure: adapter records in (Alias, Description, Status and Addresses, the
    # adapter's IPv4 addresses), the VPNs among them out, in the order of the
    # product table. Apart from Get-NetAdapter so it can be tried on adapters
    # of machines it never ran on - this one has none of the four.
    param([object[]]$Adapters = @())
    $found = New-Object System.Collections.Generic.List[object]
    foreach ($product in $script:CoopVpnProducts) {
        foreach ($adapter in @($Adapters)) {
            if (-not $adapter) { continue }
            if ([string]$adapter.Status -ne 'Up') { continue }
            $label = ([string]$adapter.Alias) + ' ' + ([string]$adapter.Description)
            if ($label -notmatch [regex]::Escape($product.Pattern)) { continue }
            $address = ''
            foreach ($candidate in @($adapter.Addresses)) {
                $text = [string]$candidate
                if ($text -notmatch '^\d{1,3}(\.\d{1,3}){3}$') { continue }
                # An adapter still waiting for its network carries the
                # link-local address Windows gives everything.
                if ($text.StartsWith('169.254.') -or $text.StartsWith('127.')) { continue }
                $address = $text
                break
            }
            if (-not $address) { continue }
            $found.Add([pscustomobject]@{ Kind = $product.Kind; Name = $product.Name; Address = $address; Interface = [string]$adapter.Alias })
        }
    }
    # ToArray, never @($found): Windows PowerShell 5.1 answers @() of a
    # List[object] with "Argument types do not match", empty or not.
    return $found.ToArray()
}

function Get-M2CoopVpnAdapters {
    $records = New-Object System.Collections.Generic.List[object]
    try {
        $addresses = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop)
        foreach ($adapter in @(Get-NetAdapter -ErrorAction Stop)) {
            $mine = @($addresses | Where-Object { $_.InterfaceIndex -eq $adapter.InterfaceIndex } | ForEach-Object { [string]$_.IPAddress })
            $records.Add([pscustomobject]@{ Alias = [string]$adapter.Name; Description = [string]$adapter.InterfaceDescription; Status = [string]$adapter.Status; Addresses = $mine })
        }
    }
    catch { return @() }
    return @(Select-M2CoopVpnAdapters -Adapters $records.ToArray())
}

function Get-M2CoopVpnKindForAddress {
    # Two of the four say who they are by the address alone: Radmin VPN lives
    # in 26.0.0.0/8 and Hamachi in 25.0.0.0/8 (ranges nobody routes on the
    # Internet), and 100.64.0.0/10 in an invite is Tailscale's - a host behind
    # CGNAT is refused before its CGNAT address could reach one. ZeroTier's are
    # ordinary private ranges, so only the invite's own field names it.
    param([AllowEmptyString()][string]$Address)
    if ($Address -match '^26\.\d{1,3}\.\d{1,3}\.\d{1,3}$') { return 'radmin' }
    if ($Address -match '^25\.\d{1,3}\.\d{1,3}\.\d{1,3}$') { return 'hamachi' }
    if (Test-M2CoopCgnatAddress $Address) { return 'tailscale' }
    return ''
}

function Resolve-M2CoopHostingVia {
    # Which way the world is offered: 'internet' (the router's ports, as since
    # 2.0.80) or one of the VPNs in the report. 'auto' keeps the Internet
    # wherever it can work and takes a VPN only where it cannot - so nothing
    # changes for anybody who hosted before. Mode 'blocked' is the one case
    # with no way at all: the Internet cannot reach this host and it has no VPN.
    param([Parameter(Mandatory = $true)]$Report, [AllowEmptyString()][string]$Requested = 'auto')
    $vpns = @($Report.Vpns)
    $want = $(if ($Requested) { $Requested.ToLowerInvariant() } else { 'auto' })
    $unreachable = @('cgnat', 'double-nat') -contains [string]$Report.Verdict
    if ($want -eq 'internet') {
        if ($unreachable) { return [pscustomobject]@{ Mode = 'blocked'; Vpn = $null } }
        return [pscustomobject]@{ Mode = 'internet'; Vpn = $null }
    }
    if ($want -ne 'auto') {
        foreach ($vpn in $vpns) {
            if ($want -eq 'vpn' -or $vpn.Kind -eq $want) { return [pscustomobject]@{ Mode = 'vpn'; Vpn = $vpn } }
        }
        $product = Get-M2CoopVpnProduct -Kind $want
        $name = $(if ($product) { $product.Name } else { 'VPN' })
        throw ("Nie widzę na tym komputerze połączonego {0} - uruchom go, dołącz do sieci i spróbuj jeszcze raz." -f $name)
    }
    if (-not $unreachable) { return [pscustomobject]@{ Mode = 'internet'; Vpn = $null } }
    if ($vpns.Count -gt 0) { return [pscustomobject]@{ Mode = 'vpn'; Vpn = $vpns[0] } }
    return [pscustomobject]@{ Mode = 'blocked'; Vpn = $null }
}

function Resolve-M2CoopRouterFallback {
    # After the router has been asked. A way left to the launcher ('auto')
    # that went to the Internet and got not one port opened goes through a VPN
    # on this machine instead: with no port open nobody from the Internet
    # reaches the world, and an invite carrying the Internet address is a
    # world the friend's client shows offline. Meskele and Sudak, 24
    # September: Radmin VPN connected, a FRITZ!Box that refused all seven
    # ports, and a code with the Internet address - only choosing Radmin by
    # hand made it work. A way the player chose is kept as it is.
    param([Parameter(Mandatory = $true)]$Via, [AllowEmptyString()][string]$Requested = 'auto',
        [object[]]$Vpns = @(), [int]$Mapped = 0, [int]$Ports = 0)
    $want = $(if ($Requested) { $Requested.ToLowerInvariant() } else { 'auto' })
    if ([string]$Via.Mode -ne 'internet' -or $want -ne 'auto' -or $Ports -le 0 -or $Mapped -gt 0) { return $Via }
    $vpns = @($Vpns)
    if ($vpns.Count -eq 0) { return $Via }
    return [pscustomobject]@{ Mode = 'vpn'; Vpn = $vpns[0] }
}

function Get-M2CoopRouterHelp {
    # What to do when the router opened nothing, in the router's own words
    # where it is one people have: a FRITZ!Box answers the search and refuses
    # every mapping until the device is allowed to share ports by itself.
    param([AllowEmptyString()][string]$Router = '', [AllowEmptyString()][string]$LanAddress = '', [int[]]$Ports = @())
    $lines = New-Object System.Collections.Generic.List[string]
    $portText = (@($Ports) -join ', ')
    if ($Router -match 'FRITZ|AVM') {
        $lines.Add(('FRITZ!Box: wejdź na http://fritz.box > Internet > Freigaben > Portfreigaben > Gerät für Freigaben hinzufügen > ten komputer ({0}) > zaznacz Selbstständige Portfreigaben für dieses Gerät erlauben > OK, potem HOSTUJ ŚWIAT jeszcze raz.' -f $LanAddress))
        $lines.Add(('Albo tam samo Neue Freigabe > Portfreigabe: TCP {0} na ten komputer.' -f $portText))
        $lines.Add('W FRITZ!Boxie: Internet > Online-Monitor - jeśli nie ma adresu IPv4 (tylko IPv6 albo DS-Lite), żadne przekierowanie nie zadziała.')
    }
    else {
        $lines.Add(('W ustawieniach routera zezwól temu komputerowi na UPnP (przekierowania portów) albo przekieruj ręcznie TCP {0} na {1}, potem HOSTUJ ŚWIAT jeszcze raz.' -f $portText, $LanAddress))
    }
    $lines.Add('Gdy łącze nie ma własnego adresu IPv4 (DS-Lite, CGNAT), zainstalujcie Radmin VPN, połączcie się w jednej sieci i hostuj przez niego - launcher wybierze go sam.')
    return @($lines.ToArray())
}

function Test-M2CoopHostAnswers {
    # Whether a world's auth server answers from this machine. The server
    # speaks first - its handshake - so bytes read back mean the whole path is
    # open; a bare connection proves nothing through a proxy that accepts it
    # before anything listens behind it (Test-CoopCoreAnswers says the same of
    # Docker Desktop).
    param([Parameter(Mandatory = $true)][string]$HostAddress, [Parameter(Mandatory = $true)][int]$Port, [int]$TimeoutMs = 4000)
    $client = New-Object Net.Sockets.TcpClient
    try {
        $wait = $client.BeginConnect($HostAddress, $Port, $null, $null)
        if (-not ($wait.AsyncWaitHandle.WaitOne($TimeoutMs) -and $client.Connected)) { return $false }
        $client.EndConnect($wait)
        $stream = $client.GetStream()
        $stream.ReadTimeout = $TimeoutMs
        $buffer = New-Object byte[] 16
        return ($stream.Read($buffer, 0, $buffer.Length) -gt 0)
    }
    catch { return $false }
    finally { $client.Close() }
}

# ---------------------------------------------------------------- firewall

function Test-M2CoopFirewallRule {
    $rule = Get-NetFirewallRule -DisplayName $script:CoopFirewallRule -ErrorAction SilentlyContinue
    return [bool]$rule
}

function Add-M2CoopFirewallRule {
    param([Parameter(Mandatory = $true)][int[]]$Ports)
    if (Test-M2CoopFirewallRule) { return $true }
    # The rule is the operating system's; adding it takes the administrator's
    # consent, asked by Windows itself (UAC), never assumed.
    $list = ($Ports | Sort-Object -Unique) -join ','
    $command = "New-NetFirewallRule -DisplayName '$script:CoopFirewallRule' -Direction Inbound -Protocol TCP -LocalPort $list -Action Allow -Profile Any | Out-Null"
    try {
        $p = Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile', '-Command', $command) -Verb RunAs -Wait -PassThru -WindowStyle Hidden
        [void]$p
    } catch { return $false }
    return (Test-M2CoopFirewallRule)
}

function Remove-M2CoopFirewallRule {
    if (-not (Test-M2CoopFirewallRule)) { return $true }
    $command = "Remove-NetFirewallRule -DisplayName '$script:CoopFirewallRule'"
    try {
        $p = Start-Process -FilePath 'powershell.exe' -ArgumentList @('-NoProfile', '-Command', $command) -Verb RunAs -Wait -PassThru -WindowStyle Hidden
        [void]$p
    } catch { return $false }
    return (-not (Test-M2CoopFirewallRule))
}

# ---------------------------------------------------------------- accounts

function Invoke-M2CoopSql {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Query)
    # No double quote may reach docker from Windows PowerShell 5.1 (it ends the
    # argument): the query is its own argument and uses single quotes only.
    if ($Query.Contains('"')) { throw 'Invoke-M2CoopSql: zapytanie nie moze zawierac cudzyslowu' }
    $container = (Get-M2CoopContainerPrefix -ServerRoot $ServerRoot) + '-db'
    # The query goes in on stdin and the root password comes from the
    # container's own MARIADB_ROOT_PASSWORD, so no secret is ever an argument
    # of a Windows process. stderr is dropped rather than redirected into the
    # pipeline: Windows PowerShell 5.1 turns each of its lines into an error
    # record, which stops a caller running with ErrorActionPreference Stop.
    $out = $Query | & docker exec -i $container sh -c 'MYSQL_PWD=$MARIADB_ROOT_PASSWORD exec mariadb -uroot -N -B' 2>$null
    if ($LASTEXITCODE -ne 0) { throw ("Baza w kontenerze {0} nie wykonala zapytania (kod {1})." -f $container, $LASTEXITCODE) }
    return @($out | ForEach-Object { [string]$_ } | Where-Object { $_ -ne '' })
}

function Get-M2CoopHashExpression {
    param([Parameter(Mandatory = $true)][string]$Password)
    # The engine's own hash (mysql5_password): '*' and SHA1 of the raw SHA1.
    return "CONCAT('*', UPPER(SHA1(UNHEX(SHA1('" + $Password + "')))))"
}

function New-M2CoopSecret {
    param([int]$Length = 10)
    $alphabet = 'abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789'.ToCharArray()
    $bytes = New-Object byte[] ($Length)
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    return -join ($bytes | ForEach-Object { $alphabet[$_ % $alphabet.Length] })
}

function Get-M2CoopDefaultPasswordAccounts {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $q = "SELECT login FROM account.account WHERE status='OK' AND ((login='admin' AND password=" + (Get-M2CoopHashExpression 'admin') +
        ") OR (login='test' AND password=" + (Get-M2CoopHashExpression 'test') + "))"
    return @(Invoke-M2CoopSql -ServerRoot $ServerRoot -Query $q | Sort-Object -Unique)
}

function Set-M2CoopAccountPassword {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Login, [Parameter(Mandatory = $true)][string]$Password)
    if ($Login -notmatch '^[A-Za-z0-9]{2,16}$') { throw 'Login: tylko litery i cyfry, 2-16 znakow.' }
    if ($Password -notmatch '^[A-Za-z0-9]{6,16}$') { throw 'Haslo: tylko litery i cyfry, 6-16 znakow.' }
    Invoke-M2CoopSql -ServerRoot $ServerRoot -Query ("UPDATE account.account SET password=" + (Get-M2CoopHashExpression $Password) + " WHERE login='" + $Login + "'") | Out-Null
}

function Set-M2CoopAccountBlocked {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Login, [bool]$Blocked = $true)
    if ($Login -notmatch '^[A-Za-z0-9]{2,16}$') { throw 'Zly login.' }
    $status = $(if ($Blocked) { 'BLOCK' } else { 'OK' })
    Invoke-M2CoopSql -ServerRoot $ServerRoot -Query ("UPDATE account.account SET status='" + $status + "' WHERE login='" + $Login + "'") | Out-Null
}

function New-M2CoopFriend {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Name)
    $base = ($Name.ToLowerInvariant() -replace '[^a-z0-9]', '')
    if ($base.Length -lt 2) { throw 'Nazwa znajomego: co najmniej dwie litery lub cyfry.' }
    if ($base.Length -gt 12) { $base = $base.Substring(0, 12) }
    $login = $base
    $taken = @(Invoke-M2CoopSql -ServerRoot $ServerRoot -Query ("SELECT login FROM account.account WHERE login LIKE '" + $base + "%'"))
    $n = 1
    while ($taken -contains $login) { $n++; $login = $base + $n }
    $password = New-M2CoopSecret -Length 10
    $socialBytes = New-Object byte[] 4
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($socialBytes)
    $social = '{0:D7}' -f ([BitConverter]::ToUInt32($socialBytes, 0) % 10000000)
    Invoke-M2CoopSql -ServerRoot $ServerRoot -Query ("INSERT INTO account.account (login, password, social_id, status) VALUES ('" +
        $login + "', " + (Get-M2CoopHashExpression $password) + ", '" + $social + "', 'OK')") | Out-Null
    $state = Read-M2CoopState -ServerRoot $ServerRoot
    $friend = [pscustomobject]@{ name = $Name; login = $login; password = $password; socialId = $social; created = (Get-Date).ToString('s'); blocked = $false }
    $state.friends = @(@($state.friends) + $friend)
    Save-M2CoopState -ServerRoot $ServerRoot -State $state
    return $friend
}

function Get-M2CoopFirewallBlocks {
    # A block rule beats every allow rule in Windows Firewall, and answering
    # "Anuluj" once to the question Windows asks about Docker leaves exactly
    # such rules for its programs - after which no rule for the ports helps.
    # Read-only: the rules are named, never changed.
    $found = @()
    try {
        $filters = @(Get-NetFirewallApplicationFilter -All -ErrorAction Stop | Where-Object { [string]$_.Program -match 'docker|wslrelay|vpnkit' })
        foreach ($filter in $filters) {
            foreach ($rule in @($filter | Get-NetFirewallRule -ErrorAction SilentlyContinue)) {
                if ([string]$rule.Direction -eq 'Inbound' -and [string]$rule.Action -eq 'Block' -and [string]$rule.Enabled -eq 'True') {
                    $found += [pscustomobject]@{ Name = [string]$rule.DisplayName; Program = [string]$filter.Program; Profile = [string]$rule.Profile }
                }
            }
        }
    } catch { }
    return $found
}

# ---------------------------------------------------------------- hosting

function Get-M2CoopGameBindings {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    # What Docker publishes for the game container right now. That, not .env
    # and not the state file, is whether a friend can reach the world.
    $container = (Get-M2CoopContainerPrefix -ServerRoot $ServerRoot) + '-game'
    $lines = @(& docker port $container 2>$null | ForEach-Object { [string]$_ } | Where-Object { $_ -match '->' })
    $open = @($lines | Where-Object { $_ -match '->\s*(0\.0\.0\.0|\[::\]):' })
    return [pscustomobject]@{ Running = ($lines.Count -gt 0); Public = ($open.Count -gt 0); Lines = $lines }
}

function Set-M2CoopEnvValue {
    # One key of .env replaced in place or appended; nothing else in the file
    # is touched. The value is a literal, so a $ in it is escaped for Replace.
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value)
    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) { throw "Brak pliku .env: $envPath" }
    $content = [IO.File]::ReadAllText($envPath)
    $pattern = '(?m)^' + [regex]::Escape($Name) + '=[^\r\n]*'
    $line = $Name + '=' + $Value
    if ([regex]::IsMatch($content, $pattern)) { $content = [regex]::Replace($content, $pattern, $line.Replace('$', '$$')) }
    else {
        if ($content -and -not $content.EndsWith("`n")) { $content += "`r`n" }
        $content += $line + "`r`n"
    }
    [IO.File]::WriteAllText($envPath, $content, [Text.UTF8Encoding]::new($false))
}

function Protect-M2CoopAccounts {
    # Every copy of this server ships admin/admin (the GM) and test/test. Both
    # get a password of their own while they still carry the shipped one, and
    # the state file keeps it, so nobody is locked out of a character.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $ServerRoot)
    $changed = [ordered]@{}
    foreach ($login in $defaults) {
        $password = New-M2CoopSecret -Length 10
        Set-M2CoopAccountPassword -ServerRoot $ServerRoot -Login $login -Password $password
        $changed[$login] = $password
    }
    if ($changed.Count -gt 0) {
        $state = Read-M2CoopState -ServerRoot $ServerRoot
        $accounts = [ordered]@{}
        if ($state.PSObject.Properties.Name -contains 'accounts' -and $state.accounts) {
            foreach ($p in $state.accounts.PSObject.Properties) { $accounts[$p.Name] = $p.Value }
        }
        foreach ($k in $changed.Keys) { $accounts[$k] = $changed[$k] }
        if ($state.PSObject.Properties.Name -contains 'accounts') { $state.accounts = [pscustomobject]$accounts }
        else { $state | Add-Member -NotePropertyName accounts -NotePropertyValue ([pscustomobject]$accounts) }
        Save-M2CoopState -ServerRoot $ServerRoot -State $state
    }
    return [pscustomobject]$changed
}

function Set-M2CoopFriendBlocked {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)][string]$Login, [bool]$Blocked = $true)
    Set-M2CoopAccountBlocked -ServerRoot $ServerRoot -Login $Login -Blocked $Blocked
    $state = Read-M2CoopState -ServerRoot $ServerRoot
    foreach ($f in @($state.friends)) { if ([string]$f.login -eq $Login) { $f.blocked = $Blocked } }
    Save-M2CoopState -ServerRoot $ServerRoot -State $state
}

function Get-M2CoopWorldName {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $state = Read-M2CoopState -ServerRoot $ServerRoot
    $name = [string]$state.worldName
    if (-not $name) { $name = 'Swiat ' + ($env:USERNAME -replace '[^A-Za-z0-9 ]', '') }
    return $name
}

function Get-M2CoopFriendInvite {
    param([Parameter(Mandatory = $true)][string]$ServerRoot, [Parameter(Mandatory = $true)]$Friend,
        [Parameter(Mandatory = $true)][string]$HostAddress, [AllowEmptyString()][string]$Vpn = '',
        [AllowEmptyString()][string]$Lan = '')
    $ports = Get-M2CoopGamePorts -ServerRoot $ServerRoot
    return (New-M2CoopInvite -HostAddress $HostAddress -Ports $ports -WorldName (Get-M2CoopWorldName -ServerRoot $ServerRoot) `
            -Login ([string]$Friend.login) -Password ([string]$Friend.password) -Vpn $Vpn -Lan $Lan)
}

function Get-M2CoopInviteTarget {
    # The address a friend's client is sent to: the host's VPN address when
    # the world was last hosted through a VPN, the address the Internet sees
    # otherwise. The VPN's address is read again rather than trusted from the
    # state file, and the stored one stands in only while the VPN is off - a
    # Radmin or Tailscale address stays with the machine. Address is empty when
    # neither could be read.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $state = Read-M2CoopState -ServerRoot $ServerRoot
    $hosting = $state.hosting
    $names = @()
    if ($hosting) { $names = @($hosting.PSObject.Properties.Name) }
    # The host's address in its own network goes with every code: a friend
    # in the same house - the second PC, the laptop on the same Wi-Fi - is
    # sent there instead (Select-M2CoopJoinHost). Read afresh, the stored one
    # standing in only while no adapter answers.
    $lan = ''
    $lanNow = Get-M2CoopLanAddress
    if ($lanNow) { $lan = [string]$lanNow.Address }
    if (-not $lan -and ($names -contains 'lanAddress')) { $lan = [string]$hosting.lanAddress }
    if (($names -contains 'mode') -and [string]$hosting.mode -eq 'vpn' -and ($names -contains 'vpn')) {
        $kind = [string]$hosting.vpn
        $address = ''
        foreach ($vpn in @(Get-M2CoopVpnAdapters)) { if ($vpn.Kind -eq $kind) { $address = $vpn.Address; break } }
        if (-not $address -and ($names -contains 'friendAddress')) { $address = [string]$hosting.friendAddress }
        $product = Get-M2CoopVpnProduct -Kind $kind
        return [pscustomobject]@{ Address = $address; Vpn = $kind; VpnName = $(if ($product) { $product.Name } else { $kind }); Lan = $lan }
    }
    return [pscustomobject]@{ Address = (Get-M2CoopPublicAddress); Vpn = ''; VpnName = ''; Lan = $lan }
}

function Get-M2CoopJoinAdvice {
    # What the friend's own machine lacks to reach a world, as far as it can
    # tell: for a world offered through a VPN, that VPN connected here. Empty
    # when nothing is missing that this machine can see.
    param([Parameter(Mandatory = $true)]$Invite)
    $kind = ''
    if ($Invite.PSObject.Properties.Name -contains 'vpn') { $kind = [string]$Invite.vpn }
    if (-not $kind) { $kind = Get-M2CoopVpnKindForAddress ([string]$Invite.host) }
    if (-not $kind) { return '' }
    foreach ($vpn in @(Get-M2CoopVpnAdapters)) { if ($vpn.Kind -eq $kind) { return '' } }
    $product = Get-M2CoopVpnProduct -Kind $kind
    $name = $(if ($product) { $product.Name } else { 'VPN' })
    return ("Świat znajomego jest dostępny przez {0}. Zainstaluj {0} i dołącz do sieci znajomego (jak się nazywa i jakie ma hasło, powie Ci znajomy) - bez tego gra się nie połączy." -f $name)
}

# ---------------------------------------------------------------- invite

function Test-M2CoopLanInviteAddress {
    # An address worth sending a friend in the host's own network to: a
    # private IPv4 of 10/8, 172.16/12 or 192.168/16, not the loopback.
    param([AllowEmptyString()][string]$Address)
    return ($Address -and $Address -notmatch '^127\.' -and (Test-M2CoopPrivateAddress $Address))
}

function New-M2CoopInvite {
    param([Parameter(Mandatory = $true)][string]$HostAddress, [Parameter(Mandatory = $true)][int[]]$Ports,
        [string]$WorldName = '', [string]$Login = '', [string]$Password = '', [AllowEmptyString()][string]$Vpn = '',
        [AllowEmptyString()][string]$Lan = '')
    $auth = $Ports[0]
    $game = @($Ports | Select-Object -Skip 1)
    $channels = [Math]::Max(1, [int][Math]::Ceiling(($game | Measure-Object).Count / 3.0))
    $payload = [ordered]@{
        v = 1; name = $WorldName; host = $HostAddress; auth = $auth
        channel = $(if ($game.Count -gt 0) { $game[0] } else { 13000 }); channels = $channels
        login = $Login; password = $Password
    }
    # Only a VPN invite carries the field, so an Internet invite is the same
    # code it was in 2.0.80, and a reader that does not know the field - the
    # client's Dolacz.ps1 of client 2.0.17 - skips it.
    if ($Vpn) { $payload['vpn'] = $Vpn }
    # The host's address in its own network, for a friend in the same house.
    # The Internet address reaches the host from there only through a router
    # that forwards the ports and loops a connection to its own address back
    # in, and many do neither: xXxDaronxXx (24 September) tested with his own
    # laptop, the router answered no UPnP, and not one connection reached the
    # auth core in half an hour. Readers older than the field skip it.
    if ((Test-M2CoopLanInviteAddress $Lan) -and $Lan -ne $HostAddress) { $payload['lan'] = $Lan }
    $json = $payload | ConvertTo-Json -Compress
    $b64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($json)).TrimEnd('=').Replace('+', '-').Replace('/', '_')
    return $script:CoopInvitePrefix + $b64
}

function Read-M2CoopInvite {
    param([Parameter(Mandatory = $true)][string]$Code)
    $text = $Code.Trim()
    if (-not $text.StartsWith($script:CoopInvitePrefix)) { throw 'To nie jest kod zaproszenia COOP (powinien zaczynac sie od M2COOP1:).' }
    $b64 = $text.Substring($script:CoopInvitePrefix.Length).Replace('-', '+').Replace('_', '/')
    switch ($b64.Length % 4) { 2 { $b64 += '==' } 3 { $b64 += '=' } }
    try { $json = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($b64)) } catch { throw 'Kod zaproszenia jest uszkodzony.' }
    $invite = $json | ConvertFrom-Json
    if ([string]$invite.host -notmatch '^[A-Za-z0-9.-]{1,253}$') { throw 'Kod zaproszenia: zly adres hosta.' }
    foreach ($field in @('auth', 'channel')) {
        $v = [int]$invite.$field
        if ($v -le 0 -or $v -ge 65536) { throw "Kod zaproszenia: zly port ($field)." }
    }
    # Every invite leaves here with a vpn field, empty for an Internet one, so
    # a caller under StrictMode can read it; an unknown product counts as none.
    $vpn = ''
    if ($invite.PSObject.Properties.Name -contains 'vpn') {
        $vpn = [string]$invite.vpn
        if (-not (Get-M2CoopVpnProduct -Kind $vpn)) { $vpn = '' }
        $invite.vpn = $vpn
    }
    else { $invite | Add-Member -NotePropertyName vpn -NotePropertyValue $vpn }
    # And a lan field, empty unless it holds a private IPv4.
    $lan = ''
    if ($invite.PSObject.Properties.Name -contains 'lan') {
        $lan = [string]$invite.lan
        if (-not (Test-M2CoopLanInviteAddress $lan)) { $lan = '' }
        $invite.lan = $lan
    }
    else { $invite | Add-Member -NotePropertyName lan -NotePropertyValue $lan }
    return $invite
}

function Test-M2CoopSameNetwork {
    # Whether this machine stands in the network an address belongs to, as a
    # home network is laid out: the first three numbers the same (a /24).
    # Pure, so it can be tried on addresses of machines it never ran on.
    param([AllowEmptyString()][string]$Address, [string[]]$LocalAddresses = @())
    if ($Address -notmatch '^(\d{1,3}\.\d{1,3}\.\d{1,3})\.\d{1,3}$') { return $false }
    $prefix = $Matches[1] + '.'
    foreach ($local in @($LocalAddresses)) { if (([string]$local).StartsWith($prefix)) { return $true } }
    return $false
}

function Select-M2CoopJoinHost {
    # Where a friend's client is sent: the host's address in its own network
    # when the invite carries one, this machine stands in that network and
    # the world answers there; the invite's own address otherwise. Pure but
    # for the probe, a scriptblock of (address, port) answering whether the
    # world's auth speaks there. The home address is asked only in the same
    # network: plenty of home networks are 192.168.1.x, and a friend's own
    # would be asked about a machine that is not there.
    param([Parameter(Mandatory = $true)]$Invite, [string[]]$LocalAddresses = @(),
        [Parameter(Mandatory = $true)][scriptblock]$Probe)
    $lan = ''
    if ($Invite.PSObject.Properties.Name -contains 'lan') { $lan = [string]$Invite.lan }
    $auth = [int]$Invite.auth
    $same = [bool]((Test-M2CoopLanInviteAddress $lan) -and (Test-M2CoopSameNetwork -Address $lan -LocalAddresses $LocalAddresses))
    if ($same -and [bool](& $Probe $lan $auth)) {
        return [pscustomobject]@{ Host = $lan; Lan = $true; SameNetwork = $true; Answers = $true; LanAddress = $lan }
    }
    $answers = [bool](& $Probe ([string]$Invite.host) $auth)
    return [pscustomobject]@{ Host = [string]$Invite.host; Lan = $false; SameNetwork = $same; Answers = $answers; LanAddress = $lan }
}

function Get-M2CoopLocalAddresses {
    # This machine's IPv4 addresses, the loopback and link-local left out.
    try {
        return @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop | ForEach-Object { [string]$_.IPAddress } |
            Where-Object { $_ -and $_ -notmatch '^(127\.|169\.254\.)' })
    }
    catch { return @() }
}

function Resolve-M2CoopJoinHost {
    param([Parameter(Mandatory = $true)]$Invite)
    $probe = { param($address, $port) Test-M2CoopHostAnswers -HostAddress $address -Port $port -TimeoutMs 2500 }
    return (Select-M2CoopJoinHost -Invite $Invite -LocalAddresses @(Get-M2CoopLocalAddresses) -Probe $probe)
}

function Get-M2CoopJoinNotes {
    # What the joining side says after the join, in its own words: the home
    # network taken, the world answering, or what to ask the host for.
    param([Parameter(Mandatory = $true)]$Choice)
    $notes = @()
    if ($Choice.Lan) { $notes += ('Jesteś w tej samej sieci domowej co host - gra połączy się przez jego adres w tej sieci ({0}), bez routera.' -f $Choice.Host) }
    elseif ($Choice.Answers) { $notes += 'Serwer znajomego odpowiada.' }
    elseif ($Choice.SameNetwork) {
        $notes += ('Jesteś w tej samej sieci domowej co host ({0}), ale jego serwer tu nie odpowiada. Host musi mieć włączone hostowanie i pozwolić Windows na regułę zapory (HOSTUJ ŚWIAT, w okienku Windows "Tak"). Potem wklej kod jeszcze raz.' -f $Choice.LanAddress)
    }
    else { $notes += 'Serwer znajomego teraz nie odpowiada - poproś, żeby uruchomił serwer (GRAJ) i włączył hostowanie.' }
    return $notes
}

function Get-M2CoopClientFolder {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)
    $config = Join-Path $ServerRoot '.m2launcher.json'
    if (Test-Path -LiteralPath $config -PathType Leaf) {
        try {
            $c = Get-Content -LiteralPath $config -Raw -Encoding UTF8 | ConvertFrom-Json
            foreach ($name in @('clientRoot', 'clientExecutable')) {
                if ($c.PSObject.Properties.Name -contains $name -and $c.$name) {
                    $p = [string]$c.$name
                    if ($name -eq 'clientExecutable') { $p = Split-Path -Parent $p }
                    if (Test-Path -LiteralPath $p -PathType Container) { return $p }
                }
            }
        } catch { }
    }
    foreach ($candidate in @((Join-Path (Split-Path -Parent $ServerRoot) 'Klient'), (Join-Path (Split-Path -Parent $ServerRoot) 'Client'))) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'metin2client.exe') -PathType Leaf) { return $candidate }
    }
    return ''
}

function Write-M2CoopClientConfig {
    # HostAddress is where the client goes when it is not the invite's own
    # address: the host's home one (Select-M2CoopJoinHost).
    param([Parameter(Mandatory = $true)][string]$ClientFolder, [Parameter(Mandatory = $true)]$Invite,
        [AllowEmptyString()][string]$HostAddress = '')
    # The client reads coop.cfg as ASCII; a Polish letter becomes its plain
    # one rather than vanishing ("Swiat", not "wiat").
    $plain = [string]$Invite.name
    $pairs = @{ [char]0x0105 = 'a'; [char]0x0107 = 'c'; [char]0x0119 = 'e'; [char]0x0142 = 'l'; [char]0x0144 = 'n'; [char]0x00F3 = 'o'
        [char]0x015B = 's'; [char]0x017A = 'z'; [char]0x017C = 'z'; [char]0x0104 = 'A'; [char]0x0106 = 'C'; [char]0x0118 = 'E'
        [char]0x0141 = 'L'; [char]0x0143 = 'N'; [char]0x00D3 = 'O'; [char]0x015A = 'S'; [char]0x0179 = 'Z'; [char]0x017B = 'Z' }
    foreach ($k in $pairs.Keys) { $plain = $plain.Replace([string]$k, $pairs[$k]) }
    $name = ($plain -replace '[^\x20-\x7e]', '')
    if (-not $name) { $name = [string]$Invite.host }
    $target = $(if ($HostAddress) { $HostAddress } else { [string]$Invite.host })
    $lines = @(
        '# Metin2 SinglePlayer - swiat znajomego (zapisal launcher, kod zaproszenia)',
        ('name=' + $name),
        ('host=' + $target),
        ('auth=' + [int]$Invite.auth),
        ('channel=' + [int]$Invite.channel),
        ('channels=' + [int]$Invite.channels)
    )
    $path = Join-Path $ClientFolder 'coop.cfg'
    [IO.File]::WriteAllText($path, (($lines -join "`r`n") + "`r`n"), [Text.Encoding]::ASCII)
    return $path
}

Export-ModuleMember -Function Get-M2CoopStatePath, Read-M2CoopState, Save-M2CoopState, Get-M2CoopEnvValue, Get-M2CoopContainerPrefix,
    Get-M2CoopGamePorts, Get-M2CoopLanAddress, Get-M2CoopPublicAddress, Test-M2CoopPrivateAddress, Test-M2CoopCgnatAddress,
    Find-M2CoopGateway, Invoke-M2CoopUpnp, Get-M2CoopExternalAddress, Get-M2CoopPortMapping, Add-M2CoopPortMapping,
    Remove-M2CoopPortMapping, Get-M2CoopNetworkReport, Test-M2CoopFirewallRule, Add-M2CoopFirewallRule, Remove-M2CoopFirewallRule,
    Invoke-M2CoopSql, Get-M2CoopHashExpression, New-M2CoopSecret, Get-M2CoopDefaultPasswordAccounts, Set-M2CoopAccountPassword,
    Set-M2CoopAccountBlocked, New-M2CoopFriend, New-M2CoopInvite, Read-M2CoopInvite, Get-M2CoopClientFolder, Write-M2CoopClientConfig,
    Get-M2CoopFirewallBlocks, Get-M2CoopGameBindings, Set-M2CoopEnvValue, Protect-M2CoopAccounts, Set-M2CoopFriendBlocked,
    Get-M2CoopWorldName, Get-M2CoopFriendInvite, Get-M2CoopAccessDigest, Test-M2CoopAccess, Grant-M2CoopAccess,
    Get-M2CoopVpnProduct, Select-M2CoopVpnAdapters, Get-M2CoopVpnAdapters, Get-M2CoopVpnKindForAddress, Resolve-M2CoopHostingVia,
    Test-M2CoopHostAnswers, Get-M2CoopInviteTarget, Get-M2CoopJoinAdvice,
    Get-M2CoopUpnpRefusal, Resolve-M2CoopRouterFallback, Get-M2CoopRouterHelp,
    Test-M2CoopLanInviteAddress, Test-M2CoopSameNetwork, Select-M2CoopJoinHost, Get-M2CoopLocalAddresses,
    Resolve-M2CoopJoinHost, Get-M2CoopJoinNotes
