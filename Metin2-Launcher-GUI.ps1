[CmdletBinding()]
param([switch]$SelfTest)

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($PSScriptRoot)
$cliLauncher = Join-Path $root 'Metin2-Launcher.ps1'
$modulePath = Join-Path $root 'launcher\Metin2Launcher.psm1'
$diagnosticsModulePath = Join-Path $root 'launcher\Metin2Launcher.Diagnostics.psm1'
$configPath = Join-Path $root '.m2launcher.json'
$logDirectory = Join-Path $root 'launcher-logs'
$supportDirectory = Join-Path $root 'support-bundles'
$composeFile = Join-Path $root 'linux-port\docker\docker-compose.yml'
$sessionLog = Join-Path $logDirectory ('launcher-{0}.log' -f (Get-Date -Format 'yyyyMMdd'))

foreach ($required in @($cliLauncher, $modulePath, $diagnosticsModulePath, $composeFile)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Brakuje wymaganego pliku: $required"
    }
}

Import-Module $modulePath -Force
Import-Module $diagnosticsModulePath -Force
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName Microsoft.VisualBasic

if ($SelfTest) {
    $cliErrors = $null
    $guiErrors = $null
    $diagnosticErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($cliLauncher, [ref]$null, [ref]$cliErrors)
    [void][System.Management.Automation.Language.Parser]::ParseFile($PSCommandPath, [ref]$null, [ref]$guiErrors)
    [void][System.Management.Automation.Language.Parser]::ParseFile($diagnosticsModulePath, [ref]$null, [ref]$diagnosticErrors)
    $formProbe = [Windows.Forms.Form]::new()
    $formProbe.Dispose()
    $portGuidance = Get-M2LauncherErrorGuidance -Text 'Bind for 127.0.0.1:7788 failed: port is already allocated'
    $wslGuidance = Get-M2LauncherErrorGuidance -Text 'There was a problem with WSL; wsl.exe exit status 1'
    [pscustomobject]@{
        Gui = 'OK'
        CliParserErrors = @($cliErrors).Count
        GuiParserErrors = @($guiErrors).Count
        DiagnosticsParserErrors = @($diagnosticErrors).Count
        PortErrorParser = $portGuidance.Code
        WslErrorParser = $wslGuidance.Code
        ComposePresent = Test-Path -LiteralPath $composeFile -PathType Leaf
        DockerCliPresent = $null -ne (Get-Command docker -ErrorAction SilentlyContinue)
        ConfigReadable = $null -ne (Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath)
    } | ConvertTo-Json
    exit 0
}

[Windows.Forms.Application]::EnableVisualStyles()

function Write-LocalLog {
    # -FileOnly keeps very chatty build output (apt, unpacking) in the session
    # log without flooding the small on-screen box.
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [switch]$FileOnly
    )
    $line = '{0}  {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    [IO.File]::AppendAllText($sessionLog, $line + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    if (-not $FileOnly -and $script:logBox -and -not $script:logBox.IsDisposed) {
        $script:logBox.AppendText($line + [Environment]::NewLine)
        # Keep the box bounded so a long build cannot grow it without limit.
        if ($script:logBox.Lines.Count -gt 600) {
            $script:logBox.Lines = $script:logBox.Lines[-400..-1]
        }
        $script:logBox.SelectionStart = $script:logBox.TextLength
        $script:logBox.ScrollToCaret()
    }
}

function New-Button {
    param(
        [string]$Text,
        [int]$X,
        [int]$Y,
        [int]$Width = 210,
        [int]$Height = 52,
        [Drawing.Color]$Color = [Drawing.Color]::FromArgb(45, 110, 190)
    )
    $button = [Windows.Forms.Button]::new()
    $button.Text = $Text
    $button.Location = [Drawing.Point]::new($X, $Y)
    $button.Size = [Drawing.Size]::new($Width, $Height)
    $button.BackColor = $Color
    $button.ForeColor = [Drawing.Color]::White
    $button.FlatStyle = 'Flat'
    $button.FlatAppearance.BorderSize = 0
    $button.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $button.Cursor = [Windows.Forms.Cursors]::Hand
    return $button
}

function Get-LauncherConfig {
    Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
}

function Save-ClientExecutable {
    param([Parameter(Mandatory = $true)][string]$Executable)
    $config = Get-LauncherConfig
    $config.clientExecutable = [IO.Path]::GetFullPath($Executable)
    $config.clientRoot = [IO.Path]::GetFullPath((Split-Path -Parent $Executable))
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    Write-LocalLog "Wybrano klienta: $([IO.Path]::GetFileName($Executable))"
}

function Select-ClientExecutable {
    $config = Get-LauncherConfig
    $dialog = [Windows.Forms.OpenFileDialog]::new()
    $dialog.Title = 'Wybierz plik uruchamiający klienta Metin2'
    $dialog.Filter = 'Program klienta Metin2 (*.exe)|*.exe|Wszystkie pliki (*.*)|*.*'
    $dialog.CheckFileExists = $true
    if ($config.clientRoot -and (Test-Path -LiteralPath $config.clientRoot -PathType Container)) {
        $dialog.InitialDirectory = $config.clientRoot
    }
    if ($dialog.ShowDialog($script:form) -eq [Windows.Forms.DialogResult]::OK) {
        Save-ClientExecutable -Executable $dialog.FileName
        return $dialog.FileName
    }
    return ''
}

function Find-ClientExecutable {
    $config = Get-LauncherConfig
    if ($config.clientExecutable -and (Test-Path -LiteralPath $config.clientExecutable -PathType Leaf)) {
        return [string]$config.clientExecutable
    }
    if ($config.clientRoot -and (Test-Path -LiteralPath $config.clientRoot -PathType Container)) {
        foreach ($name in @('metin2client.exe', 'Metin2.exe', 'metin2.exe', 'start.exe', 'launcher.exe')) {
            $candidate = Join-Path $config.clientRoot $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                Save-ClientExecutable -Executable $candidate
                return $candidate
            }
        }
    }
    return ''
}

function Start-ConfiguredClient {
    $executable = Find-ClientExecutable
    if (-not $executable) { $executable = Select-ClientExecutable }
    if (-not $executable) {
        [Windows.Forms.MessageBox]::Show(
            'Nie wybrano klienta. Użyj przycisku „Wybierz klienta”.',
            'Metin2 Playerbots', 'OK', 'Information') | Out-Null
        return
    }
    try {
        Start-Process -FilePath $executable -WorkingDirectory (Split-Path -Parent $executable)
        Write-LocalLog "Uruchomiono klienta: $([IO.Path]::GetFileName($executable))"
    }
    catch {
        Write-LocalLog "BŁĄD uruchamiania klienta: $($_.Exception.Message)"
        [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Nie udało się uruchomić klienta', 'OK', 'Error') | Out-Null
    }
}

function Invoke-QuickProcess {
    param([string]$FileName, [string]$Arguments, [int]$TimeoutMs = 1800)
    try {
        $psi = [Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $FileName
        $psi.Arguments = $Arguments
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($psi)
        if (-not $process.WaitForExit($TimeoutMs)) {
            try { $process.Kill() } catch {}
            return [pscustomobject]@{ ExitCode = -1; Output = '' }
        }
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Output = $process.StandardOutput.ReadToEnd() + $process.StandardError.ReadToEnd()
        }
    }
    catch { return [pscustomobject]@{ ExitCode = -1; Output = $_.Exception.Message } }
}

function Refresh-Status {
    $dockerInstalled = $null -ne (Get-Command docker -ErrorAction SilentlyContinue)
    $dockerProcessesRunning = @(Get-Process -Name 'Docker Desktop', 'com.docker.backend' -ErrorAction SilentlyContinue).Count -gt 0
    $dockerEngineReady = $false
    if ($dockerInstalled) {
        $engineResult = Invoke-QuickProcess -FileName 'docker.exe' -Arguments 'info --format "{{.ServerVersion}}"' -TimeoutMs 2500
        $dockerEngineReady = $engineResult.ExitCode -eq 0
    }
    $script:dockerStatus.Text = if (-not $dockerInstalled) { 'Docker: NIEZAINSTALOWANY' }
        elseif ($dockerEngineReady) { 'Docker: GOTOWY' }
        elseif ($dockerProcessesRunning) { 'Docker: STARTUJE / WYMAGA NAPRAWY' }
        else { 'Docker: ZATRZYMANY' }
    $script:dockerStatus.ForeColor = if ($dockerEngineReady) { [Drawing.Color]::LightGreen }
        elseif ($dockerInstalled) { [Drawing.Color]::Gold }
        else { [Drawing.Color]::Tomato }

    $serverRunning = $false
    if ($dockerEngineReady) {
        $composeDirectory = Join-Path $root 'linux-port\docker'
        $result = Invoke-QuickProcess -FileName 'docker.exe' -Arguments (
            'compose --project-directory "{0}" -f "{1}" ps --services --status running' -f $composeDirectory, $composeFile) -TimeoutMs 1800
        $serverRunning = $result.ExitCode -eq 0 -and $result.Output -match '(?m)^game\s*$'
    }
    $script:serverStatus.Text = if ($serverRunning) { 'Serwer: DZIAŁA' } else { 'Serwer: ZATRZYMANY' }
    $script:serverStatus.ForeColor = if ($serverRunning) { [Drawing.Color]::LightGreen } else { [Drawing.Color]::Silver }
}

# apt/dpkg chatter from a first image build. Kept in the session log, kept out
# of the on-screen box so the interesting lines stay readable.
$script:M2_NOISY_BUILD = 'Get:\d|Unpacking |Selecting previously|Preparing to unpack|Reading database|Setting up |Suggested packages:|Recommended packages:|The following NEW packages|The following packages will be|debconf:'

# Discord invite the ZIP button falls back to, and the once-per-session cache of
# the support address read from the update manifest.
$script:openContactAfterAction = ''
$script:supportSettingsCache = $null

function Read-SharedText {
    # The child process still holds these files open for writing.
    param([Parameter(Mandatory = $true)][string]$Path)
    try {
        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            $reader = New-Object IO.StreamReader($stream, [Text.Encoding]::UTF8)
            try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
        }
        finally { $stream.Dispose() }
    }
    catch { return $null }
}

function Update-ActionPhase {
    # Turn BuildKit / Compose chatter into a phase name and a step count, so the
    # progress bar and the status line can show real movement during the long
    # first build instead of an endless marquee.
    param([Parameter(Mandatory = $true)][string]$Line)
    $step = [Regex]::Match($Line, '^\s*#\d+\s+\[([^\]]+?)\s+(\d+)/(\d+)\]')
    if ($step.Success) {
        $script:activePhase = $step.Groups[1].Value
        $script:activePhaseStep = [int]$step.Groups[2].Value
        $script:activePhaseTotal = [int]$step.Groups[3].Value
        if (-not $script:activeBuildNoticed) {
            $script:activeBuildNoticed = $true
            Write-LocalLog 'Trwa budowanie obrazów serwera. Przy pierwszym uruchomieniu to normalnie kilkanaście–kilkadziesiąt minut — nie przerywaj.'
        }
        return
    }
    if ($Line -match 'transferring context:\s*([\d.]+\s*[kKMG]?B)') {
        $script:activePhase = "przesyłanie plików do budowy ($($Matches[1]))"
        $script:activePhaseStep = 0; $script:activePhaseTotal = 0
        return
    }
    $container = [Regex]::Match($Line, 'Container\s+(\S+)\s+(Creating|Created|Starting|Started|Waiting|Healthy|Recreate|Stopping|Stopped)')
    if ($container.Success) {
        $script:activePhase = "$($container.Groups[1].Value): $($container.Groups[2].Value)"
        $script:activePhaseStep = 0; $script:activePhaseTotal = 0
    }
}

function Update-ActionStatusText {
    if (-not $script:activeProcess -or -not $script:actionStatus) { return }
    $elapsed = (Get-Date) - $script:activeStarted
    $text = 'Trwa: {0}...  {1:mm\:ss}' -f $script:activeAction, $elapsed
    if ($script:activePhaseTotal -gt 0) {
        $pct = [int](100 * $script:activePhaseStep / $script:activePhaseTotal)
        $pct = [Math]::Max(0, [Math]::Min(100, $pct))
        $text += '   —   {0} {1}/{2} ({3}%)' -f $script:activePhase, $script:activePhaseStep, $script:activePhaseTotal, $pct
        if ($script:progress.Style -ne 'Blocks') { $script:progress.Style = 'Blocks' }
        $script:progress.Value = $pct
    }
    elseif ($script:activePhase) {
        $text += '   —   {0}' -f $script:activePhase
    }
    $script:actionStatus.Text = $text
}

function Update-ActionStream {
    # Tail the running action's output into the log while it runs, so a long
    # start does not look like a frozen window.
    if (-not $script:activeProcess) { return }
    foreach ($key in @('out', 'err')) {
        $path = if ($key -eq 'out') { $script:activeOut } else { $script:activeErr }
        if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $content = Read-SharedText -Path $path
        if ($null -eq $content) { continue }
        $offset = if ($key -eq 'out') { $script:activeOutOffset } else { $script:activeErrOffset }
        if ($content.Length -le $offset) { continue }
        $fresh = $content.Substring($offset)
        $lastBreak = $fresh.LastIndexOf("`n")
        if ($lastBreak -lt 0) { continue }
        $complete = $fresh.Substring(0, $lastBreak + 1)
        if ($key -eq 'out') { $script:activeOutOffset = $offset + $complete.Length }
        else { $script:activeErrOffset = $offset + $complete.Length }
        $script:activeOutputAll += $complete
        foreach ($line in ($complete -split '\r?\n')) {
            if (-not $line.Trim()) { continue }
            Update-ActionPhase -Line $line
            if ($line -match $script:M2_NOISY_BUILD) { Write-LocalLog $line -FileOnly }
            else { Write-LocalLog $line }
        }
    }
    Update-ActionStatusText
}

function Complete-LauncherAction {
    if (-not $script:activeProcess) { return }
    try { $script:activeProcess.Refresh() } catch {}
    if (-not $script:activeProcess.HasExited) { return }

    $exitCode = $script:activeProcess.ExitCode
    # Flush whatever the action wrote between the last tick and its exit.
    Update-ActionStream
    foreach ($key in @('out', 'err')) {
        $path = if ($key -eq 'out') { $script:activeOut } else { $script:activeErr }
        if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        $content = Read-SharedText -Path $path
        if ($null -eq $content) { continue }
        $offset = if ($key -eq 'out') { $script:activeOutOffset } else { $script:activeErrOffset }
        if ($content.Length -le $offset) { continue }
        $tail = $content.Substring($offset)
        if ($key -eq 'out') { $script:activeOutOffset = $content.Length } else { $script:activeErrOffset = $content.Length }
        $script:activeOutputAll += $tail
        foreach ($line in ($tail -split '\r?\n')) {
            if (-not $line.Trim()) { continue }
            if ($line -match $script:M2_NOISY_BUILD) { Write-LocalLog $line -FileOnly } else { Write-LocalLog $line }
        }
    }
    $output = $script:activeOutputAll

    $action = $script:activeAction
    $launchClient = $script:launchClientAfterAction
    $openSupport = $script:openSupportAfterAction
    $contactUrl = $script:openContactAfterAction
    $script:activeProcess.Dispose()
    $script:activeProcess = $null
    $script:launchClientAfterAction = $false
    $script:openSupportAfterAction = $false
    $script:openContactAfterAction = ''
    $script:progress.Style = 'Blocks'
    $script:progress.Value = 0
    $script:actionStatus.Text = if ($exitCode -eq 0) { "Gotowe: $action" } else { "Błąd: $action (kod $exitCode)" }
    $script:actionStatus.ForeColor = if ($exitCode -eq 0) { [Drawing.Color]::LightGreen } else { [Drawing.Color]::Tomato }
    Write-LocalLog "Zakończono akcję $action, kod $exitCode."
    Refresh-Status

    if ($exitCode -ne 0) {
        $guidance = Get-M2LauncherErrorGuidance -Text $output
        $message = $guidance.Message + [Environment]::NewLine + [Environment]::NewLine + 'Jak naprawić:' + [Environment]::NewLine + $guidance.Remedy
        [Windows.Forms.MessageBox]::Show(
            $message,
            $guidance.Title,
            'OK',
            'Warning') | Out-Null
    }
    if ($exitCode -eq 0 -and $launchClient) { Start-ConfiguredClient }
    if ($exitCode -eq 0 -and $openSupport -and (Test-Path $supportDirectory)) {
        Start-Process explorer.exe -ArgumentList ('"{0}"' -f $supportDirectory)
        if ($contactUrl) { Start-Process $contactUrl }
    }
}

function Get-SupportSettings {
    if ($null -eq $script:supportSettingsCache) {
        try { $script:supportSettingsCache = Get-M2SupportSettings -Config (Get-LauncherConfig) }
        catch {
            Write-LocalLog "Nie udalo sie odczytac adresu zgloszen: $($_.Exception.Message)" -FileOnly
            $script:supportSettingsCache = [pscustomobject]@{ UploadUrl = ''; ContactUrl = 'https://discord.gg/pt5tvnrN6'; Source = 'none' }
        }
    }
    return $script:supportSettingsCache
}

function Get-BotCountFromEnv {
    $envPath = Join-Path $root 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^PLAYERBOT_AUTOSPAWN_COUNT=(\d+)\s*$')
        if ($match.Success) { return [int]$match.Groups[1].Value }
    }
    return 350
}

function Get-InstalledServerVersion {
    $statePath = Join-Path $root '.m2launcher-state.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$state.server) { return ([string]$state.server).Trim() }
        }
        catch { }
    }
    $versionFile = Join-Path $root 'VERSION'
    if (Test-Path -LiteralPath $versionFile -PathType Leaf) {
        return (Get-Content -LiteralPath $versionFile -Raw).Trim()
    }
    return 'unknown'
}

function Show-BotCountDialog {
    # Slider instead of a typed number: the range is a property of the world, and
    # dragging is far friendlier than guessing a value. 668 matches the largest
    # cohort the panel recognises (PID 4..671).
    param([int]$Current = 350)
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = 'Liczba grających botów'
    $dialog.Size = [Drawing.Size]::new(480, 260)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = "Ilu botów ma grać jednocześnie?`r`nEfektywny limit to liczba botów w Twoim świecie (kanoniczna paczka ma 350).`r`nZmiana wymaga restartu serwera."
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(440, 54)
    $dialog.Controls.Add($info)

    $valueLabel = [Windows.Forms.Label]::new()
    $valueLabel.Name = 'valueLabel'
    $valueLabel.Font = [Drawing.Font]::new('Segoe UI Semibold', 15)
    $valueLabel.Location = [Drawing.Point]::new(14, 70)
    $valueLabel.Size = [Drawing.Size]::new(440, 32)
    $dialog.Controls.Add($valueLabel)

    $bar = [Windows.Forms.TrackBar]::new()
    $bar.Name = 'botBar'
    $bar.Minimum = 0
    $bar.Maximum = 668
    $bar.TickFrequency = 50
    $bar.SmallChange = 1
    $bar.LargeChange = 25
    $bar.Location = [Drawing.Point]::new(12, 104)
    $bar.Size = [Drawing.Size]::new(442, 45)
    $bar.Value = [Math]::Max(0, [Math]::Min(668, $Current))
    $dialog.Controls.Add($bar)
    $valueLabel.Text = "Boty: $($bar.Value)"
    # $this/FindForm keeps the handler independent of captured locals.
    $bar.Add_ValueChanged({
            $form = $this.FindForm()
            if ($form) {
                $label = $form.Controls['valueLabel']
                if ($label) { $label.Text = "Boty: $($this.Value)" }
            }
        })

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = 'Zastosuj'
    $okButton.Location = [Drawing.Point]::new(252, 168)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = 'Anuluj'
    $cancelButton.Location = [Drawing.Point]::new(358, 168)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = $bar.Value
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return [int]$chosen
}

function Get-GuiTargetVolume {
    $statePath = Join-Path $root '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            $vp = $state.PSObject.Properties['databaseVolume']
            if ($vp -and [string]$vp.Value) { return [string]$vp.Value }
            if ([string]$state.projectName) { return "$([string]$state.projectName)_db-data" }
        }
        catch { }
    }
    $envPath = Join-Path $root 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^M2_COMPOSE_PROJECT_NAME=([a-z0-9][a-z0-9_-]+)\s*$')
        if ($match.Success) { return "$($match.Groups[1].Value)_db-data" }
    }
    return ''
}

function Start-LauncherAction {
    param(
        [Parameter(Mandatory = $true)][string]$Action,
        [switch]$Yes,
        [switch]$LaunchClient,
        [switch]$OpenSupport,
        [string[]]$ExtraArgs = @()
    )
    if ($script:activeProcess -and -not $script:activeProcess.HasExited) {
        [Windows.Forms.MessageBox]::Show('Poczekaj na zakończenie bieżącej operacji.', 'Launcher pracuje', 'OK', 'Information') | Out-Null
        return
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $script:activeOut = Join-Path $logDirectory ("action-$stamp.out.log")
    $script:activeErr = Join-Path $logDirectory ("action-$stamp.err.log")
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"{0}"' -f $cliLauncher), '-Action', $Action)
    if ($Yes) { $arguments += '-Yes' }
    if ($ExtraArgs -and $ExtraArgs.Count -gt 0) { $arguments += $ExtraArgs }
    Write-LocalLog "Rozpoczęto akcję $Action."
    $script:activeAction = $Action
    $script:launchClientAfterAction = [bool]$LaunchClient
    $script:openSupportAfterAction = [bool]$OpenSupport
    # Live-progress state for this run.
    $script:activeOutOffset = 0
    $script:activeErrOffset = 0
    $script:activeOutputAll = ''
    $script:activeStarted = Get-Date
    $script:activePhase = ''
    $script:activePhaseStep = 0
    $script:activePhaseTotal = 0
    $script:activeBuildNoticed = $false
    $script:actionStatus.Text = "Trwa: $Action..."
    $script:actionStatus.ForeColor = [Drawing.Color]::Gold
    $script:progress.Style = 'Marquee'
    $script:activeProcess = Start-Process -FilePath 'powershell.exe' `
        -ArgumentList $arguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $script:activeOut -RedirectStandardError $script:activeErr
    # Start-Process -PassThru with redirected stdout/stderr does not keep the OS
    # process handle, so a later $activeProcess.ExitCode reads $null even after
    # the process has exited cleanly. That made every action -- including a fully
    # successful start -- report "Błąd: ... (kod )" with an empty code. Touching
    # .Handle now caches it so ExitCode is readable in Complete-LauncherAction.
    try { $null = $script:activeProcess.Handle } catch {}
}

function Install-Or-Prepare {
    $missing = @($cliLauncher, $composeFile, $modulePath) | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }
    if ($missing.Count) {
        [Windows.Forms.MessageBox]::Show('Paczka jest niekompletna. Rozpakuj ponownie całe archiwum RAR.', 'Brak plików', 'OK', 'Error') | Out-Null
        return
    }

    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
        $answer = [Windows.Forms.MessageBox]::Show(
            'Docker Desktop nie jest zainstalowany. Czy otworzyć oficjalną stronę pobierania?',
            'Wymagany Docker Desktop', 'YesNo', 'Question')
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Start-Process 'https://www.docker.com/products/docker-desktop/'
        }
        return
    }

    $preflight = Get-M2DockerPreflight -ServerRoot $root
    Write-LocalLog (Format-M2DockerPreflightReport -Report $preflight)
    if (-not $preflight.CanStart) {
        [Windows.Forms.MessageBox]::Show(
            ((@($preflight.BlockingIssues) -join [Environment]::NewLine) + [Environment]::NewLine + [Environment]::NewLine + 'Po naprawie uruchom launcher ponownie.'),
            'Komputer nie jest jeszcze gotowy',
            'OK',
            'Warning') | Out-Null
        return
    }

    if (-not (Find-ClientExecutable)) { [void](Select-ClientExecutable) }
    try {
        $desktop = [Environment]::GetFolderPath('Desktop')
        $shortcutPath = Join-Path $desktop 'Metin2 Playerbots.lnk'
        $shell = New-Object -ComObject WScript.Shell
        $shortcut = $shell.CreateShortcut($shortcutPath)
        $shortcut.TargetPath = Join-Path $root 'Metin2-Launcher-GUI.bat'
        $shortcut.WorkingDirectory = $root
        $shortcut.Description = 'Metin2 Singleplayer Playerbots'
        $shortcut.Save()
        Write-LocalLog 'Sprawdzono paczkę i utworzono skrót na pulpicie.'
    }
    catch { Write-LocalLog "Nie udało się utworzyć skrótu: $($_.Exception.Message)" }
    [Windows.Forms.MessageBox]::Show(
        'Paczka jest gotowa. Utworzono skrót na pulpicie. Kliknij GRAJ, aby uruchomić Docker, serwer i klienta.',
        'Gotowe', 'OK', 'Information') | Out-Null
}

$script:form = [Windows.Forms.Form]::new()
$script:form.Text = 'Metin2 Singleplayer Playerbots — All in One'
$script:form.Size = [Drawing.Size]::new(780, 732)
$script:form.MinimumSize = [Drawing.Size]::new(780, 732)
$script:form.StartPosition = 'CenterScreen'
$script:form.BackColor = [Drawing.Color]::FromArgb(24, 25, 29)
$script:form.ForeColor = [Drawing.Color]::White
$script:form.Font = [Drawing.Font]::new('Segoe UI', 9)

$title = [Windows.Forms.Label]::new()
$title.Text = 'METIN2 SINGLEPLAYER — PLAYERBOTS'
$title.Font = [Drawing.Font]::new('Segoe UI Semibold', 19)
$title.ForeColor = [Drawing.Color]::FromArgb(247, 194, 66)
$title.Location = [Drawing.Point]::new(24, 18)
$title.Size = [Drawing.Size]::new(710, 38)
$script:form.Controls.Add($title)

$subtitle = [Windows.Forms.Label]::new()
$subtitle.Text = 'Prosty launcher: Docker, serwer, klient, aktualizacje i diagnostyka w jednym miejscu.'
$subtitle.Location = [Drawing.Point]::new(27, 58)
$subtitle.Size = [Drawing.Size]::new(700, 24)
$subtitle.ForeColor = [Drawing.Color]::Silver
$script:form.Controls.Add($subtitle)

$script:dockerStatus = [Windows.Forms.Label]::new()
$script:dockerStatus.Location = [Drawing.Point]::new(28, 92)
$script:dockerStatus.Size = [Drawing.Size]::new(310, 25)
$script:dockerStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
$script:form.Controls.Add($script:dockerStatus)

$script:serverStatus = [Windows.Forms.Label]::new()
$script:serverStatus.Location = [Drawing.Point]::new(375, 92)
$script:serverStatus.Size = [Drawing.Size]::new(300, 25)
$script:serverStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
$script:form.Controls.Add($script:serverStatus)

$installButton = New-Button '1. ZAINSTALUJ / PRZYGOTUJ' 28 128 338 58 ([Drawing.Color]::FromArgb(88, 82, 160))
$playButton = New-Button '2. GRAJ (SERWER + KLIENT)' 388 128 338 58 ([Drawing.Color]::FromArgb(27, 150, 88))
$dockerButton = New-Button 'URUCHOM DOCKER' 28 202 218 50
$stopButton = New-Button 'ZATRZYMAJ I ZAPISZ' 268 202 218 50 ([Drawing.Color]::FromArgb(180, 75, 55))
$panelButton = New-Button 'OTWÓRZ PANEL WWW' 508 202 218 50 ([Drawing.Color]::FromArgb(180, 125, 35))
$clientButton = New-Button 'WYBIERZ KLIENTA' 28 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$updateButton = New-Button 'SPRAWDŹ AKTUALIZACJE' 268 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$bundleButton = New-Button 'ZBIERZ / WYŚLIJ LOGI' 508 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$diagnosticsButton = New-Button 'DIAGNOSTYKA' 28 328 218 45 ([Drawing.Color]::FromArgb(45, 110, 190))
$openLogButton = New-Button 'OTWÓRZ LOG' 262 328 218 45 ([Drawing.Color]::FromArgb(58, 62, 72))
$folderButton = New-Button 'FOLDER LOGÓW' 496 328 230 45 ([Drawing.Color]::FromArgb(58, 62, 72))
$botCountButton = New-Button 'LICZBA BOTÓW (0–668)' 28 380 218 32 ([Drawing.Color]::FromArgb(120, 95, 40))
$importDbButton = New-Button 'IMPORTUJ BAZĘ' 262 380 218 32 ([Drawing.Color]::FromArgb(70, 120, 90))
$repairDbButton = New-Button 'NAPRAW DOSTĘP DO BAZY' 496 380 230 32 ([Drawing.Color]::FromArgb(150, 90, 55))

foreach ($button in @($installButton, $playButton, $dockerButton, $stopButton, $panelButton, $clientButton, $updateButton, $bundleButton, $diagnosticsButton, $openLogButton, $folderButton, $botCountButton, $importDbButton, $repairDbButton)) {
    $script:form.Controls.Add($button)
}

$script:actionStatus = [Windows.Forms.Label]::new()
$script:actionStatus.Text = 'Gotowy.'
$script:actionStatus.Location = [Drawing.Point]::new(28, 434)
$script:actionStatus.Size = [Drawing.Size]::new(690, 24)
$script:actionStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$script:form.Controls.Add($script:actionStatus)

$script:progress = [Windows.Forms.ProgressBar]::new()
$script:progress.Location = [Drawing.Point]::new(28, 462)
$script:progress.Size = [Drawing.Size]::new(698, 12)
$script:form.Controls.Add($script:progress)

$script:logBox = [Windows.Forms.TextBox]::new()
$script:logBox.Location = [Drawing.Point]::new(28, 490)
$script:logBox.Size = [Drawing.Size]::new(698, 150)
$script:logBox.Multiline = $true
$script:logBox.ReadOnly = $true
$script:logBox.ScrollBars = 'Vertical'
$script:logBox.BackColor = [Drawing.Color]::FromArgb(12, 13, 16)
$script:logBox.ForeColor = [Drawing.Color]::Gainsboro
$script:logBox.Font = [Drawing.Font]::new('Consolas', 8.5)
$script:form.Controls.Add($script:logBox)

$footer = [Windows.Forms.Label]::new()
$footer.Text = '„Zatrzymaj i zapisz” nie usuwa postaci ani postępu botów. Nigdy nie używa docker compose down -v.'
$footer.Location = [Drawing.Point]::new(28, 650)
$footer.Size = [Drawing.Size]::new(700, 25)
$footer.ForeColor = [Drawing.Color]::DarkGray
$script:form.Controls.Add($footer)

$installButton.Add_Click({ Install-Or-Prepare })
$playButton.Add_Click({
    if (-not (Find-ClientExecutable)) {
        if (-not (Select-ClientExecutable)) { return }
    }
    Start-LauncherAction -Action 'Start' -LaunchClient
})
$dockerButton.Add_Click({ Start-LauncherAction -Action 'StartDocker' })
$stopButton.Add_Click({
    $answer = [Windows.Forms.MessageBox]::Show(
        'Zatrzymać serwer i Docker Desktop? Postacie, baza i postęp botów zostaną zachowane.',
        'Bezpieczne zatrzymanie', 'YesNo', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) { Start-LauncherAction -Action 'StopAll' }
})
$panelButton.Add_Click({ Start-Process 'http://127.0.0.1:7788/map' })
$clientButton.Add_Click({ [void](Select-ClientExecutable) })
$updateButton.Add_Click({
    # One button for the whole flow: check in-process, and only offer to install
    # when there really is something newer.
    $installed = Get-InstalledServerVersion
    Write-LocalLog "Sprawdzam aktualizacje (zainstalowana wersja: $installed)..."
    $manifest = $null
    try {
        $config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
        $manifest = Get-M2UpdateManifest -Source ([string]$config.manifestUrl)
    }
    catch {
        Write-LocalLog "Nie udało się sprawdzić aktualizacji: $($_.Exception.Message)"
        [Windows.Forms.MessageBox]::Show(
            "Nie udało się sprawdzić aktualizacji.`r`n`r`n$($_.Exception.Message)",
            'Sprawdzanie aktualizacji', 'OK', 'Warning') | Out-Null
        return
    }
    $server = $null
    $serverProperty = $manifest.PSObject.Properties['server']
    if ($serverProperty -and $serverProperty.Value) { $server = $serverProperty.Value }
    if (-not $server -or -not [string]$server.version) {
        $message = 'Kanał aktualizacji nie ma obecnie nowej wersji serwera. Twoja instalacja pozostaje bez zmian.'
        $statusProperty = $manifest.PSObject.Properties['statusMessage']
        if ($statusProperty -and [string]$statusProperty.Value) { $message = [string]$statusProperty.Value }
        Write-LocalLog $message
        [Windows.Forms.MessageBox]::Show($message, 'Brak aktualizacji', 'OK', 'Information') | Out-Null
        return
    }
    $available = ([string]$server.version).Trim()
    Write-LocalLog "Dostępna wersja serwera: $available"
    if ($installed -and $installed -ne 'unknown' -and $installed.Equals($available, [StringComparison]::OrdinalIgnoreCase)) {
        [Windows.Forms.MessageBox]::Show("Masz już najnowszą wersję ($available).", 'Aktualizacje', 'OK', 'Information') | Out-Null
        return
    }
    $answer = [Windows.Forms.MessageBox]::Show(
        "Znaleziono nową wersję serwera: $available`r`n(zainstalowana: $installed)`r`n`r`nZainstalować teraz?`r`n`r`nTwoje postacie, przedmioty i boty pozostaną bez zmian. Aktualizacja przebudowuje serwer lokalnie — przy pierwszym razie może to potrwać kilkanaście–kilkadziesiąt minut. Postęp zobaczysz w logu poniżej.",
        'Dostępna aktualizacja', 'YesNo', 'Question')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) {
        Write-LocalLog 'Aktualizacja odłożona na później.'
        return
    }
    Start-LauncherAction -Action 'UpdateAll' -Yes
})
$diagnosticsButton.Add_Click({ Start-LauncherAction -Action 'Diagnose' })
$bundleButton.Add_Click({
    # The support address comes from the update manifest, so it is looked up
    # once per session - a slow or missing network just falls back to the ZIP.
    $support = Get-SupportSettings
    if ($support.UploadUrl) {
        $answer = [Windows.Forms.MessageBox]::Show(
            "Spakowac logi i wyslac je od razu do autora projektu?`r`n`r`nPaczka zawiera logi Dockera i konfiguracje z usunietymi haslami.`r`n`r`nNIE = tylko zapisz ZIP na dysku.",
            'Wyslij logi', 'YesNoCancel', 'Question')
        if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Start-LauncherAction -Action 'SendLogs' -Yes
            return
        }
    }
    else {
        [Windows.Forms.MessageBox]::Show(
            "Zapisze paczke ZIP z logami i otworze jej folder - dolacz ja na Discordzie.`r`n`r`nOtworze tez zaproszenie na serwer.",
            'Logi', 'OK', 'Information') | Out-Null
        $script:openContactAfterAction = $support.ContactUrl
    }
    Start-LauncherAction -Action 'Logs' -OpenSupport
})
$openLogButton.Add_Click({
    if (-not (Test-Path -LiteralPath $sessionLog -PathType Leaf)) { Write-LocalLog 'Utworzono dziennik launchera.' }
    Start-Process notepad.exe -ArgumentList ('"{0}"' -f $sessionLog)
})
$folderButton.Add_Click({
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    Start-Process explorer.exe -ArgumentList ('"{0}"' -f $logDirectory)
})
$botCountButton.Add_Click({
    $current = Get-BotCountFromEnv
    $count = Show-BotCountDialog -Current $current
    if ($null -eq $count) { return }
    $answer = [Windows.Forms.MessageBox]::Show(
        "Ustawić $count grających botów i zrestartować serwer teraz, aby zastosować? Baza i postęp botów pozostaną bez zmian.",
        'Liczba botów', 'YesNoCancel', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
        Start-LauncherAction -Action 'SetBots' -Yes -ExtraArgs @('-BotCount', "$count")
    }
    else {
        Start-LauncherAction -Action 'SetBots' -ExtraArgs @('-BotCount', "$count")
    }
})
$importDbButton.Add_Click({
    $target = Get-GuiTargetVolume
    $sources = @()
    try { $sources = @(Get-M2DbDataVolumes | Where-Object { $_.Name -ne $target }) } catch { $sources = @() }
    if (-not $sources -or $sources.Count -eq 0) {
        [Windows.Forms.MessageBox]::Show('Nie znaleziono innej bazy Docker do importu na tym komputerze.', 'Import bazy', 'OK', 'Information') | Out-Null
        return
    }
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = 'Importuj bazę z innej instalacji'
    $dlg.Size = [Drawing.Size]::new(470, 320)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $lbl = [Windows.Forms.Label]::new()
    $lbl.Text = 'Wybierz źródłową instalację. Jej świat (postacie, poziomy, ekwipunek) zostanie skopiowany do bieżącej instalacji.'
    $lbl.Location = [Drawing.Point]::new(12, 10)
    $lbl.Size = [Drawing.Size]::new(430, 44)
    $dlg.Controls.Add($lbl)
    $list = [Windows.Forms.ListBox]::new()
    $list.Location = [Drawing.Point]::new(12, 58)
    $list.Size = [Drawing.Size]::new(430, 170)
    foreach ($item in $sources) { [void]$list.Items.Add($item.Project) }
    $list.SelectedIndex = 0
    $dlg.Controls.Add($list)
    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = 'Importuj'
    $okButton.Location = [Drawing.Point]::new(246, 240)
    $okButton.Size = [Drawing.Size]::new(95, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dlg.Controls.Add($okButton)
    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = 'Anuluj'
    $cancelButton.Location = [Drawing.Point]::new(347, 240)
    $cancelButton.Size = [Drawing.Size]::new(95, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dlg.Controls.Add($cancelButton)
    $dlg.AcceptButton = $okButton
    $dlg.CancelButton = $cancelButton
    $result = $dlg.ShowDialog()
    $picked = [string]$list.SelectedItem
    $dlg.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK -or -not $picked) { return }
    $confirm = [Windows.Forms.MessageBox]::Show(
        "Zaimportować świat z '$picked' do bieżącej instalacji?`r`n`r`nObecny świat zostanie ZASTĄPIONY (kopia trafi do folderu 'backups'), a serwer zostanie zatrzymany na czas importu. Źródło pozostanie nietknięte.",
        'Potwierdź import bazy', 'YesNo', 'Warning')
    if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'ImportDb' -Yes -ExtraArgs @('-ImportSource', "$picked")
})
$repairDbButton.Add_Click({
    $answer = [Windows.Forms.MessageBox]::Show(
        "Naprawić dostęp do bazy?`r`n`r`nUżyj tego, gdy po imporcie serwer nie startuje (playerbot-migrate kończy się błędem). Odtwarza tylko techniczne konto bazy — postacie, przedmioty i boty pozostają BEZ ZMIAN. Serwer zostanie zatrzymany na czas naprawy.",
        'Napraw dostęp do bazy', 'YesNo', 'Question')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'RepairDb'
})

$timer = [Windows.Forms.Timer]::new()
$timer.Interval = 1200
$timer.Add_Tick({ Update-ActionStream; Complete-LauncherAction })
$timer.Start()

$statusTimer = [Windows.Forms.Timer]::new()
$statusTimer.Interval = 8000
$statusTimer.Add_Tick({ if (-not $script:activeProcess) { Refresh-Status } })
$statusTimer.Start()

$script:form.Add_FormClosing({
    param($sender, $eventArgs)
    if ($script:activeProcess -and -not $script:activeProcess.HasExited) {
        $answer = [Windows.Forms.MessageBox]::Show(
            'Launcher nadal wykonuje operację. Czy na pewno zamknąć okno?',
            'Operacja w toku', 'YesNo', 'Warning')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { $eventArgs.Cancel = $true }
    }
})

if (Test-Path -LiteralPath $sessionLog -PathType Leaf) {
    $tail = Get-Content -LiteralPath $sessionLog -Tail 12 -ErrorAction SilentlyContinue
    if ($tail) { $script:logBox.Text = ($tail -join [Environment]::NewLine) + [Environment]::NewLine }
}
Write-LocalLog 'Uruchomiono GUI launchera.'
Refresh-Status
[void]$script:form.ShowDialog()
