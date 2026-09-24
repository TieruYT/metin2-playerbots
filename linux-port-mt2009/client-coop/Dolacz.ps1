# Metin2 SinglePlayer - dołączenie do świata znajomego (COOP, eksperymentalne).
#
# Znajomy, który hostuje świat, daje kod zaproszenia (M2COOP1:...). Ten plik
# leży w folderze klienta, zapisuje obok niego coop.cfg i nic więcej: klient
# czyta go przy starcie (serverinfo.py) i pokazuje świat znajomego jako drugi
# serwer na liście, "Online: <nazwa>". Kod niesie też login i hasło konta,
# które znajomy założył w swoim świecie - pokazujemy je, nigdzie nie zapisujemy.
#
# Samodzielny: na komputerze gracza, który tylko dołącza, nie ma serwera ani
# launchera, więc ten plik nie importuje niczego. Kod zaproszenia czyta tak
# samo jak launcher\Metin2Launcher.Coop.psm1 (Read-M2CoopInvite) i zapisuje
# coop.cfg tak samo jak Write-M2CoopClientConfig; świat w sieci VPN (Radmin VPN,
# Tailscale, ZeroTier, Hamachi) rozpoznaje jak Get-M2CoopJoinAdvice, a to, czy
# serwer znajomego odpowiada, sprawdza jak Test-M2CoopHostAnswers. W tej samej
# sieci domowej co host (kod z launchera od 2.2.11 niesie jego adres w tej
# sieci) wybiera ten adres, jak Select-M2CoopJoinHost.
param([string]$Invite = '', [switch]$NoWindow)
$ErrorActionPreference = 'Stop'
$clientDir = $PSScriptRoot
$prefix = 'M2COOP1:'

function Read-CoopInvite {
    param([Parameter(Mandatory = $true)][string]$Code)
    $text = ($Code -replace '\s', '')
    if (-not $text.StartsWith($prefix)) { throw 'To nie jest kod zaproszenia (powinien zaczynać się od M2COOP1:).' }
    $b64 = $text.Substring($prefix.Length).Replace('-', '+').Replace('_', '/')
    switch ($b64.Length % 4) { 2 { $b64 += '==' } 3 { $b64 += '=' } }
    try { $json = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($b64)) }
    catch { throw 'Kod zaproszenia jest uszkodzony - skopiuj go jeszcze raz w całości.' }
    try { $invite = $json | ConvertFrom-Json } catch { throw 'Kod zaproszenia jest uszkodzony - skopiuj go jeszcze raz w całości.' }
    if ([string]$invite.host -notmatch '^[A-Za-z0-9.-]{1,253}$') { throw 'Kod zaproszenia: zły adres serwera.' }
    foreach ($field in @('auth', 'channel')) {
        $v = [int]$invite.$field
        if ($v -le 0 -or $v -ge 65536) { throw "Kod zaproszenia: zły port ($field)." }
    }
    return $invite
}

function Write-CoopConfig {
    # HostAddress: where the client goes when it is not the invite's own
    # address - the host's home one (Select-CoopJoinHost).
    param([Parameter(Mandatory = $true)]$Invite, [string]$HostAddress = '')
    # The client reads coop.cfg as ASCII; a Polish letter becomes its plain
    # one rather than vanishing ("Swiat", not "wiat").
    $plain = [string]$Invite.name
    $pairs = @{ [char]0x0105 = 'a'; [char]0x0107 = 'c'; [char]0x0119 = 'e'; [char]0x0142 = 'l'; [char]0x0144 = 'n'; [char]0x00F3 = 'o'
        [char]0x015B = 's'; [char]0x017A = 'z'; [char]0x017C = 'z'; [char]0x0104 = 'A'; [char]0x0106 = 'C'; [char]0x0118 = 'E'
        [char]0x0141 = 'L'; [char]0x0143 = 'N'; [char]0x00D3 = 'O'; [char]0x015A = 'S'; [char]0x0179 = 'Z'; [char]0x017B = 'Z' }
    foreach ($k in $pairs.Keys) { $plain = $plain.Replace([string]$k, $pairs[$k]) }
    $name = ($plain -replace '[^\x20-\x7e]', '')
    if (-not $name) { $name = [string]$Invite.host }
    $channels = [int]$Invite.channels
    if ($channels -lt 1) { $channels = 1 }
    $target = $(if ($HostAddress) { $HostAddress } else { [string]$Invite.host })
    $lines = @(
        '# Metin2 SinglePlayer - swiat znajomego (zapisal Dolacz.ps1, kod zaproszenia)',
        ('name=' + $name),
        ('host=' + $target),
        ('auth=' + [int]$Invite.auth),
        ('channel=' + [int]$Invite.channel),
        ('channels=' + $channels)
    )
    $path = Join-Path $clientDir 'coop.cfg'
    [IO.File]::WriteAllText($path, (($lines -join "`r`n") + "`r`n"), [Text.Encoding]::ASCII)
    return $path
}

# The world may be offered at an address in a VPN both players are in, for a
# host the Internet cannot reach. The invite names the VPN (a launcher from
# 2.0.82 writes it); an older code still gives Radmin VPN and Hamachi away by
# the address, and 100.64.0.0/10 in an invite is Tailscale's.
$vpnNames = @{ radmin = 'Radmin VPN'; tailscale = 'Tailscale'; zerotier = 'ZeroTier'; hamachi = 'Hamachi' }
$vpnPatterns = @{ radmin = 'Radmin'; tailscale = 'Tailscale'; zerotier = 'ZeroTier'; hamachi = 'Hamachi' }

function Get-CoopInviteVpn {
    param([Parameter(Mandatory = $true)]$Invite)
    $kind = ''
    if ($Invite.PSObject.Properties.Name -contains 'vpn') { $kind = [string]$Invite.vpn }
    if (-not $vpnNames.ContainsKey($kind)) { $kind = '' }
    if (-not $kind) {
        $address = [string]$Invite.host
        if ($address -match '^26\.\d{1,3}\.\d{1,3}\.\d{1,3}$') { $kind = 'radmin' }
        elseif ($address -match '^25\.\d{1,3}\.\d{1,3}\.\d{1,3}$') { $kind = 'hamachi' }
        elseif ($address -match '^100\.(\d{1,3})\.\d{1,3}\.\d{1,3}$' -and [int]$Matches[1] -ge 64 -and [int]$Matches[1] -le 127) { $kind = 'tailscale' }
    }
    return $kind
}

function Test-CoopVpnHere {
    # That VPN's adapter up on this machine, with an address of its own.
    param([Parameter(Mandatory = $true)][string]$Kind)
    try {
        $addresses = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop)
        foreach ($adapter in @(Get-NetAdapter -ErrorAction Stop)) {
            if ([string]$adapter.Status -ne 'Up') { continue }
            if ((([string]$adapter.Name) + ' ' + ([string]$adapter.InterfaceDescription)) -notmatch $vpnPatterns[$Kind]) { continue }
            foreach ($a in @($addresses | Where-Object { $_.InterfaceIndex -eq $adapter.InterfaceIndex })) {
                if (-not ([string]$a.IPAddress).StartsWith('169.254.')) { return $true }
            }
        }
    }
    catch { return $true }
    return $false
}

function Test-CoopHostAnswers {
    # The server speaks first (its handshake), so bytes read back mean the
    # whole way to the friend's world is open.
    param([Parameter(Mandatory = $true)][string]$HostAddress, [Parameter(Mandatory = $true)][int]$Port)
    $client = New-Object Net.Sockets.TcpClient
    try {
        $wait = $client.BeginConnect($HostAddress, $Port, $null, $null)
        if (-not ($wait.AsyncWaitHandle.WaitOne(4000) -and $client.Connected)) { return $false }
        $client.EndConnect($wait)
        $stream = $client.GetStream()
        $stream.ReadTimeout = 4000
        $buffer = New-Object byte[] 16
        return ($stream.Read($buffer, 0, $buffer.Length) -gt 0)
    }
    catch { return $false }
    finally { $client.Close() }
}

function Get-CoopInviteLan {
    # The host's address in its own network (a launcher from 2.2.11 writes
    # it): a private IPv4, or nothing.
    param([Parameter(Mandatory = $true)]$Invite)
    if (-not ($Invite.PSObject.Properties.Name -contains 'lan')) { return '' }
    $address = [string]$Invite.lan
    if ($address -notmatch '^(\d+)\.(\d+)\.\d+\.\d+$' -or $address -match '^127\.') { return '' }
    $a = [int]$Matches[1]; $b = [int]$Matches[2]
    if (($a -eq 10) -or ($a -eq 172 -and $b -ge 16 -and $b -le 31) -or ($a -eq 192 -and $b -eq 168)) { return $address }
    return ''
}

function Test-CoopSameNetwork {
    # This machine in the network the address belongs to, as a home network
    # is laid out: the first three numbers the same.
    param([string]$Address)
    if ($Address -notmatch '^(\d{1,3}\.\d{1,3}\.\d{1,3})\.\d{1,3}$') { return $false }
    $prefix = $Matches[1] + '.'
    try {
        foreach ($a in @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop)) {
            if (([string]$a.IPAddress).StartsWith($prefix)) { return $true }
        }
    }
    catch { }
    return $false
}

function Select-CoopJoinHost {
    # The host's home address when this machine is in that network and the
    # world answers there - the laptop in the same house, whose way to the
    # host's Internet address goes through a router that may not bring it
    # back in (xXxDaronxXx, 24 September) - and the invite's own otherwise.
    # The same choice as the launcher's Select-M2CoopJoinHost.
    param([Parameter(Mandatory = $true)]$Invite)
    $lan = Get-CoopInviteLan -Invite $Invite
    $same = [bool]($lan -and (Test-CoopSameNetwork -Address $lan))
    if ($same -and (Test-CoopHostAnswers -HostAddress $lan -Port ([int]$Invite.auth))) {
        return [pscustomobject]@{ Host = $lan; Lan = $true; SameNetwork = $true; Answers = $true; LanAddress = $lan }
    }
    $answers = Test-CoopHostAnswers -HostAddress ([string]$Invite.host) -Port ([int]$Invite.auth)
    return [pscustomobject]@{ Host = [string]$Invite.host; Lan = $false; SameNetwork = $same; Answers = [bool]$answers; LanAddress = $lan }
}

function Get-CoopJoinNotes {
    param([Parameter(Mandatory = $true)]$Invite, [Parameter(Mandatory = $true)]$Choice)
    $notes = @()
    $kind = Get-CoopInviteVpn -Invite $Invite
    if ($Choice.Lan) { $notes += ('Jesteś w tej samej sieci domowej co znajomy - gra połączy się przez jego adres w tej sieci ({0}).' -f $Choice.Host) }
    elseif ($kind -and -not (Test-CoopVpnHere -Kind $kind)) {
        $notes += ("Świat znajomego jest dostępny przez {0}. Zainstaluj {0} i dołącz do sieci znajomego (jak się nazywa i jakie ma hasło, powie Ci znajomy) - bez tego gra się nie połączy." -f $vpnNames[$kind])
    }
    elseif ($Choice.Answers) { $notes += 'Serwer znajomego odpowiada.' }
    elseif ($Choice.SameNetwork) {
        $notes += ('Jesteś w tej samej sieci domowej co znajomy ({0}), ale jego serwer tu nie odpowiada. Znajomy musi mieć włączone hostowanie i pozwolić Windows na regułę zapory (HOSTUJ ŚWIAT, w okienku Windows "Tak"). Potem kliknij Dołącz jeszcze raz.' -f $Choice.LanAddress)
    }
    else { $notes += 'Serwer znajomego teraz nie odpowiada - poproś, żeby uruchomił serwer (GRAJ) i włączył hostowanie.' }
    return $notes
}

if ($NoWindow) {
    if (-not $Invite) { $Invite = Read-Host 'Wklej kod zaproszenia' }
    $inv = Read-CoopInvite -Code $Invite
    $choice = Select-CoopJoinHost -Invite $inv
    $path = Write-CoopConfig -Invite $inv -HostAddress $choice.Host
    Write-Host ("Zapisano {0} (adres {1})" -f $path, $choice.Host)
    Write-Host ("W kliencie wybierz serwer 'Online: {0}'. Login: {1}, hasło: {2}" -f $inv.name, $inv.login, $inv.password)
    foreach ($note in @(Get-CoopJoinNotes -Invite $inv -Choice $choice)) { Write-Host $note }
    return
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()

$form = [Windows.Forms.Form]::new()
$form.Text = 'Metin2 SinglePlayer - dołącz do świata znajomego'
$form.Size = [Drawing.Size]::new(600, 470)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false

$info = [Windows.Forms.Label]::new()
$info.Text = ('1. Wklej poniżej kod zaproszenia od znajomego (zaczyna się od M2COOP1:).' + [Environment]::NewLine +
    '2. Kliknij Dołącz - świat znajomego pojawi się w grze jako drugi serwer, "Online".' + [Environment]::NewLine +
    '3. Uruchom grę, wybierz ten serwer i zaloguj się loginem i hasłem, które pokażę.')
$info.Location = [Drawing.Point]::new(14, 12)
$info.Size = [Drawing.Size]::new(560, 56)
$form.Controls.Add($info)

$codeBox = [Windows.Forms.TextBox]::new()
$codeBox.Multiline = $true
$codeBox.WordWrap = $true
$codeBox.ScrollBars = 'Vertical'
$codeBox.Location = [Drawing.Point]::new(14, 74)
$codeBox.Size = [Drawing.Size]::new(556, 96)
$codeBox.Font = [Drawing.Font]::new('Consolas', 9)
$form.Controls.Add($codeBox)
try {
    $clip = [Windows.Forms.Clipboard]::GetText()
    if ($clip -and $clip.Trim().StartsWith($prefix)) { $codeBox.Text = $clip.Trim() }
}
catch { }

$joinButton = [Windows.Forms.Button]::new()
$joinButton.Text = 'Dołącz'
$joinButton.Location = [Drawing.Point]::new(14, 180)
$joinButton.Size = [Drawing.Size]::new(150, 36)
$joinButton.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
$form.Controls.Add($joinButton)

$result = [Windows.Forms.TextBox]::new()
$result.Multiline = $true
$result.ReadOnly = $true
$result.Location = [Drawing.Point]::new(14, 228)
$result.Size = [Drawing.Size]::new(556, 110)
$result.ScrollBars = 'Vertical'
$result.Font = [Drawing.Font]::new('Consolas', 10)
$form.Controls.Add($result)

$playButton = [Windows.Forms.Button]::new()
$playButton.Text = 'Uruchom grę'
$playButton.Location = [Drawing.Point]::new(14, 350)
$playButton.Size = [Drawing.Size]::new(150, 36)
$playButton.Enabled = $false
$form.Controls.Add($playButton)

$forgetButton = [Windows.Forms.Button]::new()
$forgetButton.Text = 'Usuń świat znajomego z listy'
$forgetButton.Location = [Drawing.Point]::new(176, 350)
$forgetButton.Size = [Drawing.Size]::new(220, 36)
$form.Controls.Add($forgetButton)

$closeButton = [Windows.Forms.Button]::new()
$closeButton.Text = 'Zamknij'
$closeButton.Location = [Drawing.Point]::new(470, 350)
$closeButton.Size = [Drawing.Size]::new(100, 36)
$closeButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
$form.Controls.Add($closeButton)
$form.CancelButton = $closeButton

$existing = Join-Path $clientDir 'coop.cfg'
if (Test-Path -LiteralPath $existing -PathType Leaf) {
    $text = [IO.File]::ReadAllText($existing)
    if ($text -match '(?m)^name=(.*)$') {
        $result.Text = ("W kliencie jest już świat znajomego: {0}.`r`nNowy kod go zastąpi." -f $Matches[1].Trim())
        $playButton.Enabled = $true
    }
}

$joinButton.Add_Click({
    try {
        $inv = Read-CoopInvite -Code $codeBox.Text
        $form.Cursor = [Windows.Forms.Cursors]::WaitCursor
        $choice = Select-CoopJoinHost -Invite $inv
        [void](Write-CoopConfig -Invite $inv -HostAddress $choice.Host)
        try { [Windows.Forms.Clipboard]::SetText([string]$inv.password) } catch { }
        $notes = @(Get-CoopJoinNotes -Invite $inv -Choice $choice)
        $form.Cursor = [Windows.Forms.Cursors]::Default
        $result.Text = ("Gotowe. W grze wybierz serwer 'Online: {0}'.`r`n`r`nLogin: {1}`r`nHasło: {2}`r`n(hasło jest też w schowku)`r`n`r`n{3}" -f $inv.name, $inv.login, $inv.password, ($notes -join "`r`n"))
        $playButton.Enabled = $true
    }
    catch {
        [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Kod zaproszenia', 'OK', 'Warning') | Out-Null
    }
})

$playButton.Add_Click({
    $exe = Join-Path $clientDir 'metin2client.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        [Windows.Forms.MessageBox]::Show('Nie ma metin2client.exe obok tego pliku.', 'Uruchom grę', 'OK', 'Warning') | Out-Null
        return
    }
    Start-Process -FilePath $exe -WorkingDirectory $clientDir | Out-Null
    $form.Close()
})

$forgetButton.Add_Click({
    if (Test-Path -LiteralPath $existing -PathType Leaf) { [IO.File]::Delete($existing) }
    $result.Text = 'Świat znajomego usunięty z listy serwerów.'
    $playButton.Enabled = $false
})

[void]$form.ShowDialog()
