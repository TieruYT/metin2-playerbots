[CmdletBinding()]
param(
    [switch]$SelfTest,
    # The window's own test (Metin2-Launcher-GUI.Layout.ps1): the window is
    # built off screen, every page is walked at three sizes, a picture of
    # each goes to -UiTestOutput, and the process ends before any timer,
    # Docker question, update check or action.
    [switch]$UiSelfTest,
    [string]$UiTestOutput,
    [ValidateSet('pl', 'en')][string]$UiLanguage = 'pl',
    # Another installation for the modules and the configuration, so the
    # window can be tried from a checkout against a working server.
    [string]$ServerRoot = ''
)

$ErrorActionPreference = 'Stop'
if (-not $ServerRoot) { $ServerRoot = $PSScriptRoot }
$root = [IO.Path]::GetFullPath($ServerRoot)
$cliLauncher = Join-Path $root 'Metin2-Launcher.ps1'
$modulePath = Join-Path $root 'launcher\Metin2Launcher.psm1'
$diagnosticsModulePath = Join-Path $root 'launcher\Metin2Launcher.Diagnostics.psm1'
$configPath = Join-Path $root '.m2launcher.json'
$logDirectory = Join-Path $root 'launcher-logs'
if ($UiSelfTest) {
    if (-not $UiTestOutput) { throw 'UiTestOutput is required for UiSelfTest.' }
    $logDirectory = Join-Path ([IO.Path]::GetFullPath($UiTestOutput)) 'test-logs'
}
$supportDirectory = Join-Path $root 'support-bundles'
$composeFile = Join-Path $root 'linux-port\docker\docker-compose.yml'
$sessionLog = Join-Path $logDirectory ('launcher-{0}.log' -f (Get-Date -Format 'yyyyMMdd'))

function Write-StartupFailure {
    # Straight to the file: this runs before (or instead of) the window, so
    # Write-LocalLog and its on-screen box may not exist yet.
    param([string]$Text)
    try {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        [IO.File]::AppendAllText($sessionLog,
            ('{0}  BLAD LAUNCHERA: {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Text) + [Environment]::NewLine,
            [Text.UTF8Encoding]::new($false))
    }
    catch { }
}

trap {
    if ($UiSelfTest) { Write-Error $_ -ErrorAction Continue; exit 1 }
    # A launcher that dies before its first log line left nothing behind but a
    # dialog nobody could copy from - after the 2.0.8 restart the session log
    # ended at "Uruchamiam launcher ponownie" and the player saw an error box
    # (11 September). Whatever stops the script is written down first, then
    # shown with its text, so the next report carries the reason.
    Write-StartupFailure ($_ | Out-String)
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [Windows.Forms.MessageBox]::Show(
            ("Launcher nie wystartowal:`r`n`r`n{0}`r`n`r`nSzczegoly sa w folderze launcher-logs." -f $_.Exception.Message),
            'Blad launchera', 'OK', 'Error') | Out-Null
    }
    catch { }
    break
}

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
# Before the first control exists: an exception thrown inside a button or
# timer handler is logged with its stack and shown with its text, instead of
# the .NET "Unhandled exception has occurred" dialog and an empty log.
[Windows.Forms.Application]::SetUnhandledExceptionMode([Windows.Forms.UnhandledExceptionMode]::CatchException)
[Windows.Forms.Application]::add_ThreadException([System.Threading.ThreadExceptionEventHandler]{
    param($sender, $eventArgs)
    Write-StartupFailure ('w oknie: ' + $eventArgs.Exception.ToString())
    try {
        [Windows.Forms.MessageBox]::Show(
            ("Blad w oknie launchera:`r`n`r`n{0}`r`n`r`nSzczegoly sa w folderze launcher-logs. Okno dziala dalej." -f $eventArgs.Exception.Message),
            'Blad launchera', 'OK', 'Error') | Out-Null
    }
    catch { }
})

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

# The session log is opened in Notepad and pasted into chat, and Polish letters
# do not survive either. Everything written to the file is transliterated; the
# on-screen box is left alone, because it renders them correctly and there is no
# reason to make the window worse to fix the file. A character that is neither
# ASCII nor in the table - including the replacement character left behind by an
# older, mis-encoded log - becomes a question mark rather than disappearing.
$script:M2_ASCII_MAP = @{
    [char]0x0105 = 'a'; [char]0x0107 = 'c'; [char]0x0119 = 'e'; [char]0x0142 = 'l'
    [char]0x0144 = 'n'; [char]0x00F3 = 'o'; [char]0x015B = 's'; [char]0x017A = 'z'
    [char]0x017C = 'z'
    [char]0x0104 = 'A'; [char]0x0106 = 'C'; [char]0x0118 = 'E'; [char]0x0141 = 'L'
    [char]0x0143 = 'N'; [char]0x00D3 = 'O'; [char]0x015A = 'S'; [char]0x0179 = 'Z'
    [char]0x017B = 'Z'
    [char]0x2013 = '-'; [char]0x2014 = '-'; [char]0x2026 = '...'
    [char]0x2018 = "'"; [char]0x2019 = "'"; [char]0x201C = '"'; [char]0x201D = '"'
    [char]0x00A0 = ' '
}

function ConvertTo-M2AsciiLine {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return $Text }
    $builder = New-Object Text.StringBuilder
    foreach ($ch in $Text.ToCharArray()) {
        if ($script:M2_ASCII_MAP.ContainsKey($ch)) {
            [void]$builder.Append($script:M2_ASCII_MAP[$ch])
        }
        elseif ([int]$ch -lt 128) { [void]$builder.Append($ch) }
        else { [void]$builder.Append('?') }
    }
    return $builder.ToString()
}

function Write-LocalLog {
    # -FileOnly keeps very chatty build output (apt, unpacking) in the session
    # log without flooding the small on-screen box.
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [switch]$FileOnly
    )
    $line = '{0}  {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    [IO.File]::AppendAllText($sessionLog,
        (ConvertTo-M2AsciiLine $line) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
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

# Every word this window shows, in both languages.
#
# The Discord has more and more English speakers and a launcher they cannot read
# is a launcher they run wrongly - the difference between "ZATRZYMAJ I ZAPISZ"
# and "wipe everything" is not guessable. Only the interface is translated here;
# the two web panels are separate applications with their own thousand-odd
# strings and are not covered by this switch.
$script:Strings = @{
    pl = @{
        formTitle    = 'Metin2 Singleplayer Playerbots - All in One'
        title        = 'METIN2 SINGLEPLAYER - PLAYERBOTS'
        subtitle     = 'Prosty launcher: Docker, serwer, klient, aktualizacje i diagnostyka w jednym miejscu.'
        install      = '1. ZAINSTALUJ / PRZYGOTUJ'
        play         = '2. GRAJ (SERWER + KLIENT)'
        launchClient = 'Uruchom takze klienta gry'
        playNoClient = '2. GRAJ (SAM SERWER)'
        docker       = 'URUCHOM DOCKER'
        stop         = 'ZATRZYMAJ I ZAPISZ'
        panel        = 'OTWORZ PANEL WWW'
        client       = 'WYBIERZ KLIENTA'
        update       = 'SPRAWDZ AKTUALIZACJE'
        bundle       = 'ZBIERZ / WYSLIJ LOGI'
        diagnostics  = 'DIAGNOSTYKA'
        openLog      = 'OTWORZ LOG'
        logFolder    = 'FOLDER LOGOW'
        botCount     = 'LICZBA BOTOW (0-2500)'
        importDb     = 'IMPORTUJ BAZE'
        worldBackup  = 'KOPIA / NOWY SWIAT'
        backupDialog = 'Kopia swiata'
        backupInfo   = 'Kopia zapisuje caly swiat - postacie, poziomy, ekwipunek, boty i konta gry - do jednego pliku zip w folderze backups. Serwer zostanie na czas kazdej z tych operacji zatrzymany i zapisany.'
        backupMake   = 'Zapisz kopie swiata'
        backupLoad   = 'Przywroc swiat z kopii'
        backupReset  = 'Wyzeruj swiat i zacznij od nowa (swieza instalacja)'
        backupPick   = 'Wybierz plik kopii'
        backupNone   = 'W folderze backups nie ma jeszcze zadnej kopii. Zapisz najpierw kopie.'
        repairDb     = 'NAPRAW DOSTEP DO BAZY'
        dbAccess     = 'DANE DO BAZY (NAVICAT)'
        gmPanel      = 'PANEL GM F9 (TEST)'
        updateClient = 'AKTUALIZUJ KLIENTA'
        dbAccessTitle = 'Dane do polaczenia z baza'
        dbAccessHint = 'Wpisz te dane w Navicat, HeidiSQL albo DBeaver (typ MySQL/MariaDB, polaczenie TCP). Konto root widzi wszystko, konto gry tylko bazy gry. Baza slucha wylacznie na tym komputerze. Jesli baza odrzuca haslo, kliknij NAPRAW DOSTEP DO BAZY - ustawia oba konta na hasla z pliku .env. Nie wklejaj tych hasel na Discordzie.'
        dbAccessProtoNote = 'Na plikach 2.x przedmioty i potwory (item_proto, mob_proto) sa w bazie world; player.item_proto i player.mob_proto to tylko widoki. Zmiany w world zostaja po restarcie serwera.'
        startupUpdateTitle = 'Dostepna aktualizacja'
        startupServerUpdate = 'Znaleziono nowsza wersje serwera: {0}' + [Environment]::NewLine + '(zainstalowana: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Czy chcesz dokonac aktualizacji teraz?' + [Environment]::NewLine + [Environment]::NewLine + 'Postacie, przedmioty i boty zostana bez zmian. Serwer zostanie przebudowany - postep w logu na dole. Odpowiedz NIE odklada pytanie do nastepnej wersji; przycisk AKTUALIZUJ dziala zawsze.'
        startupClientUpdate = 'Znaleziono nowsza wersje klienta: {0}' + [Environment]::NewLine + '(zainstalowana: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Czy chcesz zaktualizowac klienta teraz?' + [Environment]::NewLine + [Environment]::NewLine + 'Podmienia pliki pack w folderze klienta; poprzednie trafiaja do backups\client. Odpowiedz NIE odklada pytanie do nastepnej wersji; przycisk AKTUALIZUJ KLIENTA dziala zawsze.'
        dbAccessOpenEnv = 'OTWORZ PLIK .ENV'
        dbAccessNoEnv = 'Brak pliku linux-port\docker\.env - uruchom najpierw serwer (GRAJ), launcher go utworzy.'
        language     = 'JEZYK / LANGUAGE: POLSKI'
        ready        = 'Gotowy.'
        footer       = '"Zatrzymaj i zapisz" nie usuwa postaci ani postepu botow. Nigdy nie uzywa docker compose down -v.'
        botDialog    = 'Liczba grajacych botow'
        difficulty   = 'POZIOM TRUDNOSCI'
        difficultyDialog = 'Poziom trudnosci swiata'
        coop         = 'COOP: GRA ZE ZNAJOMYMI'
        apply        = 'Zastosuj'
        cancel       = 'Anuluj'
        panelDialog  = 'Ktory panel otworzyc?'
        panelInfo    = 'Oba panele pokazuja ten sam swiat i dzialaja jednoczesnie.'
        panelClassic = "Oryginalny panel`r`nmapa i sterowanie"
        panelSeban   = "Zaawansowany panel seban latino`r`nprofile, rankingi, gospodarka, obciazenie"
        panelPw      = 'Nie moge sie zalogowac (haslo do panelu)'
        importDialog = 'Importuj baze z innej instalacji'
        importInfo   = 'Wybierz zrodlowa instalacje. Jej swiat (postacie, poziomy, ekwipunek) zostanie skopiowany do biezacej instalacji.'
        importOk     = 'Importuj'
        langSwitched = 'Jezyk zmieniony. Uruchom launcher ponownie, zeby zobaczyc zmiane.'
    }
    en = @{
        formTitle    = 'Metin2 Singleplayer Playerbots - All in One'
        title        = 'METIN2 SINGLEPLAYER - PLAYERBOTS'
        subtitle     = 'One launcher: Docker, the server, the client, updates and diagnostics in one place.'
        install      = '1. INSTALL / PREPARE'
        play         = '2. PLAY (SERVER + CLIENT)'
        launchClient = 'Start the game client too'
        playNoClient = '2. PLAY (SERVER ONLY)'
        docker       = 'START DOCKER'
        stop         = 'STOP AND SAVE'
        panel        = 'OPEN WEB PANEL'
        client       = 'CHOOSE CLIENT'
        update       = 'CHECK FOR UPDATES'
        bundle       = 'COLLECT / SEND LOGS'
        diagnostics  = 'DIAGNOSTICS'
        openLog      = 'OPEN LOG'
        logFolder    = 'LOG FOLDER'
        botCount     = 'BOT COUNT (0-2500)'
        importDb     = 'IMPORT DATABASE'
        worldBackup  = 'BACKUP / NEW WORLD'
        backupDialog = 'World backup'
        backupInfo   = 'A backup writes the whole world - characters, levels, equipment, bots and game accounts - into one zip file in the backups folder. The server is stopped and saved for each of these operations.'
        backupMake   = 'Save a backup'
        backupLoad   = 'Restore from a backup'
        backupReset  = 'Wipe the world and start over (fresh install)'
        backupPick   = 'Choose a backup file'
        backupNone   = 'There is no backup in the backups folder yet. Save one first.'
        repairDb     = 'REPAIR DATABASE ACCESS'
        dbAccess     = 'DATABASE LOGIN (NAVICAT)'
        gmPanel      = 'GM PANEL F9 (BETA)'
        updateClient = 'UPDATE CLIENT'
        dbAccessTitle = 'Database connection details'
        dbAccessHint = 'Enter these in Navicat, HeidiSQL or DBeaver (MySQL/MariaDB, TCP connection). root sees everything, the game account only the game databases. The database listens on this computer only. If it rejects the password, click REPAIR DATABASE ACCESS - it sets both accounts to the passwords in .env. Never paste these passwords on Discord.'
        dbAccessProtoNote = 'On the 2.x files items and monsters (item_proto, mob_proto) live in the world database; player.item_proto and player.mob_proto are views. Changes in world survive a server restart.'
        startupUpdateTitle = 'Update available'
        startupServerUpdate = 'A newer server version was found: {0}' + [Environment]::NewLine + '(installed: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Update now?' + [Environment]::NewLine + [Environment]::NewLine + 'Characters, items and bots stay as they are. The server is rebuilt - progress in the log below. NO postpones the question until the next version; the UPDATE button always works.'
        startupClientUpdate = 'A newer client version was found: {0}' + [Environment]::NewLine + '(installed: {1})' + [Environment]::NewLine + [Environment]::NewLine + 'Update the client now?' + [Environment]::NewLine + [Environment]::NewLine + 'Replaces the pack files in the client folder; the previous ones go to backups\client. NO postpones the question until the next version; the UPDATE CLIENT button always works.'
        dbAccessOpenEnv = 'OPEN .ENV FILE'
        dbAccessNoEnv = 'No linux-port\docker\.env yet - start the server (PLAY) once, the launcher creates it.'
        language     = 'LANGUAGE / JEZYK: ENGLISH'
        ready        = 'Ready.'
        footer       = '"Stop and save" never deletes characters or bot progress. It never uses docker compose down -v.'
        botDialog    = 'Number of playing bots'
        difficulty   = 'DIFFICULTY'
        difficultyDialog = 'World difficulty'
        coop         = 'CO-OP: PLAY WITH FRIENDS'
        apply        = 'Apply'
        cancel       = 'Cancel'
        panelDialog  = 'Which panel should open?'
        panelInfo    = 'Both panels show the same world and run at the same time.'
        panelClassic = "Original panel`r`nmap and controls"
        panelSeban   = "Advanced panel by seban latino`r`nprofiles, rankings, economy, load"
        panelPw      = 'I cannot log in (panel password)'
        importDialog = 'Import a database from another installation'
        importInfo   = 'Pick the source installation. Its world - characters, levels, equipment - is copied into this one.'
        importOk     = 'Import'
        langSwitched = 'Language changed. Restart the launcher to see it.'
    }
}

$script:Lang = 'pl'
try {
    $storedLang = (Get-LauncherConfig).language
    if ($storedLang -eq 'en') { $script:Lang = 'en' }
}
catch { }

if ($UiSelfTest) { $script:Lang = $UiLanguage }

function T {
    param([Parameter(Mandatory = $true)][string]$Key)
    $table = $script:Strings[$script:Lang]
    if ($table -and $table.ContainsKey($Key)) { return [string]$table[$Key] }
    return [string]$script:Strings['pl'][$Key]
}

function Switch-LauncherLanguage {
    $config = Get-LauncherConfig
    $config.language = if ($script:Lang -eq 'en') { 'pl' } else { 'en' }
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    $script:Lang = $config.language
    Write-LocalLog ("Language: {0}" -f $config.language)
    [Windows.Forms.MessageBox]::Show((T 'langSwitched'), (T 'formTitle'),
        [Windows.Forms.MessageBoxButtons]::OK,
        [Windows.Forms.MessageBoxIcon]::Information) | Out-Null
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

    if ($script:versionLabel) { Update-VersionFooter }
}

# apt/dpkg chatter from a first image build, plus Docker's note about the data
# volume it did not create itself. Both are harmless, but players read the
# word "warning" next to their database and reach for `down -v`, which is the
# one command that would actually destroy the world. Kept in the session log,
# kept out of the on-screen box so the interesting lines stay readable.
$script:M2_NOISY_BUILD = 'already exists but was not created by Docker Compose|Get:\d|Unpacking |Selecting previously|Preparing to unpack|Reading database|Setting up |Suggested packages:|Recommended packages:|The following NEW packages|The following packages will be|debconf:'

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

function Set-ActionPhase {
    # One place records the phase, so the clock under it always starts when the
    # phase actually changes. Without that clock a start that had stopped
    # looked exactly like a start that was working: the line read
    # "m2zip-db: Healthy" for eight minutes while the migration behind it
    # could not reach the database at all, and nothing said which it was.
    param([Parameter(Mandatory = $true)][string]$Phase, [int]$Step = 0, [int]$Total = 0)
    if ($script:activePhase -ne $Phase) {
        $script:activePhase = $Phase
        $script:activePhaseSince = Get-Date
    }
    $script:activePhaseStep = $Step
    $script:activePhaseTotal = $Total
}

function Update-ActionPhase {
    # Turn BuildKit / Compose chatter into a phase name and a step count, so the
    # progress bar and the status line can show real movement during the long
    # first build instead of an endless marquee.
    param([Parameter(Mandatory = $true)][string]$Line)
    $step = [Regex]::Match($Line, '^\s*#\d+\s+\[([^\]]+?)\s+(\d+)/(\d+)\]')
    if ($step.Success) {
        Set-ActionPhase $step.Groups[1].Value ([int]$step.Groups[2].Value) ([int]$step.Groups[3].Value)
        if (-not $script:activeBuildNoticed) {
            $script:activeBuildNoticed = $true
            Write-LocalLog 'Trwa budowanie obrazów serwera. Przy pierwszym uruchomieniu to normalnie kilkanaście–kilkadziesiąt minut — nie przerywaj.'
        }
        return
    }
    if ($Line -match '^\[faza\]\s*(.+?)\s*(\(|$)') {
        Set-ActionPhase $Matches[1]
        return
    }
    if ($Line -match 'transferring context:\s*([\d.]+\s*[kKMG]?B)') {
        Set-ActionPhase "przesyłanie plików do budowy ($($Matches[1]))"
        return
    }
    # The database migration is where a start sits longest, and it says plainly
    # what it is waiting for. Show that instead of the last container line from
    # a minute ago, so "still waiting" cannot be mistaken for progress.
    if ($Line -match 'still waiting for the database \((\d+)s\)') {
        Set-ActionPhase ('migracja bazy czeka na bazę ({0} s)' -f $Matches[1])
        return
    }
    if ($Line -match 'the database is not answering yet|Unknown server host') {
        Set-ActionPhase 'migracja bazy: baza nie odpowiada'
        return
    }
    if ($Line -match 'waiting for the complete mt2009 schema') {
        Set-ActionPhase 'migracja bazy: czekam na schemat'
        return
    }
    $container = [Regex]::Match($Line, 'Container\s+(\S+)\s+(Creating|Created|Starting|Started|Waiting|Healthy|Recreate|Stopping|Stopped)')
    if ($container.Success) {
        Set-ActionPhase "$($container.Groups[1].Value): $($container.Groups[2].Value)"
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
        # How long THIS phase has lasted, not only the whole action: a build
        # step that takes four minutes is normal, the same container line for
        # four minutes is not, and the two used to look identical.
        $inPhase = if ($script:activePhaseSince) { (Get-Date) - $script:activePhaseSince } else { [TimeSpan]::Zero }
        $text += '   —   {0} ({1:mm\:ss})' -f $script:activePhase, $inPhase
        if ($inPhase.TotalSeconds -ge $script:activeStallSeconds) {
            $text += '  ⚠ bez zmian — sprawdź DIAGNOSTYKA'
            if (-not $script:activeStallNoticed) {
                $script:activeStallNoticed = $true
                Write-LocalLog ("Od {0:N0} min nic się nie zmienia na etapie: {1}. To nie musi być awaria (duży świat wstaje wolno), ale jeśli potrwa dalej, użyj DIAGNOSTYKA i ZBIERZ LOGI." -f $inPhase.TotalMinutes, $script:activePhase)
            }
        }
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
    if ($exitCode -eq 0 -and $action -like 'Update*' -and (Get-LauncherFingerprint) -ne $script:launcherFingerprint) {
        $answer = [Windows.Forms.MessageBox]::Show(
            "Launcher zostal zaktualizowany.`r`n`r`nTo okno dziala jeszcze na starej wersji - nowe przyciski i poprawki pojawia sie dopiero po ponownym uruchomieniu.`r`n`r`nUruchomic launcher ponownie teraz?",
            'Aktualizacja zainstalowana', 'YesNo', 'Information')
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            Restart-Launcher
            return
        }
        $script:launcherFingerprint = Get-LauncherFingerprint
    }
    if ($exitCode -eq 0 -and $launchClient) { Start-ConfiguredClient }
    if ($script:offerClientAfterAction) {
        $script:offerClientAfterAction = $false
        if ($exitCode -eq 0) { Offer-ClientUpdate }
    }
    if ($exitCode -eq 0 -and $openSupport -and (Test-Path $supportDirectory)) {
        Start-Process explorer.exe -ArgumentList ('"{0}"' -f $supportDirectory)
        if ($contactUrl) { Start-Process $contactUrl }
    }
}

function Confirm-DockerReady {
    # Without this the docker CLI just returns nothing and the caller reports
    # "no databases found", which reads as data loss rather than a stopped engine.
    if (Test-M2DockerRunning) { return $true }
    [Windows.Forms.MessageBox]::Show(
        "Silnik Dockera jest zatrzymany, wiec nie widac zadnych baz.`r`n`r`nKliknij przycisk URUCHOM DOCKER, poczekaj az status u gory zmieni sie na - Docker: GOTOWY - i sprobuj ponownie.`r`n`r`nZadne dane nie zginely: bazy sa na dysku, tylko Docker ich teraz nie pokazuje.",
        'Docker jest zatrzymany', 'OK', 'Warning') | Out-Null
    return $false
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

function Get-SpawnPlanFromEnv {
    # The spawn plan as .env has it; 1 / 0 / 24 when the keys are not there yet.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $plan = @{ Minutes = 1; Late = 0; Hours = 24 }
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $content = [IO.File]::ReadAllText($envPath)
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_SPAWN_WINDOW_MINUTES=(\d+)\s*$')
        if ($m.Success) { $plan.Minutes = [int]$m.Groups[1].Value }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_LATE_JOINERS=(\d+)\s*$')
        if ($m.Success) { $plan.Late = [int]$m.Groups[1].Value }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_LATE_JOIN_HOURS=(\d+)\s*$')
        if ($m.Success) { $plan.Hours = [int]$m.Groups[1].Value }
    }
    return $plan
}

function Get-KingdomPlanFromEnv {
    # PLAYERBOT_AUTOSPAWN_PER_KINGDOM with the three numbers, and the second
    # channel with its share, as .env has them; off, 0/0/0 and 40% otherwise.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $plan = @{ PerKingdom = $false; Shinsoo = 0; Chunjo = 0; Jinno = 0; Channel2 = $false; Channel2Share = 40 }
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $content = [IO.File]::ReadAllText($envPath)
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_AUTOSPAWN_PER_KINGDOM=(\d+)\s*$')
        if ($m.Success) { $plan.PerKingdom = $m.Groups[1].Value -eq '1' }
        foreach ($pair in @(@('Shinsoo', 'PLAYERBOT_AUTOSPAWN_SHINSOO'), @('Chunjo', 'PLAYERBOT_AUTOSPAWN_CHUNJO'), @('Jinno', 'PLAYERBOT_AUTOSPAWN_JINNO'))) {
            $m = [Regex]::Match($content, '(?m)^' + $pair[1] + '=(\d+)\s*$')
            if ($m.Success) { $plan[$pair[0]] = [int]$m.Groups[1].Value }
        }
        $m = [Regex]::Match($content, '(?m)^M2_PLAYERBOT_CH2=(\d+)\s*$')
        if ($m.Success) { $plan.Channel2 = $m.Groups[1].Value -eq '1' }
        $m = [Regex]::Match($content, '(?m)^PLAYERBOT_CH2_SHARE=(\d+)\s*$')
        if ($m.Success) { $plan.Channel2Share = [int]$m.Groups[1].Value }
    }
    return $plan
}

function Get-DifficultyFromEnv {
    # M2_DIFFICULTY and the hour counts, as .env has them; easy and zeros when
    # the keys are not there yet (an older .env, which start-server.ps1 fills in).
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $level = 'easy'; $bio = '0'; $horse = '0'; $book = '0'; $botBook = '0'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $content = [IO.File]::ReadAllText($envPath)
        $m = [Regex]::Match($content, '(?m)^M2_DIFFICULTY=(\S+)\s*$')
        if ($m.Success) { $level = $m.Groups[1].Value.Trim().ToLowerInvariant() }
        $m = [Regex]::Match($content, '(?m)^M2_BIOLOGIST_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $bio = $m.Groups[1].Value.Trim() }
        $m = [Regex]::Match($content, '(?m)^M2_HORSE_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $horse = $m.Groups[1].Value.Trim() }
        $m = [Regex]::Match($content, '(?m)^M2_BOOK_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $book = $m.Groups[1].Value.Trim() }
        $m = [Regex]::Match($content, '(?m)^M2_BOT_BOOK_WAIT_HOURS=(\S+)\s*$')
        if ($m.Success) { $botBook = $m.Groups[1].Value.Trim() }
    }
    if ($level -notin @('easy', 'medium', 'hard', 'custom')) { $level = 'easy' }
    return @{ Level = $level; Biologist = $bio; Horse = $horse; Book = $book; BotBook = $botBook }
}

function Show-DifficultyDialog {
    # Four presets as radio buttons and the hour counts custom reads; the
    # numbers are what the migrate service turns into the event flags at the
    # next start (quest/m2_difficulty.lua, and the engine and the bots for the
    # skill books), so the dialog says a restart is needed. Returns
    # @{ Level; Biologist; Horse; Book; BotBook } or $null.
    param([hashtable]$Current)
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'difficultyDialog')
    $dialog.Size = [Drawing.Size]::new(560, 470)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = "Ile czeka się u Biologa, u Stajennego (kucyk, Księgi Konia, treningi medalami) i na kolejną księgę umiejętności? Biolog i Stajenny dotyczą graczy; księgi mają osobny czas dla graczy i dla botów.`r`nZmiana wymaga restartu serwera (panel WWW zmienia to samo od razu)."
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(520, 58)
    $dialog.Controls.Add($info)

    $labels = @{
        easy   = 'Łatwy - bez czekania u Biologa, Stajennego i na księgi (tak jak dotąd)'
        medium = 'Średni - Biolog 8 h; kucyk i Księgi Konia 4 h; treningi 6/7 h; księgi 7 h'
        hard   = 'Trudny - jak w oryginale: Biolog 24 h; kucyk i Księgi 12 h; treningi 18/21 h; księgi 21 h'
        custom = 'Własny - godziny poniżej'
    }
    $radios = @{}
    $y = 76
    foreach ($level in @('easy', 'medium', 'hard', 'custom')) {
        $radio = [Windows.Forms.RadioButton]::new()
        $radio.Name = "level_$level"
        $radio.Text = $labels[$level]
        $radio.Location = [Drawing.Point]::new(18, $y)
        $radio.Size = [Drawing.Size]::new(516, 26)
        $radio.Checked = ($Current.Level -eq $level)
        $dialog.Controls.Add($radio)
        $radios[$level] = $radio
        $y += 30
    }

    $bioLabel = [Windows.Forms.Label]::new()
    $bioLabel.Text = 'Biolog: godzin między oddaniami'
    $bioLabel.Location = [Drawing.Point]::new(40, $y + 8)
    $bioLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($bioLabel)
    $bioBox = [Windows.Forms.NumericUpDown]::new()
    $bioBox.Name = 'bioHours'
    $bioBox.DecimalPlaces = 1
    $bioBox.Increment = 0.5
    $bioBox.Minimum = 0
    $bioBox.Maximum = 720
    $bioBox.Location = [Drawing.Point]::new(310, $y + 5)
    $bioBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($bioBox)

    $horseLabel = [Windows.Forms.Label]::new()
    $horseLabel.Text = 'Stajenny: godzin na kucyka, Księgę i trening'
    $horseLabel.Location = [Drawing.Point]::new(40, $y + 38)
    $horseLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($horseLabel)
    $horseBox = [Windows.Forms.NumericUpDown]::new()
    $horseBox.Name = 'horseHours'
    $horseBox.DecimalPlaces = 1
    $horseBox.Increment = 0.5
    $horseBox.Minimum = 0
    $horseBox.Maximum = 720
    $horseBox.Location = [Drawing.Point]::new(310, $y + 35)
    $horseBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($horseBox)

    $bookLabel = [Windows.Forms.Label]::new()
    $bookLabel.Text = 'Księgi umiejętności - gracze: godzin'
    $bookLabel.Location = [Drawing.Point]::new(40, $y + 68)
    $bookLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($bookLabel)
    $bookBox = [Windows.Forms.NumericUpDown]::new()
    $bookBox.Name = 'bookHours'
    $bookBox.DecimalPlaces = 1
    $bookBox.Increment = 0.5
    $bookBox.Minimum = 0
    $bookBox.Maximum = 720
    $bookBox.Location = [Drawing.Point]::new(310, $y + 65)
    $bookBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($bookBox)

    $botBookLabel = [Windows.Forms.Label]::new()
    $botBookLabel.Text = 'Księgi umiejętności - boty: godzin'
    $botBookLabel.Location = [Drawing.Point]::new(40, $y + 98)
    $botBookLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($botBookLabel)
    $botBookBox = [Windows.Forms.NumericUpDown]::new()
    $botBookBox.Name = 'botBookHours'
    $botBookBox.DecimalPlaces = 1
    $botBookBox.Increment = 0.5
    $botBookBox.Minimum = 0
    $botBookBox.Maximum = 720
    $botBookBox.Location = [Drawing.Point]::new(310, $y + 95)
    $botBookBox.Size = [Drawing.Size]::new(90, 24)
    $dialog.Controls.Add($botBookBox)

    $toDecimal = {
        param([string]$Text)
        $n = 0.0
        if ([double]::TryParse("$Text".Trim().Replace(',', '.'), [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture, [ref]$n)) {
            # [double] on both sides: with an int beside it PowerShell picks
            # Math.Min(int, int) and rounds 0.5 to 0, so a custom half hour
            # showed as 0 and Apply wrote the 0 back.
            return [decimal][Math]::Max([double]0, [Math]::Min([double]720, $n))
        }
        return [decimal]0
    }
    $bioBox.Value = & $toDecimal $Current.Biologist
    $horseBox.Value = & $toDecimal $Current.Horse
    $bookBox.Value = & $toDecimal $Current.Book
    $botBookBox.Value = & $toDecimal $Current.BotBook

    # The hour boxes belong to "custom"; the presets say their numbers themselves.
    $sync = {
        $form = $this.FindForm()
        if (-not $form) { return }
        $custom = $form.Controls['level_custom'].Checked
        $form.Controls['bioHours'].Enabled = $custom
        $form.Controls['horseHours'].Enabled = $custom
        $form.Controls['bookHours'].Enabled = $custom
        $form.Controls['botBookHours'].Enabled = $custom
    }
    foreach ($radio in $radios.Values) { $radio.Add_CheckedChanged($sync) }
    $bioBox.Enabled = $radios['custom'].Checked
    $horseBox.Enabled = $radios['custom'].Checked
    $bookBox.Enabled = $radios['custom'].Checked
    $botBookBox.Enabled = $radios['custom'].Checked

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (T 'apply')
    $okButton.Location = [Drawing.Point]::new(332, $y + 142)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(438, $y + 142)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = 'easy'
    foreach ($level in $radios.Keys) { if ($radios[$level].Checked) { $chosen = $level } }
    $bio = $bioBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $horse = $horseBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $book = $bookBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $botBook = $botBookBox.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return @{ Level = $chosen; Biologist = $bio; Horse = $horse; Book = $book; BotBook = $botBook }
}

function Show-FreshWorldDialog {
    # The rates a world about to be made starts on, and whether its bots wait
    # at the door. Both reach the migrator through .env and are read before the
    # cores come up, so this is the only moment they can be chosen without
    # something already happening in the world - which is what the window is
    # for (NerrVoVy, 20 September). And whether a player's new character gets
    # the apprentice chest (seban latino, 22 September). Returns
    # @{ Exp; Drop; Yang; Hold; Starter } or $null.
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = 'Nowy świat - ustawienia na start'
    $dialog.Size = [Drawing.Size]::new(560, 426)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = ("Jak szybko ma iść nowy świat? 100% to tyle, ile w oryginalnej grze.`r`n" +
                  "Te liczby wchodzą w życie, zanim pojawi się pierwszy bot - później zmienia się je w panelu.")
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(520, 44)
    $dialog.Controls.Add($info)

    $presets = @(
        @{ Key = 'normal'; Text = 'Normalnie - 100% doświadczenia, 100% dropu, 100% yang'; Exp = 100; Drop = 100; Yang = 100 },
        @{ Key = 'relaxed'; Text = 'Spokojnie - 300% / 200% / 200%'; Exp = 300; Drop = 200; Yang = 200 },
        @{ Key = 'fast'; Text = 'Szybko - 1000% / 500% / 500%'; Exp = 1000; Drop = 500; Yang = 500 },
        @{ Key = 'custom'; Text = 'Własne liczby - poniżej'; Exp = 0; Drop = 0; Yang = 0 }
    )
    $radios = @{}
    $y = 62
    foreach ($preset in $presets) {
        $radio = [Windows.Forms.RadioButton]::new()
        $radio.Name = ('rate_' + $preset.Key)
        $radio.Text = $preset.Text
        $radio.Location = [Drawing.Point]::new(18, $y)
        $radio.Size = [Drawing.Size]::new(516, 26)
        $radio.Checked = ($preset.Key -eq 'normal')
        $dialog.Controls.Add($radio)
        $radios[$preset.Key] = $radio
        $y += 30
    }

    $boxes = @{}
    $x = 40
    foreach ($field in @(
            @{ Name = 'exp'; Label = 'Doświadczenie %' },
            @{ Name = 'drop'; Label = 'Drop %' },
            @{ Name = 'yang'; Label = 'Yang %' })) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $field.Label
        $label.Location = [Drawing.Point]::new($x, $y + 10)
        $label.Size = [Drawing.Size]::new(120, 22)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.NumericUpDown]::new()
        $box.Name = ('num_' + $field.Name)
        $box.Minimum = 1
        $box.Maximum = 10000
        $box.Increment = 50
        $box.Value = 100
        $box.Location = [Drawing.Point]::new($x, $y + 34)
        $box.Size = [Drawing.Size]::new(110, 24)
        $dialog.Controls.Add($box)
        $boxes[$field.Name] = $box
        $x += 160
    }
    $y += 70

    $holdBox = [Windows.Forms.CheckBox]::new()
    $holdBox.Text = 'Wstrzymaj boty po starcie (wpuszczę je sam, przyciskiem w panelu)'
    $holdBox.Location = [Drawing.Point]::new(18, $y + 6)
    $holdBox.Size = [Drawing.Size]::new(516, 26)
    $holdBox.Checked = $false
    $dialog.Controls.Add($holdBox)
    $y += 34

    $starterBox = [Windows.Forms.CheckBox]::new()
    $starterBox.Text = 'Skrzynia Ucznia dla nowych postaci graczy (przy pierwszym logowaniu)'
    $starterBox.Location = [Drawing.Point]::new(18, $y + 6)
    $starterBox.Size = [Drawing.Size]::new(516, 26)
    $starterBox.Checked = $true
    $dialog.Controls.Add($starterBox)
    $y += 34

    # The boxes belong to "własne"; a preset says its own numbers.
    $sync = {
        $form = $this.FindForm()
        if (-not $form) { return }
        $custom = $form.Controls['rate_custom'].Checked
        foreach ($name in @('num_exp', 'num_drop', 'num_yang')) { $form.Controls[$name].Enabled = $custom }
    }
    foreach ($radio in $radios.Values) { $radio.Add_CheckedChanged($sync) }
    foreach ($box in $boxes.Values) { $box.Enabled = $false }

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = 'Dalej'
    $okButton.Location = [Drawing.Point]::new(332, $y + 16)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(438, $y + 16)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = 'normal'
    foreach ($key in $radios.Keys) { if ($radios[$key].Checked) { $chosen = $key } }
    $values = @{ Exp = 100; Drop = 100; Yang = 100 }
    if ($chosen -eq 'custom') {
        $values = @{ Exp = [int]$boxes['exp'].Value; Drop = [int]$boxes['drop'].Value; Yang = [int]$boxes['yang'].Value }
    }
    else {
        foreach ($preset in $presets) {
            if ($preset.Key -eq $chosen) { $values = @{ Exp = $preset.Exp; Drop = $preset.Drop; Yang = $preset.Yang } }
        }
    }
    $hold = $(if ($holdBox.Checked) { 1 } else { 0 })
    $starter = $(if ($starterBox.Checked) { 1 } else { 0 })
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return @{ Exp = $values.Exp; Drop = $values.Drop; Yang = $values.Yang; Hold = $hold; Starter = $starter }
}

function Get-LauncherFingerprint {
    # An update replaces the launcher's own files, but this process already read
    # them - the new buttons cannot appear until it restarts.
    $stamps = New-Object System.Collections.Generic.List[string]
    foreach ($file in @($PSCommandPath, $cliLauncher, $modulePath, $diagnosticsModulePath)) {
        if (Test-Path -LiteralPath $file -PathType Leaf) {
            $stamps.Add(((Get-Item -LiteralPath $file).LastWriteTimeUtc.Ticks).ToString())
        }
    }
    return ($stamps -join '|')
}

function Restart-Launcher {
    # The new launcher is a console program (the .bat's cmd.exe, or
    # powershell.exe), and a console program whose parent exits in the same
    # breath can fail to initialise: on Windows 11, where the new terminal
    # takes over every console window, that is cmd.exe's "Aplikacja nie zostala
    # wlasciwie uruchomiona (0xc0000142)" after an update (Urtopy, 19 September,
    # the launcher started from its desktop shortcut). So this window stays up
    # until the new one has run for a few seconds, and a start that died on the
    # way is tried once more straight through powershell.exe.
    $batch = Join-Path $root 'Metin2-Launcher-GUI.bat'
    $attempts = @()
    if (Test-Path -LiteralPath $batch -PathType Leaf) {
        $attempts += ,@($batch)
    }
    # -STA matters: WinForms will not start without it.
    $attempts += ,@('powershell.exe', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', ('"{0}"' -f $PSCommandPath))
    $lastError = ''
    foreach ($attempt in $attempts) {
        try {
            if ($attempt.Count -gt 1) {
                $started = Start-Process -FilePath $attempt[0] -WorkingDirectory $root -PassThru `
                    -ArgumentList ($attempt[1..($attempt.Count - 1)])
            }
            else {
                $started = Start-Process -FilePath $attempt[0] -WorkingDirectory $root -PassThru
            }
        }
        catch {
            $lastError = $_.Exception.Message
            Write-LocalLog ("Restart launchera przez {0} nieudany: {1}" -f $attempt[0], $lastError)
            continue
        }
        $deadline = [DateTime]::UtcNow.AddSeconds(4)
        while ($started -and -not $started.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            [Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 200
        }
        if (-not $started -or -not $started.HasExited -or $started.ExitCode -eq 0) {
            Write-LocalLog 'Uruchamiam launcher ponownie po aktualizacji.'
            $script:form.Close()
            return
        }
        $lastError = 'kod 0x{0:X8}' -f $started.ExitCode
        Write-LocalLog ("Nowy launcher ({0}) zakonczyl sie od razu, {1}." -f $attempt[0], $lastError)
    }
    [Windows.Forms.MessageBox]::Show(
        ("Nie udalo sie uruchomic launchera ponownie ({0}).`r`n`r`nZamknij to okno i uruchom launcher skrotem z pulpitu." -f $lastError),
        'Restart launchera', 'OK', 'Warning') | Out-Null
}

function Get-InstalledServerVersion {
    # Same reasoning as Read-State in the text launcher: while a rebuild is
    # outstanding the files on disk are ahead of the containers, so the version
    # they claim must not be used to decide that nothing needs doing.
    if (Test-Path -LiteralPath (Join-Path $root '.m2launcher-rebuild-pending') -PathType Leaf) {
        return 'unknown'
    }
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

function Get-InstalledClientVersion {
    # What a client update recorded, else what the full package shipped
    # (CLIENT_VERSION beside VERSION, put there by New-M2DeployTree.ps1).
    $statePath = Join-Path $root '.m2launcher-state.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$state.client -and [string]$state.client -ne 'unknown') { return ([string]$state.client).Trim() }
        }
        catch { }
    }
    $marker = Join-Path $root 'CLIENT_VERSION'
    if (Test-Path -LiteralPath $marker -PathType Leaf) {
        return (Get-Content -LiteralPath $marker -Raw).Trim()
    }
    return 'unknown'
}

# The versions the player said NO to at startup, so the same question is not
# asked at every start - a newer version asks again. Its own file: Save-State
# in the text launcher rewrites .m2launcher-state.json with three fields only.
$script:offersPath = Join-Path $root '.m2launcher-offers.json'
function Read-DeclinedOffers {
    $declined = @{ server = ''; client = '' }
    if (Test-Path -LiteralPath $script:offersPath -PathType Leaf) {
        try {
            $saved = Get-Content -LiteralPath $script:offersPath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$saved.server) { $declined.server = [string]$saved.server }
            if ([string]$saved.client) { $declined.client = [string]$saved.client }
        }
        catch { }
    }
    return $declined
}
function Save-DeclinedOffer {
    param([string]$Component, [string]$Version)
    $declined = Read-DeclinedOffers
    $declined[$Component] = $Version
    try {
        [pscustomobject]$declined | ConvertTo-Json | Set-Content -LiteralPath $script:offersPath -Encoding UTF8
    }
    catch { }
}

function Test-VersionNewer {
    param([string]$Installed, [string]$Available)
    if (-not $Available) { return $false }
    if (-not $Installed -or $Installed -eq 'unknown') { return $true }
    return -not $Installed.Trim().Equals($Available.Trim(), [StringComparison]::OrdinalIgnoreCase)
}

$script:latestManifest = $null
$script:offerClientAfterAction = $false
$script:startupOfferDone = $false

function Offer-ClientUpdate {
    # Only on the 2.x line: there the manifest's client component is the
    # ordinary client package. On r40250 it is the experimental GM panel,
    # which nobody should be nagged into at startup.
    if (-not $script:clientUpdateIsPlain -or -not $script:latestManifest) { return }
    $clientProperty = $script:latestManifest.PSObject.Properties['client']
    if (-not $clientProperty -or -not $clientProperty.Value -or -not [string]$clientProperty.Value.version) { return }
    $available = ([string]$clientProperty.Value.version).Trim()
    $installed = Get-InstalledClientVersion
    if (-not (Test-VersionNewer -Installed $installed -Available $available)) { return }
    if ((Read-DeclinedOffers).client -eq $available) { return }
    $config = Get-LauncherConfig
    if (-not [string]$config.clientRoot) {
        Write-LocalLog "Dostepna wersja klienta $available, ale folder klienta nie jest ustawiony - pomijam pytanie."
        return
    }
    $answer = [Windows.Forms.MessageBox]::Show(
        ((T 'startupClientUpdate') -f $available, $installed),
        (T 'startupUpdateTitle'), 'YesNo', 'Question')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) {
        Save-DeclinedOffer -Component 'client' -Version $available
        Write-LocalLog "Aktualizacja klienta $available odlozona."
        return
    }
    Start-LauncherAction -Action 'UpdateClient' -Yes
}

function Offer-StartupUpdates {
    # Once per session, on the first manifest read: the server first, and the
    # client after the server action has finished (two actions cannot run at
    # once), or right away when the server is current.
    if ($script:startupOfferDone -or -not $script:latestManifest) { return }
    $script:startupOfferDone = $true
    if ($script:activeProcess -and -not $script:activeProcess.HasExited) { return }
    $installed = Get-InstalledServerVersion
    $available = $script:latestServerVersion
    if ($available -and (Test-VersionNewer -Installed $installed -Available $available) -and
            (Read-DeclinedOffers).server -ne $available) {
        $answer = [Windows.Forms.MessageBox]::Show(
            ((T 'startupServerUpdate') -f $available, $installed),
            (T 'startupUpdateTitle'), 'YesNo', 'Question')
        if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
            $script:offerClientAfterAction = $true
            Start-LauncherAction -Action 'UpdateServer' -Yes
            return
        }
        Save-DeclinedOffer -Component 'server' -Version $available
        Write-LocalLog "Aktualizacja serwera $available odlozona."
    }
    Offer-ClientUpdate
}

function Update-BotDialogValueLabel {
    # The heading of the bot dialog says what the core will start: the one
    # number, or the sum of the three while each kingdom has its own.
    param($Form)
    if (-not $Form) { return }
    $label = $Form.Controls['valueLabel']
    $check = $Form.Controls['kingdomCheck']
    if (-not $label -or -not $check) { return }
    if ($check.Checked) {
        $sum = [int]$Form.Controls['shinsooBox'].Value + [int]$Form.Controls['chunjoBox'].Value + [int]$Form.Controls['jinnoBox'].Value
        $label.Text = "Boty: $sum (osobno dla królestw)"
    }
    else { $label.Text = "Boty: $([int]$Form.Controls['botBar'].Value)" }
}

function Show-BotCountDialog {
    # Slider instead of a typed number: the range is a property of the world, and
    # dragging is far friendlier than guessing a value. The maximum matches the
    # canonical cohort the seed creates - 1500 for Chunjo alone (PID 4..1503) and
    # 2500 once the other two kingdoms are switched on (M2_PLAYERBOT_KINGDOMS=1,
    # PID 4..2503). It stopped at 1500 while the world already held 2500, so a
    # thousand seeded bots could not be asked for from here at all. Asking for
    # more than a world holds is safe and always was: the core spawns what its
    # registry has and logs requested/registered/started.
    # Under the slider, the spawn plan: the window the cohort arrives over and
    # the second cohort with its hours - "1000 w 15 minut, a dodatkowe 500 w
    # ciagu 24 godzin". Below that, the operator's own number per kingdom
    # (Greess's "Indywidualne wartosci dla krolestw") and the second channel.
    # Returns @{ Count; Minutes; Late; Hours; PerKingdom; Shinsoo; Chunjo;
    # Jinno; Channel2; Channel2Share } or $null.
    param([int]$Current = 350, [hashtable]$Plan = @{ Minutes = 1; Late = 0; Hours = 24 },
        [hashtable]$Kingdoms = @{ PerKingdom = $false; Shinsoo = 0; Chunjo = 0; Jinno = 0; Channel2 = $false; Channel2Share = 40 })
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'botDialog')
    $dialog.Size = [Drawing.Size]::new(480, 606)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = "Ilu botów ma grać jednocześnie?`r`nEfektywny limit to liczba botów w Twoim świecie: 1500 dla samego Chunjo,`r`n2500 przy włączonych trzech królestwach. Zmiana wymaga restartu serwera."
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
    $bar.Maximum = 2500
    $bar.TickFrequency = 50
    $bar.SmallChange = 1
    $bar.LargeChange = 25
    $bar.Location = [Drawing.Point]::new(12, 104)
    $bar.Size = [Drawing.Size]::new(442, 45)
    $bar.Value = [Math]::Max(0, [Math]::Min(2500, $Current))
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

    $planInfo = [Windows.Forms.Label]::new()
    $planInfo.Text = "Wejście stopniowe: tylu botów wchodzi w ciągu podanych minut od startu,`r`na dodatkowe dołączają pojedynczo w ciągu podanych godzin (0 = bez dodatkowych)."
    $planInfo.Location = [Drawing.Point]::new(14, 150)
    $planInfo.Size = [Drawing.Size]::new(440, 34)
    $dialog.Controls.Add($planInfo)

    $rows = @(
        @{ Name = 'minutesBox'; Text = 'Wejście w ciągu (min, 1-180):'; Min = 1; Max = 180; Value = [int]$Plan.Minutes; Y = 188 },
        @{ Name = 'lateBox';    Text = 'Dodatkowych botów później (0-2500):'; Min = 0; Max = 2500; Value = [int]$Plan.Late; Y = 218 },
        @{ Name = 'hoursBox';   Text = 'dołączających w ciągu (h, 1-168):'; Min = 1; Max = 168; Value = [int]$Plan.Hours; Y = 248 }
    )
    foreach ($row in $rows) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $row.Text
        $label.Location = [Drawing.Point]::new(14, $row.Y + 3)
        $label.Size = [Drawing.Size]::new(280, 22)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.NumericUpDown]::new()
        $box.Name = $row.Name
        $box.Minimum = $row.Min
        $box.Maximum = $row.Max
        $box.Value = [Math]::Max($row.Min, [Math]::Min($row.Max, $row.Value))
        $box.Location = [Drawing.Point]::new(300, $row.Y)
        $box.Size = [Drawing.Size]::new(90, 24)
        $dialog.Controls.Add($box)
    }

    # Each kingdom its own number instead of a share of the one above. The
    # core cuts each to the identities that kingdom has.
    $kingdomCheck = [Windows.Forms.CheckBox]::new()
    $kingdomCheck.Name = 'kingdomCheck'
    $kingdomCheck.Text = 'Indywidualne wartości dla królestw'
    $kingdomCheck.Location = [Drawing.Point]::new(14, 282)
    $kingdomCheck.Size = [Drawing.Size]::new(440, 24)
    $kingdomCheck.Checked = [bool]$Kingdoms.PerKingdom
    $dialog.Controls.Add($kingdomCheck)
    $kingdomRows = @(
        @{ Name = 'shinsooBox'; Text = 'Shinsoo (czerwone):'; Color = [Drawing.Color]::FromArgb(220, 40, 40); Value = [int]$Kingdoms.Shinsoo; Y = 310 },
        @{ Name = 'chunjoBox';  Text = 'Chunjo (żółte):';     Color = [Drawing.Color]::FromArgb(235, 200, 30); Value = [int]$Kingdoms.Chunjo; Y = 340 },
        @{ Name = 'jinnoBox';   Text = 'Jinno (niebieskie):'; Color = [Drawing.Color]::FromArgb(40, 110, 220); Value = [int]$Kingdoms.Jinno; Y = 370 }
    )
    foreach ($row in $kingdomRows) {
        $swatch = [Windows.Forms.Panel]::new()
        $swatch.BackColor = $row.Color
        $swatch.Location = [Drawing.Point]::new(34, $row.Y + 3)
        $swatch.Size = [Drawing.Size]::new(18, 18)
        $dialog.Controls.Add($swatch)
        $label = [Windows.Forms.Label]::new()
        $label.Text = $row.Text
        $label.Location = [Drawing.Point]::new(58, $row.Y + 3)
        $label.Size = [Drawing.Size]::new(236, 22)
        $dialog.Controls.Add($label)
        $box = [Windows.Forms.NumericUpDown]::new()
        $box.Name = $row.Name
        $box.Minimum = 0
        $box.Maximum = 2500
        $box.Value = [Math]::Max(0, [Math]::Min(2500, $row.Value))
        $box.Location = [Drawing.Point]::new(300, $row.Y)
        $box.Size = [Drawing.Size]::new(90, 24)
        $box.Enabled = $kingdomCheck.Checked
        $dialog.Controls.Add($box)
    }
    $kingdomCheck.Add_CheckedChanged({
            $form = $this.FindForm()
            if (-not $form) { return }
            foreach ($name in @('shinsooBox', 'chunjoBox', 'jinnoBox')) { $form.Controls[$name].Enabled = $this.Checked }
            $form.Controls['botBar'].Enabled = -not $this.Checked
            Update-BotDialogValueLabel $form
        })
    foreach ($name in @('shinsooBox', 'chunjoBox', 'jinnoBox')) {
        $dialog.Controls[$name].Add_ValueChanged({ Update-BotDialogValueLabel $this.FindForm() })
    }
    $bar.Enabled = -not $kingdomCheck.Checked
    Update-BotDialogValueLabel $dialog

    # The second channel: bots and players, shops on the first channel only.
    $channelCheck = [Windows.Forms.CheckBox]::new()
    $channelCheck.Name = 'channelCheck'
    $channelCheck.Text = 'Drugi kanał (CH2) dla botów i graczy'
    $channelCheck.Location = [Drawing.Point]::new(14, 406)
    $channelCheck.Size = [Drawing.Size]::new(440, 24)
    $channelCheck.Checked = [bool]$Kingdoms.Channel2
    $dialog.Controls.Add($channelCheck)
    $shareLabel = [Windows.Forms.Label]::new()
    $shareLabel.Text = 'Ile procent botów gra na CH2 (10-90):'
    $shareLabel.Location = [Drawing.Point]::new(34, 437)
    $shareLabel.Size = [Drawing.Size]::new(260, 22)
    $dialog.Controls.Add($shareLabel)
    $shareBox = [Windows.Forms.NumericUpDown]::new()
    $shareBox.Name = 'channelShareBox'
    $shareBox.Minimum = 10
    $shareBox.Maximum = 90
    $shareBox.Value = [Math]::Max(10, [Math]::Min(90, [int]$Kingdoms.Channel2Share))
    $shareBox.Location = [Drawing.Point]::new(300, 434)
    $shareBox.Size = [Drawing.Size]::new(90, 24)
    $shareBox.Enabled = $channelCheck.Checked
    $dialog.Controls.Add($shareBox)
    $channelCheck.Add_CheckedChanged({
            $form = $this.FindForm()
            if ($form) { $form.Controls['channelShareBox'].Enabled = $this.Checked }
        })
    $channelInfo = [Windows.Forms.Label]::new()
    $channelInfo.Text = "Serwer rozkłada wtedy boty na dwa rdzenie procesora. Wszystkie sklepy`r`n(botów i graczy) stoją tylko na CH1. Otwiera porty 13010-13012."
    $channelInfo.Location = [Drawing.Point]::new(14, 466)
    $channelInfo.Size = [Drawing.Size]::new(440, 36)
    $dialog.Controls.Add($channelInfo)

    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (T 'apply')
    $okButton.Location = [Drawing.Point]::new(252, 514)
    $okButton.Size = [Drawing.Size]::new(100, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dialog.Controls.Add($okButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(358, 514)
    $cancelButton.Size = [Drawing.Size]::new(96, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $okButton
    $dialog.CancelButton = $cancelButton

    $result = $dialog.ShowDialog()
    $chosen = @{
        Count         = [int]$bar.Value
        Minutes       = [int]$dialog.Controls['minutesBox'].Value
        Late          = [int]$dialog.Controls['lateBox'].Value
        Hours         = [int]$dialog.Controls['hoursBox'].Value
        PerKingdom    = [bool]$dialog.Controls['kingdomCheck'].Checked
        Shinsoo       = [int]$dialog.Controls['shinsooBox'].Value
        Chunjo        = [int]$dialog.Controls['chunjoBox'].Value
        Jinno         = [int]$dialog.Controls['jinnoBox'].Value
        Channel2      = [bool]$dialog.Controls['channelCheck'].Checked
        Channel2Share = [int]$dialog.Controls['channelShareBox'].Value
    }
    $dialog.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK) { return $null }
    return $chosen
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
    # Start-Process joins ArgumentList with spaces and quotes nothing, so a value
    # containing a space arrives at the CLI as two arguments. The default install
    # folder is "Metin2 Singleplayer", so restoring a backup sent
    # "C:\...\Metin2" to -RestoreSource and the tail
    # "Singleplayer\Serwer\backups\db-backup-....zip" to the next positional
    # parameter - which is [int]$BotCount - and every restore died with "Cannot
    # convert value ... to type System.Int32" (NieBijOddam, 13 September).
    # Parameter names pass through untouched; every value is quoted.
    foreach ($extra in @($ExtraArgs)) {
        $text = [string]$extra
        if ($text -match '^-[A-Za-z]') { $arguments += $text }
        else { $arguments += ('"{0}"' -f ($text -replace '"', '\"')) }
    }
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
    $script:activePhaseSince = Get-Date
    $script:activePhaseStep = 0
    $script:activePhaseTotal = 0
    $script:activeBuildNoticed = $false
    # After this long on one phase the status line says so. Four minutes is
    # past every normal compose step and well inside a first build's stages,
    # which carry their own step counter anyway.
    $script:activeStallSeconds = 240
    $script:activeStallNoticed = $false
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
    # @(...) round the whole pipeline, not only its input: Where-Object hands
    # back a bare string when one file is missing, and under Set-StrictMode a
    # string has no .Count - the button threw "The property 'Count' cannot be
    # found" at exactly the player whose package was incomplete.
    $missing = @(@($cliLauncher, $composeFile, $modulePath) | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
    if ($missing.Count -gt 0) {
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

    # This button prepares a package whose game sources are already on disk;
    # it never fetches them, because they are the operator's own r40250 files
    # and only installer\install.ps1 knows how to take them from that package.
    # Until now it said "Paczka jest gotowa" regardless - and a player whose
    # sources were missing pressed it, was told the package was ready, pressed
    # GRAJ and got fifteen Docker errors. Reported from the Discord: "re-running
    # the installer through Install in GUI did not restore the sources" - it
    # could not have, this is not the installer. Say which it is.
    $gameContext = Join-Path $root 'linux-port\docker\game\src'
    foreach ($made in @(Restore-M2EmptyGameContextDirs -ServerRoot $root)) {
        Write-LocalLog ("Odtworzono pusty katalog budowy: " + $made)
    }
    $requiredContext = @(Get-M2RequiredGameContext -ServerRoot $root)
    $missingContext = @($requiredContext | Where-Object { -not (Test-Path -LiteralPath (Join-Path $gameContext $_)) })
    # The database dumps are the other half of what the installer takes out
    # of the package, and the half nobody saw missing until MariaDB came up
    # empty: name them here with the sources.
    $missingContext += @(Get-M2MissingSqlDumps -ServerRoot $root | ForEach-Object { 'mariadb\initdb.d\dumps\' + $_ })
    if ($missingContext.Count -gt 0) {
        $installerPath = Join-Path $root 'installer\install.ps1'
        Write-LocalLog ("Brak zrodel gry w " + $gameContext + ": " + ($missingContext -join ', '))
        [Windows.Forms.MessageBox]::Show(
            ("Ten przycisk przygotowuje paczke, ktora ma juz na dysku zrodla gry - a tu ich nie ma." + [Environment]::NewLine +
             "Brakuje: " + ($missingContext -join ', ') + [Environment]::NewLine + [Environment]::NewLine +
             "Zrodla pochodza z Twojej wlasnej paczki serwera r40250 i zaden przycisk launchera ani zadna aktualizacja ich nie pobiera - " +
             "robi to wylacznie instalator, ktory wyciaga z tej paczki to, czego trzeba." + [Environment]::NewLine + [Environment]::NewLine +
             "Uruchom w PowerShell jako administrator:" + [Environment]::NewLine +
             "  `$env:M2_SRC_ARCHIVE = 'C:\sciezka\do\paczki-r40250.zip'" + [Environment]::NewLine +
             "  & '" + $installerPath + "'" + [Environment]::NewLine + [Environment]::NewLine +
             "Baza, postacie i ustawienia zostaja nietkniete."),
            'Brak zrodel gry - potrzebny instalator', 'OK', 'Warning') | Out-Null
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

$script:launcherFingerprint = Get-LauncherFingerprint

$script:form = [Windows.Forms.Form]::new()
$script:form.Text = (T 'formTitle')
$script:form.Size = [Drawing.Size]::new(780, 806)
$script:form.MinimumSize = [Drawing.Size]::new(780, 764)
$script:form.StartPosition = 'CenterScreen'
$script:form.BackColor = [Drawing.Color]::FromArgb(24, 25, 29)
$script:form.ForeColor = [Drawing.Color]::White
$script:form.Font = [Drawing.Font]::new('Segoe UI', 9)

$title = [Windows.Forms.Label]::new()
$title.Text = (T 'title')
$title.Font = [Drawing.Font]::new('Segoe UI Semibold', 19)
$title.ForeColor = [Drawing.Color]::FromArgb(247, 194, 66)
$title.Location = [Drawing.Point]::new(24, 18)
$title.Size = [Drawing.Size]::new(710, 38)
$script:form.Controls.Add($title)

$subtitle = [Windows.Forms.Label]::new()
$subtitle.Text = (T 'subtitle')
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

$installButton = New-Button (T 'install') 28 128 338 58 ([Drawing.Color]::FromArgb(88, 82, 160))
$playButton = New-Button (T 'play') 388 128 338 58 ([Drawing.Color]::FromArgb(27, 150, 88))
# Whether PLAY opens the client as well. It sits in the gap under the PLAY
# button, so no other control moves; the choice is kept in
# launcher.config.json, because somebody who runs the world for other
# people wants it off every time, not once.
$script:launchClientCheck = [Windows.Forms.CheckBox]::new()
$script:launchClientCheck.Text = (T 'launchClient')
$script:launchClientCheck.Location = [Drawing.Point]::new(390, 187)
$script:launchClientCheck.Size = [Drawing.Size]::new(336, 17)
$script:launchClientCheck.Font = [Drawing.Font]::new('Segoe UI', 8)
$script:launchClientCheck.ForeColor = [Drawing.Color]::Silver
function Update-PlayButtonLabel {
    $playButton.Text = if ($script:launchClientCheck.Checked) { (T 'play') } else { (T 'playNoClient') }
}
# Ustawiane przed podpieciem obslugi zmiany, zeby pierwsze przypisanie nie
# zapisywalo pliku konfiguracji przy samym otwarciu okna.
$script:launchClientCheck.Checked = [bool](Get-LauncherConfig).launchClientOnPlay
Update-PlayButtonLabel
$script:launchClientCheck.Add_CheckedChanged({
    $config = Get-LauncherConfig
    $config.launchClientOnPlay = $script:launchClientCheck.Checked
    Save-M2LauncherConfig -Config $config -ConfigPath $configPath
    Update-PlayButtonLabel
    Write-LocalLog $(if ($script:launchClientCheck.Checked) {
        'GRAJ bedzie uruchamiac takze klienta gry.' } else {
        'GRAJ bedzie uruchamiac sam serwer - klient zostaje wylaczony.' })
})
$script:form.Controls.Add($script:launchClientCheck)

$dockerButton = New-Button (T 'docker') 28 202 218 50
$stopButton = New-Button (T 'stop') 268 202 218 50 ([Drawing.Color]::FromArgb(180, 75, 55))
$panelButton = New-Button (T 'panel') 508 202 218 50 ([Drawing.Color]::FromArgb(180, 125, 35))
$clientButton = New-Button (T 'client') 28 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$updateButton = New-Button (T 'update') 268 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$bundleButton = New-Button (T 'bundle') 508 266 218 48 ([Drawing.Color]::FromArgb(75, 90, 120))
$diagnosticsButton = New-Button (T 'diagnostics') 28 328 218 45 ([Drawing.Color]::FromArgb(45, 110, 190))
$openLogButton = New-Button (T 'openLog') 262 328 218 45 ([Drawing.Color]::FromArgb(58, 62, 72))
$folderButton = New-Button (T 'logFolder') 496 328 230 45 ([Drawing.Color]::FromArgb(58, 62, 72))
$botCountButton = New-Button (T 'botCount') 28 380 218 32 ([Drawing.Color]::FromArgb(120, 95, 40))
$importDbButton = New-Button (T 'importDb') 262 380 218 32 ([Drawing.Color]::FromArgb(70, 120, 90))
$repairDbButton = New-Button (T 'repairDb') 496 380 230 32 ([Drawing.Color]::FromArgb(150, 90, 55))
$dbAccessButton = New-Button (T 'dbAccess') 28 418 218 32 ([Drawing.Color]::FromArgb(70, 100, 130))
# The optional, experimental client half of the GM panel (F9): the server half
# rides in every update, this button fetches the client package from the
# manifest's `client` component and swaps pack/root.eix + root.epk.
$gmPanelButton = New-Button (T 'gmPanel') 262 418 218 32 ([Drawing.Color]::FromArgb(120, 70, 130))
# On the mt2009 line the client update is the ordinary one - the packs the
# server's root points at - and not the experimental GM panel.
$script:clientUpdateIsPlain = ((Get-M2ServerEngine -ServerRoot $root) -ne 'r40250')
if ($script:clientUpdateIsPlain) { $gmPanelButton.Text = (T 'updateClient') }
# Backup, restore and "start over" behind one button: reported from the
# Discord as "the launcher can import a database but nothing says how to
# export one", together with a wish to get back to a fresh install.
$worldBackupButton = New-Button (T 'worldBackup') 496 418 230 32 ([Drawing.Color]::FromArgb(70, 120, 90))
# The world's difficulty - the waits at the Biologist and the stable keeper -
# chosen here and applied at the next start (M2_DIFFICULTY in .env).
$difficultyButton = New-Button (T 'difficulty') 28 456 218 32 ([Drawing.Color]::FromArgb(120, 95, 40))
# COOP (experimental): this world played with friends over the Internet, the
# hosting half for the Patreon testers behind their password (Open-CoopWindow).
# The button exists only when the optional module does.
$coopModulePath = Join-Path $root 'launcher\Metin2Launcher.Coop.psm1'
$coopButton = $null
if (Test-Path -LiteralPath $coopModulePath -PathType Leaf) {
    Import-Module $coopModulePath -Force
    $coopButton = New-Button (T 'coop') 262 456 218 32 ([Drawing.Color]::FromArgb(40, 120, 150))
}

# The language switch sits with the other small buttons rather than in a menu:
# somebody who cannot read the window needs to find it without reading anything.
$languageButton = New-Button (T 'language') 508 702 218 28 ([Drawing.Color]::FromArgb(60, 70, 95))
$languageButton.Add_Click({ Switch-LauncherLanguage })

foreach ($button in @($installButton, $playButton, $dockerButton, $stopButton, $panelButton, $clientButton, $updateButton, $bundleButton, $diagnosticsButton, $openLogButton, $folderButton, $botCountButton, $importDbButton, $repairDbButton, $dbAccessButton, $gmPanelButton, $worldBackupButton, $difficultyButton, $languageButton)) {
    $script:form.Controls.Add($button)
}
if ($coopButton) { $script:form.Controls.Add($coopButton) }

$script:actionStatus = [Windows.Forms.Label]::new()
$script:actionStatus.Text = (T 'ready')
$script:actionStatus.Location = [Drawing.Point]::new(28, 500)
$script:actionStatus.Size = [Drawing.Size]::new(690, 24)
$script:actionStatus.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$script:form.Controls.Add($script:actionStatus)

$script:progress = [Windows.Forms.ProgressBar]::new()
$script:progress.Location = [Drawing.Point]::new(28, 528)
$script:progress.Size = [Drawing.Size]::new(698, 12)
$script:form.Controls.Add($script:progress)

$script:logBox = [Windows.Forms.TextBox]::new()
$script:logBox.Location = [Drawing.Point]::new(28, 550)
$script:logBox.Size = [Drawing.Size]::new(698, 104)
$script:logBox.Multiline = $true
$script:logBox.ReadOnly = $true
$script:logBox.ScrollBars = 'Vertical'
$script:logBox.BackColor = [Drawing.Color]::FromArgb(12, 13, 16)
$script:logBox.ForeColor = [Drawing.Color]::Gainsboro
$script:logBox.Font = [Drawing.Font]::new('Consolas', 8.5)
$script:form.Controls.Add($script:logBox)

$footer = [Windows.Forms.Label]::new()
$footer.Text = (T 'footer')
$footer.Location = [Drawing.Point]::new(28, 660)
$footer.Size = [Drawing.Size]::new(700, 25)
$footer.ForeColor = [Drawing.Color]::DarkGray
$script:form.Controls.Add($footer)


$script:versionLabel = [Windows.Forms.Label]::new()
$script:versionLabel.Location = [Drawing.Point]::new(28, 666)
# Four lines when an update is waiting: the "!! NOWA WERSJA" notice goes above
# the three version lines, and at 54 pixels the client line was cut off.
$script:versionLabel.Size = [Drawing.Size]::new(700, 74)
$script:versionLabel.ForeColor = [Drawing.Color]::Silver
$script:versionLabel.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$script:form.Controls.Add($script:versionLabel)

$script:latestServerVersion = $null
$script:latestClientVersion = $null
$script:latestVersionChecked = $false

function Get-LauncherVersionOnDisk {
    # The launcher ships inside the server package, so the VERSION file beside
    # it is its version. Read at startup for what this window runs, and again
    # for the footer: after an update applied in this session the file is
    # ahead of the process, and the footer says so.
    $path = Join-Path $root 'VERSION'
    try {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $text = (Get-Content -LiteralPath $path -Raw -ErrorAction Stop).Trim()
            if ($text) { return $text }
        }
    }
    catch { }
    return 'nieznana'
}
$script:launcherVersion = Get-LauncherVersionOnDisk

function Set-LatestVersionsFromManifest {
    param($Manifest)
    if (-not $Manifest) { return }
    $serverProperty = $Manifest.PSObject.Properties['server']
    if ($serverProperty -and $serverProperty.Value -and [string]$serverProperty.Value.version) {
        $script:latestServerVersion = ([string]$serverProperty.Value.version).Trim()
    }
    $clientProperty = $Manifest.PSObject.Properties['client']
    if ($clientProperty -and $clientProperty.Value -and [string]$clientProperty.Value.version) {
        $script:latestClientVersion = ([string]$clientProperty.Value.version).Trim()
    }
}

function Update-VersionFooter {
    # The manifest lives behind GitHub's anonymous per-IP budget, so it is read
    # once per session and whenever the player asks for a check - never on the
    # 8-second status timer, which would spend that budget for nothing.
    $installed = Get-InstalledServerVersion
    $installedText = if ($installed -and $installed -ne 'unknown') { $installed } else { 'nieznana (przebudowa w toku)' }
    $latestText = if ($script:latestServerVersion) { $script:latestServerVersion }
        elseif ($script:latestVersionChecked) { 'nie udalo sie sprawdzic' }
        else { 'sprawdzanie...' }
    $latestClientText = if ($script:latestClientVersion) { $script:latestClientVersion }
        elseif ($script:latestVersionChecked) { 'nie udalo sie sprawdzic' }
        else { 'sprawdzanie...' }
    # Three lines, asked for on the Discord: the server, the launcher itself
    # (its newest version is the server package's) and the client.
    $onDisk = Get-LauncherVersionOnDisk
    $launcherText = $script:launcherVersion
    if ($onDisk -ne $script:launcherVersion) {
        $launcherText = '{0} (na dysku {1} - uruchom launcher ponownie)' -f $script:launcherVersion, $onDisk
    }
    $clientInstalled = Get-InstalledClientVersion
    $clientText = if ($clientInstalled -and $clientInstalled -ne 'unknown') { $clientInstalled } else { 'nieznana' }
    $script:versionLabel.Text = ("Serwer: {0}   |   najnowszy: {1}`r`nLauncher: {2}   |   najnowszy: {3}`r`nKlient: {4}   |   najnowszy: {5}" -f
        $installedText, $latestText, $launcherText, $latestText, $clientText, $latestClientText)
    $upToDate = $script:latestServerVersion -and $installed -and $installed -ne 'unknown' -and
        $installed.Equals($script:latestServerVersion, [StringComparison]::OrdinalIgnoreCase)
    # The same question for the client, which has its own version and its own
    # button. A player who has the newest server and an old client saw nothing
    # but a green footer, because only the server was ever compared.
    $clientBehind = $false
    if ($script:latestClientVersion -and $clientInstalled -and $clientInstalled -ne 'unknown') {
        $clientBehind = -not $clientInstalled.Equals(
            $script:latestClientVersion, [StringComparison]::OrdinalIgnoreCase)
    }
    $serverBehind = $script:latestServerVersion -and $installed -and $installed -ne 'unknown' -and -not $upToDate
    # What the blink timer below reads. A colour alone is easy to miss on a
    # window nobody is looking at, and "nie wiedzialem ze jest nowa wersja" is
    # what this is for: the line says so in words as well.
    $script:updateAvailable = [bool]($serverBehind -or $clientBehind)
    if ($script:updateAvailable) {
        $what = if ($serverBehind -and $clientBehind) { 'SERWERA I KLIENTA' }
            elseif ($serverBehind) { 'SERWERA' }
            else { 'KLIENTA' }
        $script:versionLabel.Text = ("!! NOWA WERSJA {0} - kliknij ZAINSTALUJ AKTUALIZACJE`r`n{1}" -f
            $what, $script:versionLabel.Text)
    }
    $script:versionBaseColor = if ($upToDate -and -not $clientBehind) { [Drawing.Color]::LightGreen }
        elseif ($script:latestServerVersion) { [Drawing.Color]::Gold }
        else { [Drawing.Color]::Silver }
    $script:versionLabel.ForeColor = $script:versionBaseColor
}

function Read-LatestServerVersion {
    param([switch]$Force)
    if ($script:latestVersionChecked -and -not $Force) { return }
    $script:latestVersionChecked = $true
    try {
        $config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
        $manifest = Get-M2UpdateManifest -Source ([string]$config.manifestUrl) -TimeoutSec 8
        $script:latestManifest = $manifest
        Set-LatestVersionsFromManifest -Manifest $manifest
    }
    catch { }
    Update-VersionFooter
    Offer-StartupUpdates
}

$installButton.Add_Click({ Install-Or-Prepare })
$playButton.Add_Click({
    $withClient = $script:launchClientCheck.Checked
    if ($withClient) {
        if (-not (Find-ClientExecutable)) {
            if (-not (Select-ClientExecutable)) { return }
        }
    }
    Start-LauncherAction -Action 'Start' -LaunchClient:$withClient
})
$dockerButton.Add_Click({ Start-LauncherAction -Action 'StartDocker' })
$stopButton.Add_Click({
    $answer = [Windows.Forms.MessageBox]::Show(
        'Zatrzymać serwer i Docker Desktop? Postacie, baza i postęp botów zostaną zachowane.',
        'Bezpieczne zatrzymanie', 'YesNo', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) { Start-LauncherAction -Action 'StopAll' }
})
function Get-M2PanelAddresses {
    # Both web panels of the same world, at whatever ports this installation
    # actually publishes them on. The defaults match the compose file; a world
    # whose .env moves a port is followed rather than guessed at.
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $classic = 7788
    $seban = 7790
    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_PANEL_PUBLIC_PORT=(\d+)$' | Select-Object -First 1
        if ($match) { $classic = [int]$match.Matches[0].Groups[1].Value }
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_SEBAN_PANEL_PORT=(\d+)$' | Select-Object -First 1
        if ($match) { $seban = [int]$match.Matches[0].Groups[1].Value }
    }
    return [pscustomobject]@{
        ClassicUrl = "http://127.0.0.1:$classic/map"
        SebanUrl   = "http://127.0.0.1:$seban/"
        ClassicPort = $classic
        SebanPort   = $seban
    }
}

function Show-PanelPasswordDialog {
    # In-process and read-only: an action would print the passphrase through the
    # launcher log, and the launcher log travels in support bundles that get
    # posted on the Discord. Same rule as the database credentials dialog.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    $passphrase = ''
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = [Regex]::Match([IO.File]::ReadAllText($envPath), '(?m)^M2_PANEL_PASSWORD=(.+?)\s*$')
        if ($match.Success) { $passphrase = $match.Groups[1].Value }
    }
    if (-not $passphrase) {
        [Windows.Forms.MessageBox]::Show(
            "W pliku .env nie ma jeszcze hasla do panelu.`r`n`r`nKliknij GRAJ raz - launcher je uzupelni i pokaze.",
            'Haslo do panelu', 'OK', 'Information') | Out-Null
        return
    }

    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = 'Haslo do panelu WWW'
    $dialog.Size = [Drawing.Size]::new(520, 250)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = 'Panel ma jedno haslo i nie ma loginu. Zaznacz je i skopiuj.'
    $info.Location = [Drawing.Point]::new(16, 14)
    $info.Size = [Drawing.Size]::new(480, 20)
    $dialog.Controls.Add($info)

    $box = [Windows.Forms.TextBox]::new()
    $box.Text = $passphrase
    $box.ReadOnly = $true
    $box.Location = [Drawing.Point]::new(16, 40)
    $box.Size = [Drawing.Size]::new(480, 26)
    $box.Font = [Drawing.Font]::new('Consolas', 12)
    $dialog.Controls.Add($box)

    $hint = [Windows.Forms.Label]::new()
    $hint.Text = ('Jesli panel go nie przyjmuje, zapamietal starsze haslo z pierwszego' + [Environment]::NewLine +
        'uruchomienia. Reset kasuje jeden plik konfiguracyjny panelu i ustawia' + [Environment]::NewLine +
        'haslo powyzej. Swiat, postacie i boty sa w bazie i nie sa tym ruszane.')
    $hint.Location = [Drawing.Point]::new(16, 76)
    $hint.Size = [Drawing.Size]::new(480, 60)
    $dialog.Controls.Add($hint)

    $resetButton = [Windows.Forms.Button]::new()
    $resetButton.Text = 'Zresetuj haslo panelu'
    $resetButton.Location = [Drawing.Point]::new(16, 148)
    $resetButton.Size = [Drawing.Size]::new(230, 34)
    $resetButton.DialogResult = [Windows.Forms.DialogResult]::Yes
    $dialog.Controls.Add($resetButton)

    $closeButton = [Windows.Forms.Button]::new()
    $closeButton.Text = 'Zamknij'
    $closeButton.Location = [Drawing.Point]::new(396, 148)
    $closeButton.Size = [Drawing.Size]::new(100, 34)
    $closeButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($closeButton)
    $dialog.CancelButton = $closeButton

    $answer = $dialog.ShowDialog()
    $dialog.Dispose()
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'PanelPassword' -Yes
}

function Show-CoopSecretDialog {
    # Passwords are shown here and nowhere else: an action's output is a file
    # under launcher-logs, and support bundles carry that folder.
    param([string]$Title, [string]$Intro, [string]$Secret)
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = $Title
    $dialog.Size = [Drawing.Size]::new(560, 290)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false
    $info = [Windows.Forms.Label]::new()
    $info.Text = $Intro
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(520, 44)
    $dialog.Controls.Add($info)
    $box = [Windows.Forms.TextBox]::new()
    $box.Text = $Secret
    $box.ReadOnly = $true
    $box.Multiline = $true
    $box.ScrollBars = 'Vertical'
    $box.WordWrap = $true
    $box.Location = [Drawing.Point]::new(14, 60)
    $box.Size = [Drawing.Size]::new(520, 132)
    $box.Font = [Drawing.Font]::new('Consolas', 10)
    $dialog.Controls.Add($box)
    $copy = [Windows.Forms.Button]::new()
    $copy.Text = 'Kopiuj do schowka'
    $copy.Location = [Drawing.Point]::new(14, 202)
    $copy.Size = [Drawing.Size]::new(170, 32)
    $copy.Add_Click({ try { [Windows.Forms.Clipboard]::SetText($box.Text) } catch { } })
    $dialog.Controls.Add($copy)
    $close = [Windows.Forms.Button]::new()
    $close.Text = 'Zamknij'
    $close.Location = [Drawing.Point]::new(434, 202)
    $close.Size = [Drawing.Size]::new(100, 32)
    $close.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($close)
    $dialog.CancelButton = $close
    [void]$dialog.ShowDialog()
    $dialog.Dispose()
}

function Get-CoopClientFolder {
    $config = Get-LauncherConfig
    $folder = [string]$config.clientRoot
    if (-not $folder -and [string]$config.clientExecutable) { $folder = Split-Path -Parent ([string]$config.clientExecutable) }
    if ($folder -and (Test-Path -LiteralPath $folder -PathType Container)) { return $folder }
    return ''
}

function Show-CoopUnlockDialog {
    # Hosting is tried by the Patreon testers first. Their password is asked
    # once and remembered by the module (.m2coop.json); a friend who only
    # joins needs none, so the second way out opens just the joining tab.
    # Returns 'unlocked', 'join' or 'cancel'. Nothing typed here is logged.
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = 'COOP - testy dla patronów'
    $dialog.Size = [Drawing.Size]::new(520, 270)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false
    $dialog.Tag = 'cancel'
    $info = [Windows.Forms.Label]::new()
    $info.Text = ('Hostowanie własnego świata w COOP testują na razie patroni. Wpisz hasło z posta dla patronów - ' +
        'launcher zapamięta je na tej instalacji.')
    $info.Location = [Drawing.Point]::new(14, 12)
    $info.Size = [Drawing.Size]::new(476, 40)
    $dialog.Controls.Add($info)
    $box = [Windows.Forms.TextBox]::new()
    $box.UseSystemPasswordChar = $true
    $box.Location = [Drawing.Point]::new(14, 60)
    $box.Size = [Drawing.Size]::new(300, 26)
    $box.Font = [Drawing.Font]::new('Consolas', 11)
    $dialog.Controls.Add($box)
    $unlock = [Windows.Forms.Button]::new()
    $unlock.Text = 'Odblokuj'
    $unlock.Location = [Drawing.Point]::new(326, 58)
    $unlock.Size = [Drawing.Size]::new(164, 30)
    $dialog.Controls.Add($unlock)
    $dialog.AcceptButton = $unlock
    $wrong = [Windows.Forms.Label]::new()
    $wrong.Location = [Drawing.Point]::new(14, 94)
    $wrong.Size = [Drawing.Size]::new(476, 20)
    $wrong.ForeColor = [Drawing.Color]::DarkRed
    $dialog.Controls.Add($wrong)
    $joinInfo = [Windows.Forms.Label]::new()
    $joinInfo.Text = 'Dołączasz do świata znajomego? Hasło nie jest potrzebne - wystarczy kod zaproszenia od niego.'
    $joinInfo.Location = [Drawing.Point]::new(14, 128)
    $joinInfo.Size = [Drawing.Size]::new(476, 36)
    $joinInfo.ForeColor = [Drawing.Color]::DimGray
    $dialog.Controls.Add($joinInfo)
    $join = [Windows.Forms.Button]::new()
    $join.Text = 'Mam kod zaproszenia'
    $join.Location = [Drawing.Point]::new(14, 176)
    $join.Size = [Drawing.Size]::new(200, 32)
    $dialog.Controls.Add($join)
    $cancel = [Windows.Forms.Button]::new()
    $cancel.Text = 'Anuluj'
    $cancel.Location = [Drawing.Point]::new(390, 176)
    $cancel.Size = [Drawing.Size]::new(100, 32)
    $cancel.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancel)
    $dialog.CancelButton = $cancel
    $unlock.Add_Click({
        if (Grant-M2CoopAccess -ServerRoot $root -Password $box.Text) {
            Write-LocalLog 'COOP: hostowanie odblokowane hasłem testów.'
            $dialog.Tag = 'unlocked'
            $dialog.Close()
            return
        }
        Write-LocalLog 'COOP: podano złe hasło testów.'
        $wrong.Text = 'To nie jest hasło testów COOP.'
        $box.SelectAll()
        $box.Focus()
    })
    $join.Add_Click({
        $dialog.Tag = 'join'
        $dialog.Close()
    })
    [void]$dialog.ShowDialog()
    $result = [string]$dialog.Tag
    $dialog.Dispose()
    return $result
}

function Open-CoopWindow {
    if ((Get-Command Test-M2CoopAccess -ErrorAction SilentlyContinue) -and -not (Test-M2CoopAccess -ServerRoot $root)) {
        $choice = Show-CoopUnlockDialog
        if ($choice -eq 'join') { Show-CoopDialog -JoinOnly; return }
        if ($choice -ne 'unlocked') { return }
    }
    Show-CoopDialog
}

function Show-CoopDialog {
    # Co-op over the Internet (experimental; hosting is for the Patreon testers
    # since 2.0.80, see Open-CoopWindow). Hosting and its end restart the game
    # container and so run as actions in the main window; everything else here
    # is quick and in-process, and nothing that shows a password is written to
    # any log. -JoinOnly is the window for a friend who has an invite code and
    # no testers' password: the joining tab alone, and nothing that asks the
    # database or Docker about a world this machine does not host.
    param([switch]$JoinOnly)
    if (-not (Get-Command Get-M2CoopNetworkReport -ErrorAction SilentlyContinue)) {
        [Windows.Forms.MessageBox]::Show('Ta paczka nie ma modułu COOP.', 'COOP', 'OK', 'Information') | Out-Null
        return
    }
    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = $(if ($JoinOnly) { 'COOP - dołączam do świata znajomego' } else { 'COOP - gra ze znajomymi przez internet (eksperymentalne)' })
    $dialog.Size = [Drawing.Size]::new(660, 600)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $tabs = [Windows.Forms.TabControl]::new()
    $tabs.Location = [Drawing.Point]::new(10, 10)
    $tabs.Size = [Drawing.Size]::new(626, 500)
    $dialog.Controls.Add($tabs)
    $hostTab = [Windows.Forms.TabPage]::new()
    $hostTab.Text = 'Hostuję swój świat'
    $joinTab = [Windows.Forms.TabPage]::new()
    $joinTab.Text = 'Dołączam do znajomego'
    if (-not $JoinOnly) { $tabs.TabPages.Add($hostTab) }
    $tabs.TabPages.Add($joinTab)

    # ------------------------------------------------------------ host tab
    $status = [Windows.Forms.Label]::new()
    $status.Location = [Drawing.Point]::new(12, 10)
    $status.Size = [Drawing.Size]::new(594, 74)
    $status.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    $hostTab.Controls.Add($status)

    $friendsLabel = [Windows.Forms.Label]::new()
    $friendsLabel.Text = 'Znajomi (każdy ma własne konto w Twoim świecie):'
    $friendsLabel.Location = [Drawing.Point]::new(12, 90)
    $friendsLabel.Size = [Drawing.Size]::new(400, 20)
    $hostTab.Controls.Add($friendsLabel)

    $list = [Windows.Forms.ListView]::new()
    $list.View = 'Details'
    $list.FullRowSelect = $true
    $list.HideSelection = $false
    $list.MultiSelect = $false
    $list.Location = [Drawing.Point]::new(12, 112)
    $list.Size = [Drawing.Size]::new(430, 150)
    [void]$list.Columns.Add('Znajomy', 170)
    [void]$list.Columns.Add('Login', 130)
    [void]$list.Columns.Add('Stan', 110)
    $hostTab.Controls.Add($list)

    # How the world is offered. A host the Internet cannot reach (CGNAT, a
    # second router) is offered through a VPN both players are in; the VPNs
    # are read off this machine's adapters when the window opens.
    $viaLabel = [Windows.Forms.Label]::new()
    $viaLabel.Text = 'Połączenie:'
    $viaLabel.Location = [Drawing.Point]::new(12, 276)
    $viaLabel.Size = [Drawing.Size]::new(80, 20)
    $hostTab.Controls.Add($viaLabel)
    $viaBox = [Windows.Forms.ComboBox]::new()
    $viaBox.DropDownStyle = 'DropDownList'
    $viaBox.Location = [Drawing.Point]::new(96, 272)
    $viaBox.Size = [Drawing.Size]::new(346, 24)
    $hostTab.Controls.Add($viaBox)
    $viaValues = New-Object System.Collections.Generic.List[string]
    [void]$viaBox.Items.Add('Automatycznie (internet, przy CGNAT przez VPN)')
    $viaValues.Add('auto')
    [void]$viaBox.Items.Add('Internet (porty w routerze, UPnP)')
    $viaValues.Add('internet')
    $viaFound = @()
    if (-not $JoinOnly) { try { $viaFound = @(Get-M2CoopVpnAdapters) } catch { $viaFound = @() } }
    foreach ($vpn in $viaFound) {
        [void]$viaBox.Items.Add(('{0} - adres {1}' -f $vpn.Name, $vpn.Address))
        $viaValues.Add([string]$vpn.Kind)
    }
    $viaBox.SelectedIndex = 0
    if (-not $JoinOnly) {
        # The way the world was last hosted, while that VPN is still here.
        try {
            $lastHosting = (Read-M2CoopState -ServerRoot $root).hosting
            if ($lastHosting -and (@($lastHosting.PSObject.Properties.Name) -contains 'vpn')) {
                $lastIndex = $viaValues.IndexOf([string]$lastHosting.vpn)
                if ($lastIndex -ge 2) { $viaBox.SelectedIndex = $lastIndex }
            }
        }
        catch { }
    }

    $addButton = [Windows.Forms.Button]::new()
    $addButton.Text = 'Dodaj znajomego'
    $addButton.Location = [Drawing.Point]::new(452, 112)
    $addButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($addButton)
    $inviteButton = [Windows.Forms.Button]::new()
    $inviteButton.Text = 'Kod zaproszenia'
    $inviteButton.Location = [Drawing.Point]::new(452, 150)
    $inviteButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($inviteButton)
    $blockButton = [Windows.Forms.Button]::new()
    $blockButton.Text = 'Zablokuj / odblokuj'
    $blockButton.Location = [Drawing.Point]::new(452, 188)
    $blockButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($blockButton)
    $secureButton = [Windows.Forms.Button]::new()
    $secureButton.Text = 'Zabezpiecz konta'
    $secureButton.Location = [Drawing.Point]::new(452, 232)
    $secureButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($secureButton)
    $passwordsButton = [Windows.Forms.Button]::new()
    $passwordsButton.Text = 'Moje hasła'
    $passwordsButton.Location = [Drawing.Point]::new(452, 270)
    $passwordsButton.Size = [Drawing.Size]::new(154, 32)
    $hostTab.Controls.Add($passwordsButton)

    $hostButton = [Windows.Forms.Button]::new()
    $hostButton.Text = 'HOSTUJ ŚWIAT'
    $hostButton.Location = [Drawing.Point]::new(12, 316)
    $hostButton.Size = [Drawing.Size]::new(200, 42)
    $hostButton.BackColor = [Drawing.Color]::FromArgb(27, 150, 88)
    $hostButton.ForeColor = [Drawing.Color]::White
    $hostButton.FlatStyle = 'Flat'
    $hostButton.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $hostTab.Controls.Add($hostButton)
    $stopButtonCoop = [Windows.Forms.Button]::new()
    $stopButtonCoop.Text = 'ZAKOŃCZ HOSTOWANIE'
    $stopButtonCoop.Location = [Drawing.Point]::new(222, 316)
    $stopButtonCoop.Size = [Drawing.Size]::new(200, 42)
    $stopButtonCoop.BackColor = [Drawing.Color]::FromArgb(180, 75, 55)
    $stopButtonCoop.ForeColor = [Drawing.Color]::White
    $stopButtonCoop.FlatStyle = 'Flat'
    $stopButtonCoop.Font = [Drawing.Font]::new('Segoe UI Semibold', 10)
    $hostTab.Controls.Add($stopButtonCoop)
    $checkButton = [Windows.Forms.Button]::new()
    $checkButton.Text = 'Sprawdź sieć'
    $checkButton.Location = [Drawing.Point]::new(452, 316)
    $checkButton.Size = [Drawing.Size]::new(154, 42)
    $hostTab.Controls.Add($checkButton)

    $hostHelp = [Windows.Forms.Label]::new()
    $hostHelp.Text = ('Kolejność: Zabezpiecz konta, Dodaj znajomego, HOSTUJ ŚWIAT, a potem Kod zaproszenia - ' +
        'skopiuj go i wyślij znajomemu w prywatnej wiadomości (zawiera hasło). Hostowanie uruchamia ponownie ' +
        'serwer gry (około minuty) i prosi Windows o zgodę na regułę zapory dla portów 11000 i 13000-13002; ' +
        'przez internet otwiera je w routerze (UPnP). Gdy operator nie daje publicznego adresu (CGNAT - częste ' +
        'w internecie komórkowym), zainstalujcie Radmin VPN albo Tailscale i połączcie się w jednej sieci: ' +
        'launcher ją wykryje i hostuje przez nią, bez routera. Ty grasz dalej na serwerze 1, znajomy na serwerze Online.')
    $hostHelp.Location = [Drawing.Point]::new(12, 368)
    $hostHelp.Size = [Drawing.Size]::new(594, 96)
    $hostHelp.ForeColor = [Drawing.Color]::DimGray
    $hostTab.Controls.Add($hostHelp)

    # ------------------------------------------------------------ join tab
    $joinInfo = [Windows.Forms.Label]::new()
    $joinInfo.Text = ('Wklej kod zaproszenia, który dostałeś od znajomego (zaczyna się od M2COOP1:). ' +
        'Launcher dopisze jego świat do Twojego klienta jako drugi serwer na liście.')
    $joinInfo.Location = [Drawing.Point]::new(12, 12)
    $joinInfo.Size = [Drawing.Size]::new(594, 40)
    $joinTab.Controls.Add($joinInfo)
    $codeBox = [Windows.Forms.TextBox]::new()
    $codeBox.Multiline = $true
    $codeBox.WordWrap = $true
    $codeBox.ScrollBars = 'Vertical'
    $codeBox.Location = [Drawing.Point]::new(12, 56)
    $codeBox.Size = [Drawing.Size]::new(594, 110)
    $codeBox.Font = [Drawing.Font]::new('Consolas', 9)
    $joinTab.Controls.Add($codeBox)
    $joinButton = [Windows.Forms.Button]::new()
    $joinButton.Text = 'Zapisz w kliencie'
    $joinButton.Location = [Drawing.Point]::new(12, 176)
    $joinButton.Size = [Drawing.Size]::new(200, 36)
    $joinTab.Controls.Add($joinButton)
    $forgetButton = [Windows.Forms.Button]::new()
    $forgetButton.Text = 'Usuń świat znajomego z listy'
    $forgetButton.Location = [Drawing.Point]::new(222, 176)
    $forgetButton.Size = [Drawing.Size]::new(220, 36)
    $joinTab.Controls.Add($forgetButton)
    $joinStatus = [Windows.Forms.Label]::new()
    $joinStatus.Location = [Drawing.Point]::new(12, 224)
    $joinStatus.Size = [Drawing.Size]::new(594, 120)
    $joinStatus.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    $joinTab.Controls.Add($joinStatus)

    $closeButton = [Windows.Forms.Button]::new()
    $closeButton.Text = 'Zamknij'
    $closeButton.Location = [Drawing.Point]::new(536, 518)
    $closeButton.Size = [Drawing.Size]::new(100, 32)
    $closeButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($closeButton)
    $dialog.CancelButton = $closeButton

    $refresh = {
        if (-not $JoinOnly) {
            $state = Read-M2CoopState -ServerRoot $root
            $bindings = Get-M2CoopGameBindings -ServerRoot $root
            $lines = @()
            if (-not $bindings.Running) { $lines += 'Serwer gry: nie działa - najpierw GRAJ.' }
            elseif ($bindings.Public) { $lines += 'Hostowanie: WŁĄCZONE - porty gry są otwarte dla sieci.' }
            else { $lines += 'Hostowanie: wyłączone - porty gry słuchają tylko na tym komputerze.' }
            $public = ''; $viaName = ''
            if ($state.hosting) {
                $fields = @($state.hosting.PSObject.Properties.Name)
                if (($fields -contains 'friendAddress') -and [string]$state.hosting.friendAddress) { $public = [string]$state.hosting.friendAddress }
                elseif (($fields -contains 'publicAddress') -and [string]$state.hosting.publicAddress) { $public = [string]$state.hosting.publicAddress }
                if (($fields -contains 'mode') -and [string]$state.hosting.mode -eq 'vpn' -and ($fields -contains 'vpnName')) { $viaName = [string]$state.hosting.vpnName }
            }
            if ($public -and $viaName) { $lines += ('Adres dla znajomych (ostatnio): {0}, przez {1}' -f $public, $viaName) }
            elseif ($public) { $lines += ('Adres dla znajomych (ostatnio): {0}' -f $public) }
            $defaults = @()
            try { $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $root) }
            catch { $lines += 'Baza nie odpowiada - uruchom serwer (GRAJ).' }
            if ($defaults.Count -gt 0) { $lines += ('UWAGA: konta {0} mają hasła z paczki - kliknij Zabezpiecz konta.' -f ($defaults -join ', ')) }
            else { $lines += 'Konta admin i test: hasła zmienione.' }
            $status.Text = ($lines -join [Environment]::NewLine)
            $status.ForeColor = $(if ($defaults.Count -gt 0) { [Drawing.Color]::DarkRed } else { [Drawing.Color]::Black })
            $list.Items.Clear()
            foreach ($f in @($state.friends)) {
                $item = [Windows.Forms.ListViewItem]::new([string]$f.name)
                [void]$item.SubItems.Add([string]$f.login)
                [void]$item.SubItems.Add($(if ($f.blocked) { 'zablokowany' } else { 'aktywny' }))
                $item.Tag = [string]$f.login
                [void]$list.Items.Add($item)
            }
        }
        $client = Get-CoopClientFolder
        $cfg = $(if ($client) { Join-Path $client 'coop.cfg' } else { '' })
        if ($cfg -and (Test-Path -LiteralPath $cfg -PathType Leaf)) {
            $text = [IO.File]::ReadAllText($cfg)
            $name = ''; $hostName = ''
            if ($text -match '(?m)^name=(.*)$') { $name = $Matches[1].Trim() }
            if ($text -match '(?m)^host=(.*)$') { $hostName = $Matches[1].Trim() }
            $joinStatus.Text = ("W Twoim kliencie jest świat znajomego: {0} ({1}).`r`nW kliencie wybierz serwer 'Online: {0}'." -f $name, $hostName)
            $joinVpn = Get-M2CoopVpnProduct -Kind (Get-M2CoopVpnKindForAddress $hostName)
            if ($joinVpn) { $joinStatus.Text += ("`r`nTen adres jest w sieci {0} - gra połączy się, gdy {0} jest włączony i jesteś w sieci znajomego." -f $joinVpn.Name) }
        }
        elseif ($client) { $joinStatus.Text = 'W Twoim kliencie nie ma jeszcze świata znajomego.' }
        else { $joinStatus.Text = 'Nie wiem, gdzie jest klient - wskaż go przyciskiem WYBIERZ KLIENTA w oknie launchera.' }
    }

    $selectedFriend = {
        if ($list.SelectedItems.Count -eq 0) {
            [Windows.Forms.MessageBox]::Show('Zaznacz znajomego na liście.', 'COOP', 'OK', 'Information') | Out-Null
            return $null
        }
        $login = [string]$list.SelectedItems[0].Tag
        foreach ($f in @((Read-M2CoopState -ServerRoot $root).friends)) { if ([string]$f.login -eq $login) { return $f } }
        return $null
    }

    $addButton.Add_Click({
        $name = [Microsoft.VisualBasic.Interaction]::InputBox('Imię albo nick znajomego (z niego powstanie login):', 'Dodaj znajomego', '')
        if (-not $name) { return }
        try {
            $friend = New-M2CoopFriend -ServerRoot $root -Name $name
            Write-LocalLog ("COOP: dodano konto znajomego, login {0}." -f $friend.login)
            & $refresh
            Show-CoopSecretDialog -Title 'Nowy znajomy' `
                -Intro ("Konto gotowe. Login i hasło są też w kodzie zaproszenia - wyślij znajomemu kod (przycisk Kod zaproszenia) po włączeniu hostowania.") `
                -Secret ("Login: {0}`r`nHasło: {1}`r`nKod usuwania postaci: {2}" -f $friend.login, $friend.password, $friend.socialId)
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $inviteButton.Add_Click({
        $friend = & $selectedFriend
        if (-not $friend) { return }
        if ($friend.blocked) { [Windows.Forms.MessageBox]::Show('To konto jest zablokowane - najpierw je odblokuj.', 'COOP', 'OK', 'Information') | Out-Null; return }
        $dialog.Cursor = [Windows.Forms.Cursors]::WaitCursor
        $target = Get-M2CoopInviteTarget -ServerRoot $root
        $dialog.Cursor = [Windows.Forms.Cursors]::Default
        if (-not $target.Address) {
            $why = $(if ($target.Vpn) { ('Nie udało się odczytać Twojego adresu w {0} - uruchom go i spróbuj jeszcze raz.' -f $target.VpnName) } else { 'Nie udało się odczytać Twojego adresu w internecie.' })
            [Windows.Forms.MessageBox]::Show($why, 'COOP', 'OK', 'Warning') | Out-Null
            return
        }
        $code = Get-M2CoopFriendInvite -ServerRoot $root -Friend $friend -HostAddress $target.Address -Vpn $target.Vpn
        try { [Windows.Forms.Clipboard]::SetText($code) } catch { }
        Write-LocalLog ("COOP: skopiowano kod zaproszenia dla loginu {0}." -f $friend.login)
        $intro = 'Kod jest już w schowku. Wyślij go znajomemu w prywatnej wiadomości - zawiera jego hasło. Znajomy wkleja go w swoim launcherze (przycisk COOP) albo w pliku Dolacz.bat w folderze klienta.'
        if ($target.Vpn) { $intro = ('Kod jest już w schowku i prowadzi na Twój adres w {0}: znajomy musi najpierw dołączyć do Twojej sieci {0}. Wyślij mu kod w prywatnej wiadomości - zawiera jego hasło.' -f $target.VpnName) }
        Show-CoopSecretDialog -Title ('Kod zaproszenia - ' + [string]$friend.name) -Intro $intro -Secret $code
    })

    $blockButton.Add_Click({
        $friend = & $selectedFriend
        if (-not $friend) { return }
        try {
            Set-M2CoopFriendBlocked -ServerRoot $root -Login ([string]$friend.login) -Blocked (-not [bool]$friend.blocked)
            Write-LocalLog ("COOP: konto {0} {1}." -f $friend.login, $(if ($friend.blocked) { 'odblokowane' } else { 'zablokowane' }))
            & $refresh
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $secureButton.Add_Click({
        try {
            $changed = Protect-M2CoopAccounts -ServerRoot $root
            $names = @($changed.PSObject.Properties | ForEach-Object { $_.Name })
            & $refresh
            if ($names.Count -eq 0) {
                [Windows.Forms.MessageBox]::Show('Konta admin i test nie mają już haseł z paczki.', 'COOP', 'OK', 'Information') | Out-Null
                return
            }
            Write-LocalLog ("COOP: zmieniono hasła kont {0}." -f ($names -join ', '))
            $text = ($names | ForEach-Object { "Konto {0}: hasło {1}" -f $_, $changed.$_ }) -join "`r`n"
            Show-CoopSecretDialog -Title 'Nowe hasła' `
                -Intro 'Od teraz logujesz się na te konta tymi hasłami (w kliencie wpisz je zamiast starych). Launcher je pamięta - przycisk Moje hasła.' `
                -Secret $text
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $passwordsButton.Add_Click({
        $state = Read-M2CoopState -ServerRoot $root
        $lines = @()
        if ($state.PSObject.Properties.Name -contains 'accounts' -and $state.accounts) {
            foreach ($p in $state.accounts.PSObject.Properties) { $lines += ("Konto {0}: hasło {1}" -f $p.Name, $p.Value) }
        }
        foreach ($f in @($state.friends)) { $lines += ("Znajomy {0}: login {1}, hasło {2}" -f $f.name, $f.login, $f.password) }
        if ($lines.Count -eq 0) { $lines += 'Launcher nie zmieniał jeszcze żadnego hasła.' }
        Show-CoopSecretDialog -Title 'Hasła COOP' -Intro 'Hasła kont zmienionych i założonych przez okno COOP.' -Secret ($lines -join "`r`n")
    })

    $hostButton.Add_Click({
        try { $defaults = @(Get-M2CoopDefaultPasswordAccounts -ServerRoot $root) }
        catch { [Windows.Forms.MessageBox]::Show('Baza nie odpowiada - uruchom najpierw serwer (GRAJ).', 'COOP', 'OK', 'Warning') | Out-Null; return }
        if ($defaults.Count -gt 0) {
            [Windows.Forms.MessageBox]::Show(('Konta {0} mają hasła z paczki - każdy w internecie mógłby się na nie zalogować. Najpierw kliknij Zabezpiecz konta.' -f ($defaults -join ', ')), 'COOP', 'OK', 'Warning') | Out-Null
            return
        }
        $via = $viaValues[[Math]::Max(0, $viaBox.SelectedIndex)]
        $routerLine = "- otworzy te porty w routerze (UPnP), a gdy operator nie daje publicznego adresu (CGNAT), użyje VPN, jeśli go wykryje."
        if ($via -eq 'internet') { $routerLine = '- otworzy te porty w routerze (UPnP).' }
        elseif ($via -ne 'auto') {
            $product = Get-M2CoopVpnProduct -Kind $via
            $routerLine = ("- niczego nie otwiera w routerze: znajomi łączą się przez {0}, w Twojej sieci." -f $(if ($product) { $product.Name } else { 'VPN' }))
        }
        $answer = [Windows.Forms.MessageBox]::Show(
            ("Hostowanie:`r`n- uruchomi ponownie serwer gry (około minuty) - wyloguj się z gry,`r`n" +
             "- poprosi Windows o zgodę na regułę zapory dla portów gry,`r`n" +
             $routerLine + "`r`n`r`nKontynuować?"), 'Hostuj świat', 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'CoopHost' -Yes -ExtraArgs @('-CoopVia', $via)
    })

    $stopButtonCoop.Add_Click({
        $answer = [Windows.Forms.MessageBox]::Show(
            "Zakończenie hostowania zamknie porty w routerze i uruchomi ponownie serwer gry (około minuty). Znajomi zostaną rozłączeni.`r`n`r`nKontynuować?",
            'Zakończ hostowanie', 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        $dialog.Close()
        Start-LauncherAction -Action 'CoopStop' -Yes
    })

    $checkButton.Add_Click({
        $dialog.Close()
        Start-LauncherAction -Action 'CoopCheck'
    })

    $joinButton.Add_Click({
        try {
            $invite = Read-M2CoopInvite -Code $codeBox.Text
            $client = Get-CoopClientFolder
            if (-not $client) { throw 'Nie wiem, gdzie jest klient - wskaż go przyciskiem WYBIERZ KLIENTA w oknie launchera.' }
            $path = Write-M2CoopClientConfig -ClientFolder $client -Invite $invite
            Write-LocalLog ("COOP: zapisano swiat znajomego w {0}." -f $path)
            try { [Windows.Forms.Clipboard]::SetText([string]$invite.password) } catch { }
            $codeBox.Text = ''
            & $refresh
            # What this machine can tell before the client is started: a VPN
            # world needs that VPN here, and the world's auth either answers
            # from here or it does not (not hosting right now, or no path).
            $dialog.Cursor = [Windows.Forms.Cursors]::WaitCursor
            $advice = Get-M2CoopJoinAdvice -Invite $invite
            $answers = Test-M2CoopHostAnswers -HostAddress ([string]$invite.host) -Port ([int]$invite.auth)
            $dialog.Cursor = [Windows.Forms.Cursors]::Default
            Write-LocalLog ("COOP: serwer znajomego {0}." -f $(if ($answers) { 'odpowiada' } else { 'nie odpowiada' }))
            $intro = ("Uruchom klienta i wybierz serwer 'Online: {0}'. Hasło jest w schowku." -f $invite.name)
            if ($advice) { $intro = $advice + ' ' + $intro }
            elseif ($answers) { $intro += ' Serwer znajomego odpowiada.' }
            else { $intro += ' Serwer znajomego teraz nie odpowiada - sprawdź, czy ma włączone hostowanie.' }
            Show-CoopSecretDialog -Title 'Świat znajomego dodany' -Intro $intro `
                -Secret ("Login: {0}`r`nHasło: {1}" -f $invite.login, $invite.password)
        }
        catch { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'COOP', 'OK', 'Error') | Out-Null }
    })

    $forgetButton.Add_Click({
        $client = Get-CoopClientFolder
        if (-not $client) { return }
        $cfg = Join-Path $client 'coop.cfg'
        if (Test-Path -LiteralPath $cfg -PathType Leaf) {
            [IO.File]::Delete($cfg)
            Write-LocalLog 'COOP: usunieto swiat znajomego z klienta.'
        }
        & $refresh
    })

    try { & $refresh } catch { $status.Text = "Nie udało się odczytać stanu: $($_.Exception.Message)" }
    [void]$dialog.ShowDialog()
    $dialog.Dispose()
}

function Show-PanelChoiceDialog {
    # Two panels look at the same world and neither replaces the other, so the
    # button asks instead of deciding: the classic one is the map and the
    # controls this launcher has always opened, seban latino's is the wider
    # view - profiles, rankings, the economy's history, the host's load.
    param([Parameter(Mandatory = $true)]$Addresses)

    $dialog = [Windows.Forms.Form]::new()
    $dialog.Text = (T 'panelDialog')
    $dialog.Size = [Drawing.Size]::new(520, 250)
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false

    $info = [Windows.Forms.Label]::new()
    $info.Text = (T 'panelInfo')
    $info.Location = [Drawing.Point]::new(16, 14)
    $info.Size = [Drawing.Size]::new(480, 22)
    $dialog.Controls.Add($info)

    $classicButton = [Windows.Forms.Button]::new()
    $classicButton.Text = ("{0}  -  port {1}" -f (T 'panelClassic'), $Addresses.ClassicPort)
    $classicButton.Location = [Drawing.Point]::new(16, 46)
    $classicButton.Size = [Drawing.Size]::new(480, 56)
    $classicButton.DialogResult = [Windows.Forms.DialogResult]::Yes
    $dialog.Controls.Add($classicButton)

    $sebanButton = [Windows.Forms.Button]::new()
    $sebanButton.Text = ("{0}  -  port {1}" -f (T 'panelSeban'), $Addresses.SebanPort)
    $sebanButton.Location = [Drawing.Point]::new(16, 110)
    $sebanButton.Size = [Drawing.Size]::new(480, 56)
    $sebanButton.DialogResult = [Windows.Forms.DialogResult]::No
    $dialog.Controls.Add($sebanButton)

    # The third thing somebody pressing this button may actually want.
    # "podajcie te kody do gm bo ja nie moge na www wejsc", "ja nie mam zadnego
    # hasla nawet w panelu tieru" - it is one line in .env and nothing showed it.
    $passwordButton = [Windows.Forms.Button]::new()
    $passwordButton.Text = (T 'panelPw')
    $passwordButton.Location = [Drawing.Point]::new(16, 174)
    $passwordButton.Size = [Drawing.Size]::new(370, 30)
    $passwordButton.DialogResult = [Windows.Forms.DialogResult]::Retry
    $dialog.Controls.Add($passwordButton)

    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(396, 174)
    $cancelButton.Size = [Drawing.Size]::new(100, 30)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dialog.Controls.Add($cancelButton)
    $dialog.AcceptButton = $classicButton
    $dialog.CancelButton = $cancelButton

    $answer = $dialog.ShowDialog()
    $dialog.Dispose()
    switch ($answer) {
        ([Windows.Forms.DialogResult]::Yes) { return $Addresses.ClassicUrl }
        ([Windows.Forms.DialogResult]::No)  { return $Addresses.SebanUrl }
        ([Windows.Forms.DialogResult]::Retry) { return 'panel-password' }
        default { return $null }
    }
}

$panelButton.Add_Click({
    $addresses = Get-M2PanelAddresses -ServerRoot $root
    $url = Show-PanelChoiceDialog -Addresses $addresses
    if ($url -eq 'panel-password') {
        Show-PanelPasswordDialog
        return
    }
    if ($url) {
        Write-LocalLog "Otwieram panel: $url"
        Start-Process $url
    }
})
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
        $message = 'Kanał aktualizacji nie podał wersji serwera. Twoja instalacja pozostaje bez zmian.'
        $statusProperty = $manifest.PSObject.Properties['statusMessage']
        if ($statusProperty -and [string]$statusProperty.Value) { $message = [string]$statusProperty.Value }
        Write-LocalLog $message
        [Windows.Forms.MessageBox]::Show($message, 'Brak aktualizacji', 'OK', 'Information') | Out-Null
        return
    }
    $available = ([string]$server.version).Trim()
    $script:latestManifest = $manifest
    $script:latestServerVersion = $available
    Set-LatestVersionsFromManifest -Manifest $manifest
    $script:latestVersionChecked = $true
    Update-VersionFooter
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
    # The server only. The client half of the GM panel is experimental and
    # goes in through its own button below, never with the ordinary update.
    Start-LauncherAction -Action 'UpdateServer' -Yes
})
$gmPanelButton.Add_Click({
    $config = Get-M2LauncherConfig -ServerRoot $root -ConfigPath $configPath
    if (-not [string]$config.clientRoot) {
        [Windows.Forms.MessageBox]::Show('Najpierw wskaż folder klienta przyciskiem WYBIERZ KLIENTA.', 'Brak klienta', 'OK', 'Information') | Out-Null
        return
    }
    if ($script:clientUpdateIsPlain) {
        $answer = [Windows.Forms.MessageBox]::Show(
            "Zaktualizować klienta w $($config.clientRoot)?`r`n`r`nPodmienia pack\root.index i pack\root.data (skrypty gry). Poprzednie wersje trafiają do backups\client w folderze serwera.",
            'Aktualizacja klienta', 'YesNo', 'Question')
        if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
        Start-LauncherAction -Action 'UpdateClient' -Yes
        return
    }
    $answer = [Windows.Forms.MessageBox]::Show(
        "Panel GM na F9 (autor: OskarPWA) to funkcja MOCNO EKSPERYMENTALNA.`r`n`r`nInstalacja podmienia w kliencie dwa pliki: packoot.eix i packoot.epk (skrypty gry). Poprzednie wersje trafiają do kopii zapasowej w folderze serwera (backups\client), więc da się wrócić.`r`n`r`nPanel otwiera tylko postać z uprawnieniami GM klawiszem F9. Jeśli po instalacji gra nie wczytuje się do końca, przywróć pliki z kopii i zgłoś to na Discordzie.`r`n`r`nZainstalować teraz?",
        'Panel GM F9 - wersja testowa', 'YesNo', 'Warning')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'UpdateClient' -Yes
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
    $plan = Get-SpawnPlanFromEnv
    $kingdoms = Get-KingdomPlanFromEnv
    $chosen = Show-BotCountDialog -Current $current -Plan $plan -Kingdoms $kingdoms
    if ($null -eq $chosen) { return }
    $count = [int]$chosen.Count
    $extra = @('-BotCount', "$count", '-SpawnMinutes', "$($chosen.Minutes)", '-LateJoiners', "$($chosen.Late)", '-LateHours', "$($chosen.Hours)",
        '-PerKingdom', $(if ($chosen.PerKingdom) { '1' } else { '0' }),
        '-ShinsooBots', "$($chosen.Shinsoo)", '-ChunjoBots', "$($chosen.Chunjo)", '-JinnoBots', "$($chosen.Jinno)",
        '-Channel2', $(if ($chosen.Channel2) { '1' } else { '0' }), '-Channel2Share', "$($chosen.Channel2Share)")
    $what = if ($chosen.PerKingdom) {
        "osobno dla królestw: Shinsoo $($chosen.Shinsoo), Chunjo $($chosen.Chunjo), Jinno $($chosen.Jinno)"
    }
    else { "$count grających botów" }
    $channelWhat = if ($chosen.Channel2) { ", drugi kanał włączony ($($chosen.Channel2Share)% botów na CH2)" } else { '' }
    $answer = [Windows.Forms.MessageBox]::Show(
        "Ustawić $what (wejście w $($chosen.Minutes) min, $($chosen.Late) dodatkowych w ciągu $($chosen.Hours) h)$channelWhat i zrestartować serwer teraz, aby zastosować? Baza i postęp botów pozostaną bez zmian.",
        'Liczba botów', 'YesNoCancel', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
        Start-LauncherAction -Action 'SetBots' -Yes -ExtraArgs $extra
    }
    else {
        Start-LauncherAction -Action 'SetBots' -ExtraArgs $extra
    }
})
$difficultyButton.Add_Click({
    $current = Get-DifficultyFromEnv
    $chosen = Show-DifficultyDialog -Current $current
    if ($null -eq $chosen) { return }
    $what = switch ($chosen.Level) {
        'easy' { 'łatwy (bez czekania)' }
        'medium' { 'średni (Biolog 8 h, koń 4-7 h, księgi 7 h)' }
        'hard' { 'trudny (Biolog 24 h, koń 12-21 h, księgi 21 h)' }
        default { "własny (Biolog $($chosen.Biologist) h, Stajenny $($chosen.Horse) h, księgi: gracze $($chosen.Book) h, boty $($chosen.BotBook) h)" }
    }
    $answer = [Windows.Forms.MessageBox]::Show(
        "Ustawić poziom trudności: $what i zrestartować serwer teraz, aby zastosować? Baza i postęp botów pozostaną bez zmian.",
        'Poziom trudności', 'YesNoCancel', 'Question')
    if ($answer -eq [Windows.Forms.DialogResult]::Cancel) { return }
    $extra = @('-Difficulty', $chosen.Level, '-BiologistHours', "$($chosen.Biologist)", '-HorseHours', "$($chosen.Horse)",
        '-BookHours', "$($chosen.Book)", '-BotBookHours', "$($chosen.BotBook)")
    if ($answer -eq [Windows.Forms.DialogResult]::Yes) {
        Start-LauncherAction -Action 'SetDifficulty' -Yes -ExtraArgs $extra
    }
    else {
        Start-LauncherAction -Action 'SetDifficulty' -ExtraArgs $extra
    }
})
if ($coopButton) { $coopButton.Add_Click({ Open-CoopWindow }) }
$importDbButton.Add_Click({
    if (-not (Confirm-DockerReady)) { return }
    $target = Get-GuiTargetVolume
    $sources = @()
    try { $sources = @(Get-M2DbDataVolumes | Where-Object { $_.Name -ne $target }) } catch { $sources = @() }
    if (-not $sources -or $sources.Count -eq 0) {
        [Windows.Forms.MessageBox]::Show('Nie znaleziono innej bazy Docker do importu na tym komputerze.', 'Import bazy', 'OK', 'Information') | Out-Null
        return
    }
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = (T 'importDialog')
    $dlg.Size = [Drawing.Size]::new(470, 320)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $lbl = [Windows.Forms.Label]::new()
    $lbl.Text = (T 'importInfo')
    $lbl.Location = [Drawing.Point]::new(12, 10)
    $lbl.Size = [Drawing.Size]::new(430, 44)
    $dlg.Controls.Add($lbl)
    $list = [Windows.Forms.ListBox]::new()
    $list.Location = [Drawing.Point]::new(12, 58)
    $list.Size = [Drawing.Size]::new(430, 170)
    # The project names are opaque hashes, so the creation date is the only thing
    # that tells one install from another.
    foreach ($item in $sources) {
        $label = $item.Project
        if ($item.CreatedAt) { $label = '{0}   (utworzona {1:yyyy-MM-dd HH:mm})' -f $item.Project, $item.CreatedAt }
        [void]$list.Items.Add($label)
    }
    $list.SelectedIndex = 0
    $dlg.Controls.Add($list)
    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = (T 'importOk')
    $okButton.Location = [Drawing.Point]::new(246, 240)
    $okButton.Size = [Drawing.Size]::new(95, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dlg.Controls.Add($okButton)
    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(347, 240)
    $cancelButton.Size = [Drawing.Size]::new(95, 32)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dlg.Controls.Add($cancelButton)
    $dlg.AcceptButton = $okButton
    $dlg.CancelButton = $cancelButton
    $result = $dlg.ShowDialog()
    $picked = ''
    if ($list.SelectedIndex -ge 0) { $picked = [string]$sources[$list.SelectedIndex].Project }
    $dlg.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK -or -not $picked) { return }
    $confirm = [Windows.Forms.MessageBox]::Show(
        "Zaimportować świat z '$picked' do bieżącej instalacji?`r`n`r`nObecny świat zostanie ZASTĄPIONY (kopia trafi do folderu 'backups'), a serwer zostanie zatrzymany na czas importu. Źródło pozostanie nietknięte.",
        'Potwierdź import bazy', 'YesNo', 'Warning')
    if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'ImportDb' -Yes -ExtraArgs @('-ImportSource', "$picked")
})
$worldBackupButton.Add_Click({
    # One button rather than three, because the main window has no room for
    # three and the report was that the backup could not be FOUND, not that it
    # was too many clicks away. The dialog says what each of them does before
    # anything is stopped or deleted.
    if (-not (Confirm-DockerReady)) { return }
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = (T 'backupDialog')
    $dlg.Size = [Drawing.Size]::new(470, 300)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $lbl = [Windows.Forms.Label]::new()
    $lbl.Text = (T 'backupInfo')
    $lbl.Location = [Drawing.Point]::new(12, 10)
    $lbl.Size = [Drawing.Size]::new(430, 60)
    $dlg.Controls.Add($lbl)
    $choice = ''
    $makeButton = [Windows.Forms.Button]::new()
    $makeButton.Text = (T 'backupMake')
    $makeButton.Location = [Drawing.Point]::new(12, 80)
    $makeButton.Size = [Drawing.Size]::new(430, 40)
    $makeButton.Add_Click({ $script:guiBackupChoice = 'make'; $dlg.DialogResult = [Windows.Forms.DialogResult]::OK })
    $dlg.Controls.Add($makeButton)
    $loadButton = [Windows.Forms.Button]::new()
    $loadButton.Text = (T 'backupLoad')
    $loadButton.Location = [Drawing.Point]::new(12, 126)
    $loadButton.Size = [Drawing.Size]::new(430, 40)
    $loadButton.Add_Click({ $script:guiBackupChoice = 'load'; $dlg.DialogResult = [Windows.Forms.DialogResult]::OK })
    $dlg.Controls.Add($loadButton)
    $resetButton = [Windows.Forms.Button]::new()
    $resetButton.Text = (T 'backupReset')
    $resetButton.Location = [Drawing.Point]::new(12, 172)
    $resetButton.Size = [Drawing.Size]::new(430, 40)
    $resetButton.BackColor = [Drawing.Color]::FromArgb(180, 75, 55)
    $resetButton.ForeColor = [Drawing.Color]::White
    $resetButton.Add_Click({ $script:guiBackupChoice = 'reset'; $dlg.DialogResult = [Windows.Forms.DialogResult]::OK })
    $dlg.Controls.Add($resetButton)
    $cancelButton = [Windows.Forms.Button]::new()
    $cancelButton.Text = (T 'cancel')
    $cancelButton.Location = [Drawing.Point]::new(347, 222)
    $cancelButton.Size = [Drawing.Size]::new(95, 30)
    $cancelButton.DialogResult = [Windows.Forms.DialogResult]::Cancel
    $dlg.Controls.Add($cancelButton)
    $dlg.CancelButton = $cancelButton
    $script:guiBackupChoice = ''
    $result = $dlg.ShowDialog()
    $choice = $script:guiBackupChoice
    $dlg.Dispose()
    if ($result -ne [Windows.Forms.DialogResult]::OK -or -not $choice) { return }

    if ($choice -eq 'make') {
        Start-LauncherAction -Action 'BackupDb' -Yes
        return
    }
    if ($choice -eq 'load') {
        $backupRoot = Join-Path $root 'backups'
        if (-not (Test-Path -LiteralPath $backupRoot -PathType Container) -or
            -not (Get-ChildItem -LiteralPath $backupRoot -Filter 'db-backup-*.zip' -File -ErrorAction SilentlyContinue)) {
            [Windows.Forms.MessageBox]::Show((T 'backupNone'), (T 'backupDialog'), 'OK', 'Information') | Out-Null
            return
        }
        $picker = [Windows.Forms.OpenFileDialog]::new()
        $picker.Title = (T 'backupPick')
        $picker.InitialDirectory = $backupRoot
        $picker.Filter = 'Kopia swiata (db-backup-*.zip)|db-backup-*.zip|ZIP|*.zip'
        if ($picker.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { $picker.Dispose(); return }
        $file = $picker.FileName
        $picker.Dispose()
        $confirm = [Windows.Forms.MessageBox]::Show(
            "Przywrócić świat z '$([IO.Path]::GetFileName($file))'?`r`n`r`nObecny świat zostanie ZASTĄPIONY. Zanim to nastąpi, launcher zapisze go do własnej kopii w folderze 'backups', więc da się cofnąć.",
            'Potwierdź przywrócenie kopii', 'YesNo', 'Warning')
        if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
        Start-LauncherAction -Action 'RestoreDb' -Yes -ExtraArgs @('-RestoreSource', "$file")
        return
    }
    # reset: the world is wiped and the server comes straight back up on the
    # fresh one (-ThenStart), so "wyzeruj i zacznij od nowa" is one decision.
    $confirm = [Windows.Forms.MessageBox]::Show(
        "Wyzerować świat i zacząć od nowa?`r`n`r`nZniknie CAŁY obecny świat: postacie, poziomy, ekwipunek, boty i konta gry. Launcher najpierw zapisze go do kopii zip w folderze 'backups', więc da się do niego wrócić przyciskiem KOPIA SWIATA -> Przywroc swiat z kopii.`r`n`r`nPo wyzerowaniu serwer uruchomi się sam na nowym świecie. Ten start potrwa dłużej - baza powstaje od nowa i boty są zasiewane.",
        'Potwierdź wyzerowanie świata', 'YesNo', 'Warning')
    if ($confirm -ne [Windows.Forms.DialogResult]::Yes) { return }
    $again = [Windows.Forms.MessageBox]::Show(
        "Na pewno? To ostatnie pytanie.`r`n`r`nPo kliknięciu TAK obecny świat przestaje być światem tego serwera.",
        'Wyzerowanie świata', 'YesNo', 'Warning')
    if ($again -ne [Windows.Forms.DialogResult]::Yes) { return }
    # The new world's rates and whether its bots wait, asked before the old
    # one goes: this is the last moment they can be set with nothing yet
    # happening in the world.
    $fresh = Show-FreshWorldDialog
    if (-not $fresh) { return }
    Start-LauncherAction -Action 'ResetWorld' -Yes -ExtraArgs @(
        '-ThenStart',
        '-RateExp', "$($fresh.Exp)",
        '-RateDrop', "$($fresh.Drop)",
        '-RateYang', "$($fresh.Yang)",
        '-HoldBots', "$($fresh.Hold)",
        '-StarterChest', "$($fresh.Starter)")
})
$dbAccessButton.Add_Click({
    # In-process on purpose: an action would print through the log box and the
    # launcher log, and the launcher log travels in support bundles. Read-only
    # text boxes so the values can be selected and copied.
    $envPath = Join-Path $root 'linux-port\docker\.env'
    if (-not (Test-Path -LiteralPath $envPath -PathType Leaf)) {
        [Windows.Forms.MessageBox]::Show((T 'dbAccessNoEnv'), (T 'dbAccessTitle'), 'OK', 'Warning') | Out-Null
        return
    }
    $envText = [IO.File]::ReadAllText($envPath)
    $read = {
        param($name, $default)
        $m = [Regex]::Match($envText, "(?m)^$name=(.*?)\s*$")
        if ($m.Success -and $m.Groups[1].Value) { $m.Groups[1].Value } else { $default }
    }
    $rows = @(
        @('Host', '127.0.0.1'),
        @('Port', (& $read 'M2_DB_PUBLISH_PORT' '3306')),
        @('root', (& $read 'M2_DB_ROOT_PASSWORD' '')),
        @((& $read 'M2_DB_USER' 'metin2'), (& $read 'M2_DB_PASSWORD' ''))
    )
    $dlg = [Windows.Forms.Form]::new()
    $dlg.Text = (T 'dbAccessTitle')
    $dlg.Size = [Drawing.Size]::new(560, 372)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $y = 18
    foreach ($row in $rows) {
        $label = [Windows.Forms.Label]::new()
        $label.Text = $row[0]
        $label.Location = [Drawing.Point]::new(18, $y + 4)
        $label.Size = [Drawing.Size]::new(110, 22)
        $dlg.Controls.Add($label)
        $box = [Windows.Forms.TextBox]::new()
        $box.Text = $row[1]
        $box.ReadOnly = $true
        $box.Location = [Drawing.Point]::new(132, $y)
        $box.Size = [Drawing.Size]::new(396, 24)
        $box.Font = [Drawing.Font]::new('Consolas', 9)
        $dlg.Controls.Add($box)
        $y += 34
    }
    $hint = [Windows.Forms.Label]::new()
    $hint.Text = (T 'dbAccessHint')
    if ($script:clientUpdateIsPlain) { $hint.Text = (T 'dbAccessHint') + [Environment]::NewLine + [Environment]::NewLine + (T 'dbAccessProtoNote') }
    $hint.Location = [Drawing.Point]::new(18, $y + 8)
    $hint.Size = [Drawing.Size]::new(510, 160)
    $dlg.Controls.Add($hint)
    $dlg.Size = [Drawing.Size]::new(560, 412)
    $openButton = [Windows.Forms.Button]::new()
    $openButton.Text = (T 'dbAccessOpenEnv')
    $openButton.Location = [Drawing.Point]::new(18, 330)
    $openButton.Size = [Drawing.Size]::new(170, 32)
    $openButton.Add_Click({ Start-Process notepad.exe -ArgumentList ('"' + $envPath + '"') }.GetNewClosure())
    $dlg.Controls.Add($openButton)
    $okButton = [Windows.Forms.Button]::new()
    $okButton.Text = 'OK'
    $okButton.Location = [Drawing.Point]::new(433, 330)
    $okButton.Size = [Drawing.Size]::new(95, 32)
    $okButton.DialogResult = [Windows.Forms.DialogResult]::OK
    $dlg.Controls.Add($okButton)
    $dlg.AcceptButton = $okButton
    $dlg.ShowDialog() | Out-Null
    $dlg.Dispose()
})
$repairDbButton.Add_Click({
    if (-not (Confirm-DockerReady)) { return }
    $answer = [Windows.Forms.MessageBox]::Show(
        "Naprawić dostęp do bazy?`r`n`r`nUżyj tego, gdy po imporcie serwer nie startuje (playerbot-migrate kończy się błędem) albo gdy Navicat/HeidiSQL odrzuca hasło z pliku .env. Odtwarza tylko techniczne konta bazy (gry i root) — postacie, przedmioty i boty pozostają BEZ ZMIAN. Serwer zostanie zatrzymany na czas naprawy.",
        'Napraw dostęp do bazy', 'YesNo', 'Question')
    if ($answer -ne [Windows.Forms.DialogResult]::Yes) { return }
    Start-LauncherAction -Action 'RepairDb'
})

# The window's layout - the menu of five pages, the cards, the painted
# background (22 September) - is its own file and only moves the controls
# built above into place: their Click handlers and the one-action-at-a-time
# guard stay as they are. Without the file (an installation part-way through
# an update) the window keeps its plain layout. A layout that failed once on
# this computer leaves .m2launcher-classic-layout behind, and the plain
# layout is what opens from then on; deleting the file tries again.
$layoutPath = Join-Path $PSScriptRoot 'Metin2-Launcher-GUI.Layout.ps1'
$classicLayoutFlag = Join-Path $root '.m2launcher-classic-layout'
if ((Test-Path -LiteralPath $layoutPath -PathType Leaf) -and
        ($UiSelfTest -or -not (Test-Path -LiteralPath $classicLayoutFlag -PathType Leaf))) {
    try {
        . $layoutPath
    }
    catch {
        if ($UiSelfTest) { throw }
        Write-StartupFailure ('Nowy wyglad launchera: ' + ($_ | Out-String))
        if ($script:ui -and $script:ui.Cleared) {
            # The controls are half moved and there is no way back inside
            # this process; the next start opens the plain window.
            try { [IO.File]::WriteAllText($classicLayoutFlag, ($_ | Out-String)) } catch { }
            throw ('Nowy wyglad launchera nie uruchomil sie na tym komputerze ({0}). Uruchom launcher jeszcze raz - otworzy sie w poprzednim wygladzie.' -f $_.Exception.Message)
        }
    }
    if ($UiSelfTest) {
        Invoke-LayoutSelfTest -OutputDirectory $UiTestOutput
        $script:form.Dispose()
        exit 0
    }
}
elseif ($UiSelfTest) { throw 'Metin2-Launcher-GUI.Layout.ps1 is missing.' }

$timer = [Windows.Forms.Timer]::new()
$timer.Interval = 1200
$timer.Add_Tick({ Update-ActionStream; Complete-LauncherAction })
$timer.Start()

$statusTimer = [Windows.Forms.Timer]::new()
$statusTimer.Interval = 8000
$statusTimer.Add_Tick({
    if ($script:activeProcess) { return }
    Refresh-Status
    # One manifest read per session: on the first quiet tick rather than during
    # form startup, so the window is already usable while it happens.
    Read-LatestServerVersion
})
$statusTimer.Start()

# A new version people can actually notice. The footer has always changed
# colour when the server was behind, and that is easy to miss on a window
# sitting in the background - so while an update is waiting the line blinks
# red and says so in words (Tieru: "zrob migotanie na czerwono ze jest wydana
# nowa wersja klienta lub serwera, aby ludzie to widzieli").
#
# The timer owns nothing but the colour: Update-VersionFooter decides whether
# there is an update at all and what the resting colour is, so a check that
# comes back "already newest" stops the blinking on its own.
$script:versionBlinkOn = $false
$blinkTimer = [Windows.Forms.Timer]::new()
$blinkTimer.Interval = 700
$blinkTimer.Add_Tick({
    if (-not $script:versionLabel) { return }
    if (-not $script:updateAvailable) {
        if ($script:versionBlinkOn) {
            $script:versionBlinkOn = $false
            if ($script:versionBaseColor) { $script:versionLabel.ForeColor = $script:versionBaseColor }
        }
        return
    }
    $script:versionBlinkOn = -not $script:versionBlinkOn
    $script:versionLabel.ForeColor = if ($script:versionBlinkOn) { [Drawing.Color]::Red }
        elseif ($script:versionBaseColor) { $script:versionBaseColor }
        else { [Drawing.Color]::Gold }
})
$blinkTimer.Start()

# COOP: the router's mappings are leased for four hours. While hosting is on
# and this window is open they are renewed every hour by a quiet process of
# their own, outside the action runner, so no button is ever refused for it.
if ($coopButton) {
    $script:coopRenewTimer = [Windows.Forms.Timer]::new()
    $script:coopRenewTimer.Interval = 3600000
    $script:coopRenewTimer.Add_Tick({
        try {
            $coopState = Read-M2CoopState -ServerRoot $root
            if ($coopState.hosting -and $coopState.hosting.active) {
                Start-Process -FilePath 'powershell.exe' -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass',
                    '-File', ('"{0}"' -f $cliLauncher), '-Action', 'CoopRenew') | Out-Null
            }
        }
        catch { }
    })
    $script:coopRenewTimer.Start()
}

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
Update-VersionFooter
Refresh-Status
[void]$script:form.ShowDialog()
