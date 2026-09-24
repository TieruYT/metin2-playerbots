Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

function Invoke-M2DiagnosticProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FileName,
        [AllowEmptyString()][string]$Arguments = '',
        [ValidateRange(100, 30000)][int]$TimeoutMilliseconds = 3500
    )

    $process = $null
    try {
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $FileName
        $startInfo.Arguments = $Arguments
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($startInfo)
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            try { $process.Kill() } catch {}
            return [pscustomobject]@{
                ExitCode = -1
                TimedOut = $true
                Output = 'Polecenie nie odpowiedziało w wyznaczonym czasie.'
            }
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            TimedOut = $false
            Output = ($stdout + $stderr).Trim()
        }
    }
    catch {
        return [pscustomobject]@{
            ExitCode = -1
            TimedOut = $false
            Output = $_.Exception.Message
        }
    }
    finally {
        if ($process) { $process.Dispose() }
    }
}

function Get-M2OneDriveRootFor {
    # The OneDrive folder a path lies in, or nothing. OneDrive moves the
    # Desktop and the Documents into itself (Known Folder Move), so a server
    # unpacked on the Desktop can end up there one day without anybody touching
    # it: Avalach's did between 14:57 and 18:41 on 24 September, and every
    # build after that died on a boost header the build context no longer
    # carried ("forward1_256.hpp: No such file or directory"), five updates in
    # a row, while the diagnostics said the server could start. Docker reads a
    # OneDrive tree through its cloud placeholders, and files go missing from
    # what it sends to the build.
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Path)

    if (-not $Path) { return '' }
    $full = ''
    try { $full = [IO.Path]::GetFullPath($Path).TrimEnd('\') + '\' } catch { return '' }
    $roots = [Collections.Generic.List[string]]::new()
    foreach ($name in @('OneDrive', 'OneDriveConsumer', 'OneDriveCommercial')) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ($value) { $roots.Add([string]$value) }
    }
    try {
        foreach ($account in @(Get-ChildItem -LiteralPath 'HKCU:\Software\Microsoft\OneDrive\Accounts' -ErrorAction Stop)) {
            $props = Get-ItemProperty -LiteralPath $account.PSPath -ErrorAction SilentlyContinue
            if ($props -and ($props.PSObject.Properties.Name -contains 'UserFolder') -and $props.UserFolder) {
                $roots.Add([string]$props.UserFolder)
            }
        }
    }
    catch {}
    foreach ($root in $roots) {
        $prefix = ''
        try { $prefix = [IO.Path]::GetFullPath($root).TrimEnd('\') + '\' } catch { continue }
        if ($prefix.Length -gt 3 -and $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            return $prefix.TrimEnd('\')
        }
    }
    return ''
}

function Get-M2OneDriveRemedy {
    param([AllowEmptyString()][string]$OneDriveRoot = '')

    # Single quotes and -f: PowerShell takes the typographic quotes of the
    # Polish text for string delimiters.
    $where = if ($OneDriveRoot) { ' (' + $OneDriveRoot + ')' } else { '' }
    return ('Folder serwera leży w OneDrive{0}. Docker nie widzi części plików z folderów OneDrive, więc budowa serwera zatrzymuje się na pliku, którego „nie ma”, choć leży na dysku. Zamknij launcher i przenieś cały folder gry - ten, w którym są foldery Serwer i Klient - poza OneDrive, np. do C:\Metin2 Singleplayer. Potem uruchom Metin2-Launcher-GUI.bat z nowego miejsca (stary skrót na pulpicie wskazuje starą ścieżkę) i kliknij GRAJ. Świat, postacie i boty są w Dockerze i nic z nich nie zginie. Jeśli po przeniesieniu budowa dalej zgłasza brakujący plik, rozpakuj na folder Serwer pełną paczkę serwera, zostawiając swój plik .env.' -f $where)
}

function Get-M2LauncherErrorGuidance {
    param(
        [AllowEmptyString()][string]$Text,
        [AllowEmptyString()][string]$ServerRoot = ''
    )

    $value = [string]$Text
    $port = '7788'
    if ($value -match '(?i)(?:Bind for |listen (?:tcp )?)(?:\[?[^\]\s:]+\]?:)?(?<port>\d{2,5})') {
        $port = $Matches.port
    }
    elseif ($value -match '(?i)(?<port>\d{2,5}).{0,80}(?:port is already allocated|address already in use)') {
        $port = $Matches.port
    }

    # A file the build could not find, in a server folder OneDrive holds
    # (Get-M2OneDriveRootFor): the file is on the disk and not in what Docker
    # was sent, so no reinstall of the same folder helps. Asked first, because
    # the compiler's missing header reads like a broken package.
    if ($ServerRoot -and $value -match '(?i)fatal error: [^\r\n]+: No such file or directory|failed to compute cache key|not found in build context') {
        $oneDrive = Get-M2OneDriveRootFor -Path $ServerRoot
        if ($oneDrive) {
            return [pscustomobject]@{
                Code = 'ONEDRIVE_BUILD_CONTEXT'
                Title = 'Folder serwera jest w OneDrive'
                Message = 'Budowa serwera zatrzymała się na pliku, którego Docker nie dostał, choć jest w folderze serwera. Folder leży w OneDrive (zwykle dlatego, że OneDrive przeniósł do siebie Pulpit), a Docker nie widzi części plików z takich folderów. Baza i postęp są w porządku.'
                Remedy = (Get-M2OneDriveRemedy -OneDriveRoot $oneDrive)
            }
        }
    }

    # An engine file a mod rewrote to call into the bot manager for something
    # it never had: an update replaces our files and leaves that one, and the
    # build dies on it at every click - archonek on 18 September, every update
    # from 2.0.74 on, with messenger_manager.cpp and GetCompanionOwner from a
    # fork's companion system, while this function answered with nothing that
    # named the file. Asked first: the compiler's line is the whole answer.
    if ($value -match '(?i)([A-Za-z0-9_]+\.(?:cpp|h)):\d+:\d+: error: [^\r\n]*CPlayerBotManager[^\r\n]*has no member named') {
        $modFile = $Matches[1]
        return [pscustomobject]@{
            Code = 'ENGINE_FILE_FROM_MOD'
            Title = "Plik silnika $modFile pochodzi z innej przeróbki"
            Message = "Budowa rdzenia gry zatrzymała się na pliku ${modFile}: woła funkcję botów, której w tej wersji nie ma. Ten plik pochodzi z cudzej przeróbki (modu) i aktualizacja go nie podmieniła. Baza i postęp są w porządku."
            Remedy = "Przywróć fabryczny plik ${modFile}: skopiuj go z pełnej paczki serwera do folderu serwera, do podfolderu linux-port\docker\game\src\server\game\src, i kliknij ZAINSTALUJ AKTUALIZACJE albo GRAJ. Jeśli nie masz pełnej paczki, wyślij logi na Discorda (ZBIERZ / WYŚLIJ LOGI)."
        }
    }

    # Not a busy port: Windows itself refused the bind. Hyper-V and WSL reserve
    # random port ranges after a restart ("excluded port ranges"), and when
    # 11000 or 13000 falls inside one, compose fails one second after the
    # images are built with a message about access permissions. Five updates
    # in a row went that way for one player before this branch existed.
    if ($value -match '(?i)ports are not available|forbidden by its access permissions|zabroniony przez uprawnienia|WSAEACCES|\b10013\b') {
        return [pscustomobject]@{
            Code = 'PORT_EXCLUDED'
            Title = "Windows zarezerwował port $port"
            Message = "Port $port nie jest zajęty przez program - jest w zakresie, który Windows (Hyper-V/WSL) zarezerwował dla siebie po ostatnim restarcie. Docker nie może na nim nasłuchiwać, więc serwer nie wstaje. Pliki serwera i baza są w porządku."
            Remedy = 'Uruchom PowerShell jako administrator i wykonaj: net stop winnat, potem kliknij GRAJ w launcherze, a gdy serwer wstanie, wykonaj: net start winnat. Zwykle pomaga też zwykły restart Windows. Sprawdzenie zakresów: netsh interface ipv4 show excludedportrange protocol=tcp'
        }
    }

    if ($value -match '(?i)port is already allocated|address already in use|failed programming external connectivity|bind for .+ failed|port \d{2,5} (?:jest zajęty|zajmuje)') {
        return [pscustomobject]@{
            Code = 'PORT_IN_USE'
            Title = "Port $port jest już zajęty"
            Message = "Inny program albo druga instalacja serwera używa portu $port. Launcher nie uruchomi drugiego serwera na tym samym porcie."
            Remedy = 'Kliknij GRAJ albo ZAINSTALUJ AKTUALIZACJE jeszcze raz - launcher sam znajdzie kontener innej instalacji trzymający ten port i zatrzyma go, nie ruszając bazy, wolumenów ani postępu (w wersji konsolowej robi to opcja 21, Zwolnij porty). Samo wyłączenie Docker Desktop nie pomaga: kontenery mają politykę restart=unless-stopped, więc wracają przy każdym starcie silnika i znów zajmują port. Nie usuwaj wolumenów Dockera.'
        }
    }

    if ($value -match '(?i)virtuali[sz]ation support (?:(?:wasn.t |was )?not )?detected|hardware.assisted virtuali[sz]ation|virtuali[sz]ation.*disabled') {
        return [pscustomobject]@{
            Code = 'VIRTUALIZATION_DISABLED'
            Title = 'Wirtualizacja jest wyłączona'
            Message = 'Docker Desktop nie wystartuje, dopóki wirtualizacja procesora nie będzie dostępna dla Windows.'
            Remedy = 'W BIOS/UEFI włącz AMD SVM/AMD-V albo Intel VT-x. Następnie włącz funkcje „Virtual Machine Platform” i „Windows Subsystem for Linux”, uruchom jako administrator: wsl --install, po czym zrestartuj komputer.'
        }
    }

    if ($value -match '(?i)there was a problem with wsl|wsl.+(?:error|failed|exit status)|Wsl/Service/|WSL 2 installation is incomplete|windows subsystem for linux.+(?:missing|disabled)') {
        return [pscustomobject]@{
            Code = 'WSL_BROKEN'
            Title = 'WSL 2 wymaga naprawy'
            Message = 'Docker Desktop nie może uruchomić swojego środowiska WSL 2.'
            Remedy = 'Otwórz PowerShell jako administrator i wykonaj kolejno: wsl --status, wsl --update oraz wsl --install. Zrestartuj Windows. Jeżeli błąd pozostanie, sprawdź czy w BIOS/UEFI jest włączone AMD SVM/Intel VT-x.'
        }
    }

    if ($value -match '(?i)docker engine did not become ready|cannot connect to the docker daemon|open //./pipe/docker|docker desktop is unable to start|docker api is unavailable') {
        return [pscustomobject]@{
            Code = 'DOCKER_NOT_READY'
            Title = 'Docker Engine nie jest jeszcze gotowy'
            Message = 'Okno Docker Desktop może być otwarte, ale jego silnik nadal startuje albo zatrzymał się na błędzie.'
            Remedy = 'Odczekaj chwilę i spróbuj ponownie. Jeśli status nie zmieni się na „GOTOWY”, otwórz Docker Desktop → Troubleshoot → Restart. Potem użyj w launcherze „Diagnostyka” i „Zbierz logi (ZIP)”.'
        }
    }

    if ($value -match '(?i)cannot overwrite non-directory.+artifacts\.json.+with directory') {
        return [pscustomobject]@{
            Code = 'LEGACY_INSTALLER_DESTINATION'
            Title = 'Wybrany folder zawiera inną instalację'
            Message = 'Stary install.ps1 próbuje skopiować paczkę na istniejący plik lub do niezgodnego układu katalogów.'
            Remedy = 'Nie uruchamiaj starego install.ps1 na folderze obecnej paczki All-in-One. Rozpakuj pełną paczkę do pustego folderu i uruchom Metin2-Launcher-GUI.bat. Istniejącej bazy Dockera nie usuwaj.'
        }
    }

    # Docker Desktop's own Linux disk went read-only or ran out of room, so the
    # image could not be written. Nothing of the server's files is touched;
    # the fix is free space and a clean restart of the WSL machine - and
    # Docker's "Clean / Purge data" only where there is no world yet, because
    # the database lives on that same disk. ext4 answers its first I/O error by
    # remounting itself read-only, so the first build says "input/output
    # error" about buildkit's own files, the next says "read-only file
    # system", and every one after that only "failed to solve: exit code:
    # 255" - which no pattern here can read, and which is why the preflight
    # now writes to that disk before anything is built (pattsito, 23
    # September: five updates in forty minutes, each ending there).
    if ($value -match '(?i)read-only file system|no space left on device|(?:/var/lib/(?:docker|desktop-containerd)|buildkit)[^\r\n]*input/output error|desktop-containerd.+meta\.db') {
        return [pscustomobject]@{
            Code = 'DOCKER_DISK_BROKEN'
            Title = 'Dysk maszyny Dockera jest tylko do odczytu albo pełny'
            Message = 'Docker nie mógł zapisać na swoim dysku (plik docker_data.vhdx) - komunikat „read-only file system”, „input/output error” albo „no space left on device”. Zwykle zabrakło miejsca na dysku Windows, na którym leży ten plik, albo Docker Desktop zamknął się nieczysto. Pliki serwera są w porządku; baza świata leży na tym samym dysku Dockera.'
            Remedy = (Get-M2DockerDiskRemedy)
        }
    }
    if ($value -match "(?i)playerbot-migrate.+didn.t complete successfully|database import was not ready after|user: 'unauthenticated'") {
        return [pscustomobject]@{
            Code = 'DB_USER_BROKEN'
            Title = 'Serwer nie może zalogować się do własnej bazy'
            Message = 'Baza działa, ale techniczne konto, którym łączy się serwer, nie jest rozpoznawane. Dlatego krok „playerbot-migrate” nie kończy się poprawnie, a gra i panel nie wstają. Twoje postacie, przedmioty i boty są bezpieczne.'
            Remedy = 'W launcherze kliknij „NAPRAW DOSTĘP DO BAZY”, poczekaj na komunikat „Gotowe”, a potem kliknij „GRAJ”. Nie usuwaj wolumenów i nie używaj docker compose down -v.'
        }
    }

    # apt inside a build refuses a Release file dated after the machine's
    # clock, and Docker Desktop's machine takes the Windows clock: Xewi's
    # Windows ran three hours behind (19 September), so every start stopped at
    # the panel's apt-get while the containers built earlier - "z dockera
    # dziala" - still ran.
    if ($value -match '(?i)is not valid yet \(invalid for another') {
        return [pscustomobject]@{
            Code = 'CLOCK_BEHIND'
            Title = 'Zegar komputera jest przestawiony'
            Message = 'Budowa serwera zatrzymała się, bo zegar Windows - a za nim Docker - jest opóźniony względem prawdziwego czasu i serwer pakietów odrzucił pobieranie (komunikat „Release file ... is not valid yet”). Pliki serwera i baza są w porządku.'
            Remedy = 'W Windows otwórz Ustawienia → Czas i język → Data i godzina, włącz „Ustaw czas automatycznie”, sprawdź strefę czasową (dla Polski: Warszawa) i kliknij „Synchronizuj teraz”. Potem zamknij Docker Desktop (ikona w zasobniku → Quit) i kliknij GRAJ.'
        }
    }

    # An image the build makes itself, looked for on Docker Hub by an older
    # Compose: seban-collector and seban-item-grants run the image the
    # seban-panel service builds, and the compose file says pull_policy: never
    # for them only from 2.0.77 on.
    if ($value -match '(?i)pull access denied for metin2/') {
        return [pscustomobject]@{
            Code = 'LOCAL_IMAGE_PULLED'
            Title = 'Docker szukał w internecie obrazu, który serwer buduje sam'
            Message = 'Starszy Docker Compose próbował pobrać z Docker Hub obraz panelu zaawansowanego (metin2/seban-panel), zanim go zbudował, i przerwał start. Pliki serwera i baza są w porządku.'
            Remedy = 'Kliknij GRAJ jeszcze raz. Jeśli błąd wróci, otwórz PowerShell w folderze serwera, w podfolderze linux-port\docker, wykonaj: docker compose build seban-panel, a potem kliknij GRAJ. Logowanie do Docker Hub (docker login) niczego tu nie zmienia.'
        }
    }

    # Only a 404 said of the manifest itself. This reads the whole output of
    # the failed action, and a log carries "404" somewhere - the panel's answer
    # to a missing icon, a pid, a coordinate - so the bare number sent
    # archonek to wait for an update channel that was there all along
    # (18 September: the manifest answered 200, the start had failed).
    if ($value -match '(?i)update-manifest[^\r\n]*\b404\b|\b404\b[^\r\n]*update-manifest|not found[^\r\n]+update-manifest|update-manifest[^\r\n]+not found') {
        return [pscustomobject]@{
            Code = 'UPDATE_CHANNEL_UNPUBLISHED'
            Title = 'Kanał aktualizacji nie został jeszcze opublikowany'
            Message = 'Serwer GitHub nie ma obecnie manifestu aktualizacji. Nie oznacza to uszkodzenia zainstalowanego serwera.'
            Remedy = 'Możesz nadal grać na obecnej wersji. Spróbuj ponownie później; launcher nie powinien niczego instalować ani tworzyć drugiego serwera.'
        }
    }

    return [pscustomobject]@{
        Code = 'UNKNOWN'
        Title = 'Operacja nie powiodła się'
        Message = if ($value) { ($value -split '\r?\n' | Select-Object -Last 1) } else { 'Nie otrzymano szczegółów błędu.' }
        Remedy = 'Uruchom „Diagnostyka”, następnie „Zbierz logi (ZIP)” i prześlij utworzony plik na kanał pomocy projektu.'
    }
}

# What to do about a Docker disk that refuses writes: the guidance dialog, the
# preflight's blocking issue and the updater's refusal all say the same thing.
# One line per step, and never "wsl" followed on its line by "error",
# "failed" or "exit status": Get-M2LauncherErrorGuidance reads whole outputs,
# and a sentence of this remedy printed by the preflight must not look like a
# broken WSL to the rule above it.
function Get-M2DockerDiskRemedy {
    return ('1. Zwolnij miejsce na dysku z folderem Dockera (%LOCALAPPDATA%\Docker, zwykle C:) - budowa serwera potrzebuje ok. 15 GB.' + [Environment]::NewLine +
            '2. Zamknij Docker Desktop (ikona w zasobniku → Quit) i w PowerShell wpisz: wsl --shutdown' + [Environment]::NewLine +
            '3. Uruchom Docker Desktop, poczekaj na „Engine running” i kliknij GRAJ - launcher dokończy budowanie bez ponownego pobierania.' + [Environment]::NewLine +
            'Jeśli to nie pomoże, a masz już świat z postaciami, nie używaj w Docker Desktop „Clean / Purge data” ani „Reset to factory defaults” (kasują bazę) - wyślij logi na Discorda. ' +
            'Na świeżej instalacji, która jeszcze ani razu nie wystartowała, Docker Desktop → Troubleshoot → Clean / Purge data niczego nie zabierze i zakłada Dockerowi nowy dysk.')
}

function Format-M2Bytes {
    param([long]$Bytes)
    if ($Bytes -ge 1GB) { return ('{0:N1} GB' -f ($Bytes / 1GB)) }
    return ('{0:N0} MB' -f ($Bytes / 1MB))
}

# Where Docker Desktop keeps its Linux disk, and how much room the Windows
# drive under it has left. Images, the build cache and every volume - the
# world's database with them - live in one ext4 file system inside
# docker_data.vhdx, which grows as a build writes; when Windows cannot give it
# the room, ext4 takes the write error and remounts itself read-only. Moved
# with Docker Desktop's "Disk image location", the folder is named in its
# settings (customWslDistroDir; dataFolder for the Hyper-V backend).
function Get-M2DockerDataLocation {
    $directory = ''
    $appData = [Environment]::GetFolderPath('ApplicationData')
    if ($appData) {
        foreach ($name in @('settings-store.json', 'settings.json')) {
            $path = Join-Path (Join-Path $appData 'Docker') $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
            try {
                $settings = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
                foreach ($property in @($settings.PSObject.Properties)) {
                    if ($property.Name -match '(?i)^(?:customWslDistroDir|dataFolder)$' -and [string]$property.Value) {
                        $directory = [string]$property.Value
                        break
                    }
                }
            }
            catch {}
            if ($directory) { break }
        }
    }
    $localAppData = [Environment]::GetFolderPath('LocalApplicationData')
    if (-not $directory -and $localAppData) { $directory = Join-Path (Join-Path $localAppData 'Docker') 'wsl' }
    if (-not $directory) { return $null }

    $disks = @()
    if (Test-Path -LiteralPath $directory -PathType Container) {
        $disks = @(Get-ChildItem -LiteralPath $directory -Filter '*.vhdx' -Recurse -Depth 2 -File -ErrorAction SilentlyContinue |
            ForEach-Object { [pscustomobject]@{ Path = $_.FullName; Bytes = [long]$_.Length } })
    }
    $free = $null
    $total = $null
    $driveName = ''
    try {
        $rootPath = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($directory))
        if ($rootPath) {
            $drive = [IO.DriveInfo]::new($rootPath)
            $driveName = $drive.Name.TrimEnd('\')
            if ($drive.IsReady) {
                $free = [long]$drive.AvailableFreeSpace
                $total = [long]$drive.TotalSize
            }
        }
    }
    catch {}
    return [pscustomobject]@{
        Directory = $directory
        Drive = $driveName
        FreeBytes = $free
        TotalBytes = $total
        Disks = @($disks)
    }
}

# A write to Docker's disk, and nothing else. The engine goes on answering
# `docker info' after its disk has gone read-only, so the preflight said
# "mozna uruchomic serwer" while every build died at its first write. A volume
# created and removed at once is a write there (its directory and the volume
# store's database) that touches nothing of the stack. Answers the daemon's
# own words when the write fails for want of a disk, '' otherwise - including
# when it fails for any other reason, which is not this check's to name.
function Get-M2DockerDiskFault {
    $name = 'm2-disk-probe-' + [Guid]::NewGuid().ToString('N').Substring(0, 12)
    $create = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments ('volume create --label com.metin2.probe=1 ' + $name) -TimeoutMilliseconds 15000
    if ($create.ExitCode -eq 0) {
        [void](Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments ('volume rm -f ' + $name) -TimeoutMilliseconds 15000)
        return ''
    }
    $text = ([string]$create.Output).Trim()
    if ($text -notmatch '(?i)read-only file system|input/output error|no space left on device') { return '' }
    $lines = @($text -split '\r?\n' | Where-Object { $_.Trim() })
    return ([string]$lines[$lines.Count - 1]).Trim()
}

# What a support bundle says about disks, because the one question a report
# like pattsito's cannot answer without it is whether the drive was full.
# User profile paths are shortened to %USERPROFILE%.
function Get-M2DiskSpaceReport {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $profilePath = [Environment]::GetFolderPath('UserProfile')
    $hide = {
        param([string]$Path)
        if ($profilePath -and $Path.StartsWith($profilePath, [StringComparison]::OrdinalIgnoreCase)) {
            return '%USERPROFILE%' + $Path.Substring($profilePath.Length)
        }
        return $Path
    }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('Dyski Windows:')
    foreach ($drive in @([IO.DriveInfo]::GetDrives())) {
        try {
            if ($drive.DriveType -ne [IO.DriveType]::Fixed -or -not $drive.IsReady) { continue }
            $lines.Add(('  {0} wolne {1} z {2}' -f $drive.Name.TrimEnd('\'), (Format-M2Bytes $drive.AvailableFreeSpace), (Format-M2Bytes $drive.TotalSize)))
        }
        catch {}
    }
    $lines.Add('')
    $lines.Add('Docker Desktop:')
    $data = Get-M2DockerDataLocation
    if ($data) {
        $lines.Add('  folder dysku: ' + (& $hide $data.Directory))
        if (@($data.Disks).Count -eq 0) { $lines.Add('  (nie znaleziono plikow .vhdx)') }
        foreach ($disk in @($data.Disks)) {
            $lines.Add(('  {0}: {1}' -f (& $hide $disk.Path), (Format-M2Bytes $disk.Bytes)))
        }
        if ($null -ne $data.FreeBytes) {
            $lines.Add(('  wolne na dysku {0} {1}' -f $data.Drive, (Format-M2Bytes $data.FreeBytes)))
        }
    }
    else {
        $lines.Add('  (nie ustalono folderu dysku)')
    }
    $lines.Add('')
    $lines.Add('Kopie aktualizacji (backups):')
    $backups = Join-Path ([IO.Path]::GetFullPath($ServerRoot)) 'backups'
    if (Test-Path -LiteralPath $backups -PathType Container) {
        $copies = @(Get-ChildItem -LiteralPath $backups -Directory -ErrorAction SilentlyContinue)
        $bytes = 0L
        foreach ($file in @(Get-ChildItem -LiteralPath $backups -Recurse -File -Force -ErrorAction SilentlyContinue)) { $bytes += [long]$file.Length }
        $lines.Add(('  {0} katalogow, razem {1}' -f $copies.Count, (Format-M2Bytes $bytes)))
    }
    else {
        $lines.Add('  (brak)')
    }
    return ($lines -join [Environment]::NewLine)
}

function Get-M2InstallationProjectName {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $statePath = Join-Path $ServerRoot '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$state.projectName) { return [string]$state.projectName }
        }
        catch {}
    }

    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_COMPOSE_PROJECT_NAME=(.+)$' | Select-Object -First 1
        if ($match -and $match.Matches[0].Groups[1].Value) {
            return $match.Matches[0].Groups[1].Value.Trim()
        }
    }
    return ''
}

function Get-M2PanelPort {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_PANEL_PUBLIC_PORT=(\d+)$' | Select-Object -First 1
        if ($match) { return [int]$match.Matches[0].Groups[1].Value }
    }
    return 7788
}

function Get-M2ListeningProcess {
    param([Parameter(Mandatory = $true)][int]$Port)

    try {
        $connection = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction Stop | Select-Object -First 1
        if (-not $connection) { return $null }
        $processName = ''
        try { $processName = (Get-Process -Id $connection.OwningProcess -ErrorAction Stop).ProcessName } catch {}
        return [pscustomobject]@{ Pid = [int]$connection.OwningProcess; Name = $processName }
    }
    catch {
        $netstat = Invoke-M2DiagnosticProcess -FileName 'netstat.exe' -Arguments '-ano -p tcp' -TimeoutMilliseconds 3000
        if ($netstat.ExitCode -ne 0) { return $null }
        foreach ($line in ($netstat.Output -split '\r?\n')) {
            if ($line -match ('(?i)^\s*TCP\s+\S+:' + $Port + '\s+\S+\s+LISTENING\s+(\d+)\s*$')) {
                $pidValue = [int]$Matches[1]
                $processName = ''
                try { $processName = (Get-Process -Id $pidValue -ErrorAction Stop).ProcessName } catch {}
                return [pscustomobject]@{ Pid = $pidValue; Name = $processName }
            }
        }
    }
    return $null
}

function Get-M2DockerPortOwner {
    param(
        [Parameter(Mandatory = $true)][int]$Port,
        [AllowEmptyString()][string]$CurrentProject = ''
    )

    foreach ($holder in @(Get-M2DockerPortHolders -Ports @($Port) -CurrentProject $CurrentProject)) {
        return $holder
    }
    return $null
}

# Which of the wanted ports a container's PORTS column actually publishes.
#
# The column is a comma-separated list of "[host:]HOST->CONTAINER/proto", and a
# published range collapses into a single entry: "127.0.0.1:13000-13002->
# 13000-13002/tcp". The host side is what matters, and it is either one number
# or two around a dash.
function Get-M2PublishedPortMatches {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$PortsText,
        [Parameter(Mandatory = $true)][int[]]$Ports
    )

    $matched = @()
    if (-not $PortsText) { return $matched }
    foreach ($entry in ($PortsText -split ',')) {
        $piece = $entry.Trim()
        if (-not $piece -or $piece -notmatch '->') { continue }
        $hostSide = ($piece -split '->')[0].Trim()
        # Drop the bind address; IPv6 arrives as "[::]:7788".
        if ($hostSide -match '^\[[^\]]*\]:(.+)$') { $hostSide = $Matches[1] }
        elseif ($hostSide -match '^[^:]*:(.+)$') { $hostSide = $Matches[1] }
        $first = 0
        $last = 0
        if ($hostSide -match '^(\d+)-(\d+)$') { $first = [int]$Matches[1]; $last = [int]$Matches[2] }
        elseif ($hostSide -match '^(\d+)$') { $first = [int]$Matches[1]; $last = $first }
        else { continue }
        foreach ($port in @($Ports)) {
            $number = [int]$port
            if ($number -ge $first -and $number -le $last -and $matched -notcontains $number) {
                $matched += $number
            }
        }
    }
    return $matched
}

# Every running container that publishes one of these host ports, with its
# compose project and the folder it was started from. The folder is the half an
# operator needs and never had: one machine here carries five projects of this
# same server (m2dep, m2zip, m2mt, m2fresh, metin2), each from its own
# directory, and "another installation is using the port" without naming which
# one is advice nobody can act on.
function Get-M2DockerPortHolders {
    param(
        [Parameter(Mandatory = $true)][int[]]$Ports,
        [AllowEmptyString()][string]$CurrentProject = ''
    )

    $holders = @()
    if (@($Ports).Count -eq 0) { return $holders }
    $dockerPs = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments 'ps --format "{{json .}}"' -TimeoutMilliseconds 6000
    if ($dockerPs.ExitCode -ne 0) { return $holders }
    foreach ($line in ($dockerPs.Output -split '\r?\n')) {
        if (-not $line.Trim()) { continue }
        try { $container = $line | ConvertFrom-Json } catch { continue }
        # Docker prints a published range as one entry - "127.0.0.1:13000-13002
        # ->13000-13002/tcp" - so matching the literal "13001->" found nothing
        # and the three game channels looked like they belonged to no container
        # at all. The preflight then called the player's own running server a
        # foreign program and refused to start (sizowski, 13 September, on the
        # very check meant to help). Every host side is parsed, single or range.
        $matched = @(Get-M2PublishedPortMatches -PortsText ([string]$container.Ports) -Ports $Ports)
        if ($matched.Count -eq 0) { continue }
        $project = ''
        $workingDir = ''
        $labels = [string]$container.Labels
        if ($labels -match '(?:^|,)com\.docker\.compose\.project=([^,]+)') { $project = $Matches[1] }
        if ($labels -match '(?:^|,)com\.docker\.compose\.project\.working_dir=([^,]+)') { $workingDir = $Matches[1] }
        $holders += [pscustomobject]@{
            Container = [string]$container.Names
            Project = $project
            WorkingDir = $workingDir
            Ports = @($matched)
            IsCurrentProject = [bool]($CurrentProject -and $project -and $project.Equals($CurrentProject, [StringComparison]::OrdinalIgnoreCase))
        }
    }
    return $holders
}

# The host ports this installation publishes, read from its own .env. The
# preflight used to look at the panel's 7788 alone, so a collision on 7790 - the
# advanced panel, which a second copy of the server publishes too - passed the
# check with "OK: port panelu jest wolny" and then stopped compose after the
# images were built: "Bind for 127.0.0.1:7790 failed: port is already
# allocated". compose gives up on the first taken port, so the check has to know
# all of them.
function Get-M2StackHostPorts {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    $values = @{}
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        foreach ($line in @([IO.File]::ReadAllLines($envPath))) {
            if ("$line" -match '^\s*([A-Za-z0-9_]+)=(.*)$') { $values[$Matches[1]] = $Matches[2].Trim() }
        }
    }
    # ClientFixed: a port the game client dials by its own number. The local
    # server entry in the client's serverinfo.py logs in on 11000, and the
    # login answer names the core's own port inside the container, 13000 and
    # up - so moving either in .env starts a server nobody can log in to.
    # Kordix (23 September) was told by this check to do exactly that when
    # MSI_GamebarTool held 11000. Only the panels, the ItemShop and the
    # database are reached through the port .env publishes.
    $ports = @()
    foreach ($entry in @(
            @{ Key = 'M2_PANEL_PUBLIC_PORT'; Default = 7788; Name = 'panel WWW'; ClientFixed = $false },
            @{ Key = 'M2_SEBAN_PANEL_PORT'; Default = 7790; Name = 'panel zaawansowany'; ClientFixed = $false },
            @{ Key = 'M2_ITEMSHOP_PUBLIC_PORT'; Default = 7791; Name = 'ItemShop'; ClientFixed = $false },
            @{ Key = 'M2_AUTH_PORT'; Default = 11000; Name = 'serwer logowania'; ClientFixed = $true },
            @{ Key = 'M2_DB_PUBLISH_PORT'; Default = 3306; Name = 'baza danych'; ClientFixed = $false })) {
        $value = [string]$values[$entry.Key]
        $number = [int]$entry.Default
        if ($value -match '^\d+$') { $number = [int]$value }
        $ports += [pscustomobject]@{ Port = $number; Name = [string]$entry.Name; ClientFixed = [bool]$entry.ClientFixed }
    }
    # "13000-13002": each channel binds its own port and any one of them can be
    # the one that is taken.
    $first = 13000
    $last = 13002
    $range = [string]$values['M2_GAME_PORT_RANGE']
    if ($range -match '^(\d+)\s*-\s*(\d+)$') { $first = [int]$Matches[1]; $last = [int]$Matches[2] }
    elseif ($range -match '^(\d+)$') { $first = [int]$Matches[1]; $last = $first }
    if ($last -lt $first) { $last = $first }
    if (($last - $first) -gt 32) { $last = $first + 32 }
    for ($p = $first; $p -le $last; $p++) {
        $ports += [pscustomobject]@{ Port = [int]$p; Name = 'kanal gry'; ClientFixed = $true }
    }
    return $ports
}

# Containers of ANOTHER compose project sitting on this installation's ports.
# This is the recurring half of "port jest juz zajety" on a machine that has
# ever held a second copy of the server: every container ships with
# restart: unless-stopped, so Docker Desktop brings the old project back up on
# every engine start and it takes the ports before this installation can - which
# is exactly why quitting Docker by hand does not help. `docker stop` is the fix
# because that flag survives a restart; removing a volume never is.
function Get-M2ForeignPortHolders {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $project = Get-M2InstallationProjectName -ServerRoot $ServerRoot
    $ports = @(Get-M2StackHostPorts -ServerRoot $ServerRoot | ForEach-Object { [int]$_.Port })
    $holders = @(Get-M2DockerPortHolders -Ports $ports -CurrentProject $project)
    return @($holders | Where-Object { $_.Project -and -not $_.IsCurrentProject })
}

function Stop-M2ForeignPortHolders {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $stopped = @()
    $seen = @{}
    foreach ($holder in @(Get-M2ForeignPortHolders -ServerRoot $ServerRoot)) {
        if ($seen.ContainsKey($holder.Project)) { continue }
        $seen[$holder.Project] = $true
        # The whole project, not just the container that answered: its siblings
        # hold the remaining ports and the same restart policy would bring them
        # back on the next engine start.
        $ids = @(& docker ps -aq --filter ("label=com.docker.compose.project=" + $holder.Project) 2>$null |
            Where-Object { $_ })
        if ($ids.Count -eq 0) { continue }
        & docker stop $ids 1>$null 2>$null
        $stopped += [pscustomobject]@{
            Project = [string]$holder.Project
            WorkingDir = [string]$holder.WorkingDir
            Containers = $ids.Count
            Ports = @($holder.Ports)
        }
    }
    return $stopped
}

# The TCP ranges Windows has reserved for itself (Hyper-V, WSL, NAT). A port
# inside one cannot be bound by anything, Docker included, and the failure
# only shows up a second after the images are built. Empty when netsh is
# missing or says nothing - never a reason to refuse a start on its own.
function Get-M2ExcludedPortRanges {
    $ranges = @()
    try {
        $lines = & netsh interface ipv4 show excludedportrange protocol=tcp 2>$null
        foreach ($line in @($lines)) {
            if ("$line" -match '^\s*(\d+)\s+(\d+)\s*\*?\s*$') {
                $ranges += [pscustomobject]@{ Start = [int]$Matches[1]; End = [int]$Matches[2] }
            }
        }
    }
    catch { }
    return $ranges
}

function Get-M2ExcludedPortHit {
    param([Parameter(Mandatory = $true)][int]$Port, [object[]]$Ranges)
    foreach ($r in @($Ranges)) {
        if ($Port -ge $r.Start -and $Port -le $r.End) { return $r }
    }
    return $null
}

function Get-DockerDesktopCandidates {
    # The three stock folders, then wherever the CLI on PATH lives (Docker
    # Desktop keeps docker.exe under <install>\resources\bin), then the
    # uninstall entry's InstallLocation. Two players had Docker on another
    # drive: the launcher stopped Docker Desktop for them and then could not
    # start it again, and the update that happened to come between the two
    # got the blame.
    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($p in @(
            (Join-Path $env:ProgramFiles 'Docker\Docker\Docker Desktop.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'Docker\Docker\Docker Desktop.exe'),
            (Join-Path $env:LOCALAPPDATA 'Docker\Docker Desktop.exe'))) {
        if ($p) { $paths.Add($p) }
    }
    try {
        $cli = (Get-Command docker -ErrorAction Stop).Source
        if ($cli) {
            $root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $cli))
            if ($root) { $paths.Add((Join-Path $root 'Docker Desktop.exe')) }
        }
    }
    catch { }
    foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop',
                       'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop')) {
        try {
            $loc = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).InstallLocation
            if ($loc) { $paths.Add((Join-Path $loc 'Docker Desktop.exe')) }
        }
        catch { }
    }
    return @($paths | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Unique)
}

function Get-M2ClockSkewText {
    param([int]$Seconds)
    $s = [Math]::Abs($Seconds)
    if ($s -ge 3600) { return ('{0} h {1:D2} min' -f [int][Math]::Floor($s / 3600), [int][Math]::Floor(($s % 3600) / 60)) }
    if ($s -ge 60) { return ('{0} min' -f [int][Math]::Floor($s / 60)) }
    return ('{0} s' -f $s)
}

function Get-M2InternetClockSkew {
    # Seconds the Windows clock runs ahead (+) or behind (-) of the time an
    # HTTPS server puts in its Date header; $null when none answered. Docker
    # Desktop's machine takes the Windows clock, and apt inside a build refuses
    # a Release file dated after it ("not valid yet"): Xewi's Windows ran three
    # hours behind on 19 September and no build got past the panel's apt-get.
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch { }
    $response = $null
    $date = ''
    $answeredAt = [DateTime]::UtcNow
    try {
        $request = [Net.HttpWebRequest]::Create('https://api.github.com/')
        $request.Method = 'HEAD'
        $request.Timeout = 4000
        $request.UserAgent = 'metin2-playerbots-launcher'
        try { $response = $request.GetResponse() }
        catch {
            # An HTTP error still carries the header. PowerShell hands the
            # WebException over wrapped, so walk down to it.
            $inner = $_.Exception
            while ($inner -and -not ($inner -is [Net.WebException])) { $inner = $inner.InnerException }
            if ($inner) { $response = $inner.Response }
        }
        $answeredAt = [DateTime]::UtcNow
        if ($response) { $date = [string]$response.Headers['Date'] }
    }
    catch { return $null }
    finally { if ($response) { $response.Close() } }
    if (-not $date) { return $null }
    $parsed = [DateTime]::MinValue
    $styles = [Globalization.DateTimeStyles]::AdjustToUniversal -bor [Globalization.DateTimeStyles]::AssumeUniversal
    if (-not [DateTime]::TryParse($date, [Globalization.CultureInfo]::InvariantCulture, $styles, [ref]$parsed)) { return $null }
    return [int][Math]::Round(($answeredAt - $parsed).TotalSeconds)
}

function Get-M2DockerClockSkew {
    # Seconds Docker's machine runs ahead (+) or behind (-) of Windows. It is
    # set from Windows when it starts and can fall behind after the computer
    # sleeps; a restart of Docker Desktop puts it right.
    $probe = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments 'info --format "{{.SystemTime}}"' -TimeoutMilliseconds 3500
    if ($probe.ExitCode -ne 0 -or $probe.TimedOut) { return $null }
    if ([string]$probe.Output -notmatch '(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)(?:\.\d+)?(Z|[+-]\d\d:\d\d)') { return $null }
    $stamp = $Matches[1] + $(if ($Matches[2] -eq 'Z') { '+00:00' } else { $Matches[2] })
    $parsed = [DateTimeOffset]::MinValue
    if (-not [DateTimeOffset]::TryParseExact($stamp, "yyyy-MM-dd'T'HH:mm:sszzz", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$parsed)) { return $null }
    return [int][Math]::Round(($parsed.UtcDateTime - [DateTime]::UtcNow).TotalSeconds)
}

function Get-M2DockerPreflight {
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [switch]$CheckPanelPort
    )

    $root = [IO.Path]::GetFullPath($ServerRoot)
    $checks = [Collections.ArrayList]::new()
    $blocking = [Collections.ArrayList]::new()
    $warnings = [Collections.ArrayList]::new()
    $dockerCommand = Get-Command docker -ErrorAction SilentlyContinue
    $dockerCliPresent = $null -ne $dockerCommand
    $dockerProcessesRunning = @(Get-Process -Name 'Docker Desktop', 'com.docker.backend' -ErrorAction SilentlyContinue).Count -gt 0
    $dockerEngineReady = $false

    if ($dockerCliPresent) {
        [void]$checks.Add('OK: Docker CLI jest zainstalowany.')
        $dockerProbe = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments 'info --format "{{.ServerVersion}}"' -TimeoutMilliseconds 3500
        $dockerProbeText = if ($null -ne $dockerProbe.Output) { $dockerProbe.Output.Trim() } else { '' }
        # `docker info` exits 0 and still prints the daemon's own refusal. "Error
        # response from daemon: Docker Desktop is unable to start" arrives on
        # stderr, where the server version should be, and the exit code says
        # nothing is wrong - so the check reported "OK: Docker Engine odpowiada
        # (wersja Error response from daemon: Docker Desktop is unable to start)"
        # and a verdict of "mozna uruchomic serwer", six times over, to a player
        # whose WSL was broken. He therefore never saw the one warning that names
        # what to repair, because that warning is only raised when the engine is
        # known to be down. A version is digits and dots; anything else is the
        # engine failing to answer.
        $dockerVersion = ''
        foreach ($probeLine in ($dockerProbeText -split "`r?`n")) {
            $candidate = $probeLine.Trim()
            if ($candidate -match '^\d+(\.\d+)+') {
                $dockerVersion = $candidate
                break
            }
        }
        $dockerEngineReady = $dockerProbe.ExitCode -eq 0 -and -not $dockerProbe.TimedOut -and $dockerVersion -ne ''
        if ($dockerEngineReady) {
            [void]$checks.Add("OK: Docker Engine odpowiada (wersja $dockerVersion).")
        }
        elseif ($dockerProbeText -and -not $dockerProbe.TimedOut) {
            # What the daemon said, not a tidy summary of it: the message names
            # the fault and the player pastes it straight into a report.
            [void]$checks.Add("BLAD: Docker Engine nie odpowiada: $dockerProbeText")
            [void]$warnings.Add("Silnik Dockera nie wystartowal: $dockerProbeText")
        }
        elseif ($dockerProcessesRunning) {
            [void]$checks.Add('UWAGA: Docker Desktop jest otwarty, ale Engine jeszcze nie odpowiada.')
            [void]$warnings.Add('Docker Desktop nadal startuje albo zatrzymał się na błędzie.')
        }
        else {
            [void]$checks.Add('INFO: Docker Engine jest zatrzymany; launcher może go uruchomić.')
        }
    }
    else {
        [void]$checks.Add('BŁĄD: nie znaleziono Docker CLI.')
        [void]$blocking.Add('Zainstaluj Docker Desktop z oficjalnej strony i uruchom ponownie launcher.')
    }

    $desktopCandidates = @(Get-DockerDesktopCandidates)
    if (-not $dockerEngineReady -and $desktopCandidates.Count -eq 0) {
        [void]$blocking.Add('Nie znaleziono programu Docker Desktop. Zainstaluj go przed uruchomieniem serwera.')
    }

    $virtualization = 'Unknown'
    try {
        $computer = Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop
        $processor = Get-CimInstance -ClassName Win32_Processor -ErrorAction Stop | Select-Object -First 1
        if ([bool]$computer.HypervisorPresent -or [bool]$processor.VirtualizationFirmwareEnabled) {
            $virtualization = 'Enabled'
            [void]$checks.Add('OK: wirtualizacja procesora jest dostępna.')
        }
        elseif ($null -ne $processor.VirtualizationFirmwareEnabled) {
            $virtualization = 'Disabled'
            [void]$checks.Add('BŁĄD: wirtualizacja procesora jest wyłączona w BIOS/UEFI.')
            if (-not $dockerEngineReady) {
                [void]$blocking.Add('Włącz AMD SVM/AMD-V albo Intel VT-x w BIOS/UEFI, a następnie zrestartuj komputer.')
            }
        }
    }
    catch {
        [void]$checks.Add('INFO: Windows nie udostępnił stanu wirtualizacji; Docker zweryfikuje go przy starcie.')
    }

    $wslState = 'Missing'
    $wslCommand = Get-Command wsl.exe -ErrorAction SilentlyContinue
    if ($wslCommand) {
        $wslProbe = Invoke-M2DiagnosticProcess -FileName 'wsl.exe' -Arguments '--status' -TimeoutMilliseconds 4500
        if ($wslProbe.ExitCode -eq 0) {
            $wslState = 'Ready'
            [void]$checks.Add('OK: WSL odpowiada.')
        }
        else {
            $wslState = 'Error'
            [void]$checks.Add('UWAGA: polecenie wsl --status nie działa poprawnie.')
            if (-not $dockerEngineReady) {
                [void]$warnings.Add('Jeżeli Docker nie wystartuje, uruchom PowerShell jako administrator, wykonaj wsl --update i wsl --install, a następnie zrestartuj Windows.')
            }
        }
    }
    else {
        [void]$checks.Add('UWAGA: Windows Subsystem for Linux nie jest zainstalowany lub nie jest widoczny.')
        if (-not $dockerEngineReady) {
            [void]$warnings.Add('Docker Desktop zwykle wymaga WSL 2. W razie błędu wykonaj jako administrator: wsl --install, a potem zrestartuj Windows.')
        }
    }

    $panelPort = Get-M2PanelPort -ServerRoot $root
    $portOwner = $null
    $dockerPortOwner = $null
    $currentProject = Get-M2InstallationProjectName -ServerRoot $root
    $foreignHolders = @()
    if ($CheckPanelPort) {
        $busy = @()
        foreach ($entry in @(Get-M2StackHostPorts -ServerRoot $root)) {
            $listener = Get-M2ListeningProcess -Port ([int]$entry.Port)
            if ($null -ne $listener) {
                $busy += [pscustomobject]@{
                    Port = [int]$entry.Port
                    Name = [string]$entry.Name
                    ClientFixed = [bool]$entry.ClientFixed
                    Listener = $listener
                }
            }
        }
        $holders = @()
        if ($busy.Count -gt 0 -and $dockerEngineReady) {
            $holders = @(Get-M2DockerPortHolders -Ports @($busy | ForEach-Object { [int]$_.Port }) -CurrentProject $currentProject)
        }
        if ($busy.Count -eq 0) {
            [void]$checks.Add('OK: wszystkie porty serwera są wolne.')
        }
        foreach ($entry in $busy) {
            $holder = @($holders | Where-Object { $_.Ports -contains [int]$entry.Port }) | Select-Object -First 1
            if ($holder -and $holder.IsCurrentProject) {
                [void]$checks.Add("OK: port $($entry.Port) ($($entry.Name)) należy do tej instalacji ($($holder.Container)).")
            }
            elseif ($holder) {
                $where = if ($holder.WorkingDir) { ", folder $($holder.WorkingDir)" } else { '' }
                [void]$checks.Add("BŁĄD: port $($entry.Port) ($($entry.Name)) zajmuje kontener $($holder.Container) z innej instalacji (projekt $($holder.Project)$where).")
                $foreignHolders += $holder
            }
            elseif ($entry.Listener.Name -match '(?i)^(com\.docker|docker|vpnkit|wslrelay)') {
                # Docker itself holds every published port on Windows, in the
                # name of some container. Not recognising which one is a gap in
                # this check, never a reason to refuse the start: saying "close
                # com.docker.backend" to somebody whose own server is running is
                # advice that cannot be followed.
                [void]$checks.Add("UWAGA: port $($entry.Port) ($($entry.Name)) trzyma Docker ($($entry.Listener.Name)); nie rozpoznano kontenera - zakladam, ze to ta instalacja.")
                [void]$warnings.Add("Port $($entry.Port) jest zajety przez Dockera. Jesli serwer nie wstanie, sprawdz DIAGNOSTYKA i zatrzymaj inne instalacje.")
            }
            else {
                $who = if ($entry.Listener.Name) { "proces $($entry.Listener.Name), PID $($entry.Listener.Pid)" } else { "PID $($entry.Listener.Pid)" }
                [void]$checks.Add("BŁĄD: port $($entry.Port) ($($entry.Name)) zajmuje $who.")
                if ($entry.ClientFixed) {
                    [void]$blocking.Add("Port $($entry.Port) ($($entry.Name)) zajmuje $who. Zamknij ten program (Menedżer zadań, karta Szczegóły, Zakończ zadanie) i kliknij GRAJ jeszcze raz. Nie zmieniaj tego portu w pliku .env: klient gry łączy się zawsze z portem 11000 i kanałami od 13000, więc po zmianie nie dałoby się zalogować.")
                }
                else {
                    [void]$blocking.Add("Port $($entry.Port) ($($entry.Name)) zajmuje $who. Zamknij ten program albo zmień port w pliku linux-port\docker\.env.")
                }
            }
        }
        if ($foreignHolders.Count -gt 0) {
            $projects = @($foreignHolders | ForEach-Object { $_.Project } | Select-Object -Unique)
            # Single quotes: PowerShell's tokenizer accepts the typographic
            # double quotes as string delimiters, so the pair around the
            # launcher's own menu entry ends a double-quoted string mid-sentence
            # and the whole module stops parsing.
            [void]$blocking.Add(
                ('Porty serwera trzyma inna instalacja tego samego serwera (projekt: {0}). ' +
                 'Launcher zatrzymuje ją sam przy GRAJ i przy ZAINSTALUJ AKTUALIZACJE, a w wersji ' +
                 'konsolowej jest to opcja 21 (Zwolnij porty) - bez ruszania bazy, wolumenów i postępu. ' +
                 'Jeśli ten komunikat wraca mimo to, uruchom Docker Desktop i spróbuj ponownie. ' +
                 'Samo wyłączenie Dockera nie pomaga: te kontenery mają politykę restart=unless-stopped, ' +
                 'więc wracają przy każdym starcie silnika i znów zajmują porty.') -f ($projects -join ', '))
        }
        # Kept for callers that only ask about the panel.
        $panelBusy = @($busy | Where-Object { [int]$_.Port -eq [int]$panelPort }) | Select-Object -First 1
        if ($panelBusy) { $portOwner = $panelBusy.Listener }
        $panelHolder = @($holders | Where-Object { $_.Ports -contains [int]$panelPort }) | Select-Object -First 1
        if ($panelHolder) { $dockerPortOwner = $panelHolder }
    }

    # Ports the stack binds on the host, against the ranges Windows reserved.
    # The game ports are the ones that fail in practice: a range that starts
    # at 11000 or 13000 blocks the login server or the channel, and nothing
    # in the launcher used to say so.
    # @(): a function's array of one unrolls to a bare object and of none to
    # $null, and under StrictMode neither has .Count - every player whose
    # Windows reserved exactly one port range (or none) got "The property
    # 'Count' cannot be found on this object" from Start, StartDocker and
    # Logs alike, because all three run this preflight.
    $excludedRanges = @(Get-M2ExcludedPortRanges)
    if ($excludedRanges.Count -gt 0) {
        $envPath = Join-Path $root 'linux-port\docker\.env'
        $authPort = 11000
        $dbPort = 3306
        if (Test-Path -LiteralPath $envPath -PathType Leaf) {
            $m = Select-String -LiteralPath $envPath -Pattern '^M2_AUTH_PORT=(\d+)$' | Select-Object -First 1
            if ($m) { $authPort = [int]$m.Matches[0].Groups[1].Value }
            $m = Select-String -LiteralPath $envPath -Pattern '^M2_DB_PUBLISH_PORT=(\d+)$' | Select-Object -First 1
            if ($m) { $dbPort = [int]$m.Matches[0].Groups[1].Value }
        }
        $stackPorts = @(
            @{ Port = $authPort; Name = 'serwer logowania' },
            @{ Port = 13000; Name = 'kanal gry' },
            @{ Port = 13001; Name = 'kanal gry' },
            @{ Port = 13002; Name = 'kanal gry' },
            @{ Port = $dbPort; Name = 'baza danych' },
            @{ Port = [int]$panelPort; Name = 'panel' }
        )
        $hits = @()
        foreach ($p in $stackPorts) {
            $hit = Get-M2ExcludedPortHit -Port $p.Port -Ranges $excludedRanges
            if ($hit) { $hits += "$($p.Port) ($($p.Name), zakres $($hit.Start)-$($hit.End))" }
        }
        if ($hits.Count -gt 0) {
            [void]$checks.Add("BŁĄD: Windows zarezerwował porty serwera: $($hits -join ', ').")
            [void]$blocking.Add('Port serwera leży w zakresie zarezerwowanym przez Windows (Hyper-V/WSL), więc Docker nie może na nim nasłuchiwać. Uruchom PowerShell jako administrator: net stop winnat, kliknij GRAJ, a po starcie serwera: net start winnat. Zwykle pomaga też restart Windows.')
        }
        else {
            [void]$checks.Add('OK: żaden port serwera nie leży w zakresie zarezerwowanym przez Windows.')
        }
    }

    # The clock: a warning and never a stop. The build tolerates a clock that
    # runs behind where it cheaply can (the panel, the game's runtime stage),
    # and nothing in the running server needs the right hour - but a fresh
    # build of the game's libraries still asks apt, and the player is the only
    # one who can put the clock right.
    $clockSkew = Get-M2InternetClockSkew
    if ($null -ne $clockSkew) {
        if ([Math]::Abs($clockSkew) -le 300) {
            [void]$checks.Add('OK: zegar Windows zgadza się z internetem.')
        }
        else {
            $which = $(if ($clockSkew -lt 0) { 'spóźnia się' } else { 'śpieszy się' })
            [void]$checks.Add(('UWAGA: zegar Windows {0} o {1} względem internetu.' -f $which, (Get-M2ClockSkewText $clockSkew)))
            [void]$warnings.Add('Zegar Windows jest przestawiony, a Docker bierze czas od Windows - budowa serwera może się zatrzymać na komunikacie „Release file ... is not valid yet”. Ustawienia → Czas i język → Data i godzina: włącz „Ustaw czas automatycznie”, sprawdź strefę czasową (dla Polski: Warszawa) i kliknij „Synchronizuj teraz”. Potem zamknij Docker Desktop (ikona w zasobniku → Quit) i kliknij GRAJ.')
        }
    }
    if ($dockerEngineReady) {
        $dockerSkew = Get-M2DockerClockSkew
        if ($null -ne $dockerSkew -and [Math]::Abs($dockerSkew) -gt 300) {
            [void]$checks.Add(('UWAGA: zegar Dockera odbiega od zegara Windows o {0}.' -f (Get-M2ClockSkewText $dockerSkew)))
            [void]$warnings.Add('Zegar maszyny Dockera rozjechał się z zegarem Windows (zdarza się po uśpieniu komputera). Zamknij Docker Desktop (ikona w zasobniku → Quit), uruchom go ponownie i kliknij GRAJ.')
        }
    }

    # The disk under the engine: a write that must succeed, and the room the
    # Windows drive has left to grow it. The write stops the start - nothing
    # can be built or created on a read-only disk, and without this the
    # player downloads and applies the update first and learns it from an
    # "exit code: 255". The room is only a warning: a disk image that has
    # grown before carries free space of its own that no Windows number shows.
    if ($dockerEngineReady) {
        $diskFault = Get-M2DockerDiskFault
        if ($diskFault) {
            [void]$checks.Add("BŁĄD: dysk Dockera nie przyjmuje zapisu ($diskFault).")
            [void]$blocking.Add('Dysk, na którym Docker Desktop trzyma obrazy i bazę świata (docker_data.vhdx), nie przyjmuje zapisu (read-only file system).' + [Environment]::NewLine + (Get-M2DockerDiskRemedy))
        }
        else {
            [void]$checks.Add('OK: dysk Dockera przyjmuje zapis.')
        }
    }
    # A server folder OneDrive holds builds with files missing
    # (Get-M2OneDriveRootFor). Only a warning: a tree OneDrive has not turned
    # into placeholders yet still builds, and the move is the player's to make.
    $oneDrive = Get-M2OneDriveRootFor -Path $root
    if ($oneDrive) {
        [void]$checks.Add("UWAGA: folder serwera leży w OneDrive ($oneDrive).")
        [void]$warnings.Add((Get-M2OneDriveRemedy -OneDriveRoot $oneDrive))
    }
    else {
        [void]$checks.Add('OK: folder serwera nie leży w OneDrive.')
    }
    $dockerData = Get-M2DockerDataLocation
    if ($dockerData -and $null -ne $dockerData.FreeBytes) {
        $where = if ($dockerData.Drive) { $dockerData.Drive } else { $dockerData.Directory }
        if ($dockerData.FreeBytes -lt 15GB) {
            [void]$checks.Add(('UWAGA: na dysku {0} zostało {1} wolnego miejsca, a tam Docker trzyma swój dysk.' -f $where, (Format-M2Bytes $dockerData.FreeBytes)))
            [void]$warnings.Add(('Na dysku {0} zostało tylko {1} wolnego miejsca. Docker trzyma tam swój dysk (docker_data.vhdx), który rośnie przy budowie serwera - świeża budowa potrzebuje ok. 15 GB. Gdy miejsca zabraknie w trakcie budowy, dysk Dockera przechodzi w tryb tylko do odczytu. Zwolnij miejsce przed kliknięciem GRAJ albo ZAINSTALUJ AKTUALIZACJE.' -f $where, (Format-M2Bytes $dockerData.FreeBytes)))
        }
        else {
            [void]$checks.Add(('OK: na dysku {0} jest {1} wolnego miejsca dla Dockera.' -f $where, (Format-M2Bytes $dockerData.FreeBytes)))
        }
    }

    return [pscustomobject]@{
        CanStart = $blocking.Count -eq 0
        DockerCliPresent = $dockerCliPresent
        DockerProcessesRunning = $dockerProcessesRunning
        DockerEngineReady = $dockerEngineReady
        Virtualization = $virtualization
        Wsl = $wslState
        PanelPort = $panelPort
        CurrentProject = $currentProject
        ForeignPortHolders = @($foreignHolders)
        Checks = @($checks)
        Warnings = @($warnings)
        BlockingIssues = @($blocking)
    }
}

function Format-M2DockerPreflightReport {
    param([Parameter(Mandatory = $true)]$Report)

    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('=== DIAGNOSTYKA METIN2 PLAYERBOTS ===')
    foreach ($check in @($Report.Checks)) { $lines.Add([string]$check) }
    if (@($Report.Warnings).Count -gt 0) {
        $lines.Add('')
        $lines.Add('Ostrzeżenia:')
        foreach ($warning in @($Report.Warnings)) { $lines.Add("- $warning") }
    }
    if (@($Report.BlockingIssues).Count -gt 0) {
        $lines.Add('')
        $lines.Add('Co trzeba zrobić:')
        foreach ($issue in @($Report.BlockingIssues)) { $lines.Add("- $issue") }
    }
    $lines.Add('')
    $lines.Add('Wynik: ' + $(if ($Report.CanStart) { 'można uruchomić serwer.' } else { 'najpierw usuń powyższy problem.' }))
    return $lines -join [Environment]::NewLine
}

Export-ModuleMember -Function @(
    'Get-M2LauncherErrorGuidance',
    'Get-M2OneDriveRootFor',
    'Get-M2OneDriveRemedy',
    'Get-M2DockerDiskRemedy',
    'Get-M2DockerDataLocation',
    'Get-M2DockerDiskFault',
    'Get-M2DiskSpaceReport',
    'Get-M2DockerPreflight',
    'Format-M2DockerPreflightReport',
    'Get-M2StackHostPorts',
    'Get-M2PublishedPortMatches',
    'Get-M2DockerPortHolders',
    'Get-M2ForeignPortHolders',
    'Stop-M2ForeignPortHolders'
)
