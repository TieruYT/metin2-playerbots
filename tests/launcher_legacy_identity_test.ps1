[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceScript = Join-Path $repositoryRoot 'start-server.ps1'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('m2-launcher-identity-' + [Guid]::NewGuid().ToString('N'))

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw "ASSERTION FAILED: $Message" }
}

function New-FakeDocker {
    param([Parameter(Mandatory = $true)][string]$Directory)
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $body = @'
@echo off
echo %*>>"%FAKE_DOCKER_CALL_LOG%"
if /I "%1"=="info" exit /b 0
if /I "%1"=="ps" (
  if /I "%2"=="-aq" (
    type "%FAKE_DOCKER_IDS%"
    exit /b 0
  )
)
if /I "%1"=="inspect" (
  type "%FAKE_DOCKER_INSPECT_JSON%"
  exit /b 0
)
if /I "%1"=="volume" (
  if /I "%2"=="ls" exit /b 0
)
exit /b 0
'@
    [IO.File]::WriteAllText((Join-Path $Directory 'docker.cmd'), $body, [Text.Encoding]::ASCII)
}

function New-TestPackage {
    param([Parameter(Mandatory = $true)][string]$Directory)
    $composeDirectory = Join-Path $Directory 'linux-port\docker'
    New-Item -ItemType Directory -Path $composeDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourceScript -Destination (Join-Path $Directory 'start-server.ps1') -Force
    [IO.File]::WriteAllText(
        (Join-Path $composeDirectory '.env'),
        "M2_COMPOSE_PROJECT_NAME=`r`nM2_CONTAINER_PREFIX=`r`nM2_PANEL_PUBLIC_PORT=7788`r`nM2_AUTH_PORT=11000`r`nMARIADB_ROOT_PASSWORD=new-package-secret`r`n",
        [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText(
        (Join-Path $composeDirectory 'docker-compose.yml'),
        "name: `${M2_COMPOSE_PROJECT_NAME:-metin2}`r`nservices:`r`n  mariadb:`r`n    image: mariadb:10.11`r`nvolumes:`r`n  db-data:`r`n",
        [Text.UTF8Encoding]::new($false))
}

function New-LegacyDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$Secret
    )
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path $Directory '.env'),
        "MARIADB_ROOT_PASSWORD=$Secret`r`nM2_DB_PASSWORD=game-$Secret`r`nM2_COMPOSE_PROJECT_NAME=`r`n",
        [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText(
        (Join-Path $Directory 'docker-compose.yml'),
        "name: metin2`r`nservices:`r`n  mariadb:`r`n    image: mariadb:10.11`r`nvolumes:`r`n  db-data:`r`n",
        [Text.UTF8Encoding]::new($false))
}

function New-DbInspection {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][string]$Project,
        [Parameter(Mandatory = $true)][string]$Prefix,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )
    return [ordered]@{
        Id = $Id
        Name = "/$Prefix-db"
        Config = [ordered]@{
            Labels = [ordered]@{
                'com.docker.compose.project' = $Project
                'com.docker.compose.service' = 'mariadb'
                'com.docker.compose.project.working_dir' = $WorkingDirectory
            }
        }
        Mounts = @([ordered]@{
            Type = 'volume'
            Destination = '/var/lib/mysql'
            Name = "${Project}_db-data"
        })
    }
}

function Invoke-IdentityProbe {
    param(
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [Parameter(Mandatory = $true)][object[]]$Containers,
        [Parameter(Mandatory = $true)][string]$FakeDockerDirectory,
        [Parameter(Mandatory = $true)][string]$CaseDirectory
    )
    $idsPath = Join-Path $CaseDirectory 'ids.txt'
    $jsonPath = Join-Path $CaseDirectory 'inspect.json'
    $logPath = Join-Path $CaseDirectory 'docker-calls.log'
    [IO.File]::WriteAllLines($idsPath, @($Containers | ForEach-Object { [string]$_.Id }), [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($jsonPath, (ConvertTo-Json -InputObject $Containers -Depth 8), [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($logPath, '', [Text.UTF8Encoding]::new($false))

    $oldPath = $env:PATH
    $oldIds = $env:FAKE_DOCKER_IDS
    $oldJson = $env:FAKE_DOCKER_INSPECT_JSON
    $oldLog = $env:FAKE_DOCKER_CALL_LOG
    $oldPreference = $ErrorActionPreference
    try {
        $env:PATH = $FakeDockerDirectory + [IO.Path]::PathSeparator + $oldPath
        $env:FAKE_DOCKER_IDS = $idsPath
        $env:FAKE_DOCKER_INSPECT_JSON = $jsonPath
        $env:FAKE_DOCKER_CALL_LOG = $logPath
        $ErrorActionPreference = 'Continue'
        $processOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PackageRoot 'start-server.ps1') -IdentityOnly 2>&1 | Out-String
        return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Calls = [IO.File]::ReadAllText($logPath); Output = $processOutput }
    }
    finally {
        $ErrorActionPreference = $oldPreference
        $env:PATH = $oldPath
        $env:FAKE_DOCKER_IDS = $oldIds
        $env:FAKE_DOCKER_INSPECT_JSON = $oldJson
        $env:FAKE_DOCKER_CALL_LOG = $oldLog
    }
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    $fakeDocker = Join-Path $testRoot 'fake-docker'
    New-FakeDocker -Directory $fakeDocker

    # One old stack: adopt its project name, exact db-data volume and secrets.
    $singleCase = Join-Path $testRoot 'single'
    $singlePackage = Join-Path $singleCase 'package'
    $singleLegacy = Join-Path $singleCase 'legacy-server'
    New-TestPackage -Directory $singlePackage
    New-LegacyDirectory -Directory $singleLegacy -Secret 'legacy-secret'
    $singleContainer = New-DbInspection -Id 'db-one' -Project 'legacy' -Prefix 'legacy' -WorkingDirectory $singleLegacy
    $singleResult = Invoke-IdentityProbe -PackageRoot $singlePackage -Containers @($singleContainer) -FakeDockerDirectory $fakeDocker -CaseDirectory $singleCase
    Assert-True ($singleResult.ExitCode -eq 0) "single legacy installation should be adopted. Output: $($singleResult.Output)"

    $identity = Get-Content -LiteralPath (Join-Path $singlePackage '.m2install.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $newEnvironment = [IO.File]::ReadAllText((Join-Path $singlePackage 'linux-port\docker\.env'))
    Assert-True ($identity.projectName -eq 'legacy') 'Compose project must match the legacy project'
    Assert-True ($identity.containerPrefix -eq 'legacy') 'container prefix must match the legacy stack'
    Assert-True ($identity.databaseVolume -eq 'legacy_db-data') 'the exact existing db-data volume must be recorded'
    Assert-True ($newEnvironment -match '(?m)^MARIADB_ROOT_PASSWORD=legacy-secret$') 'legacy MariaDB credentials must be preserved'
    Assert-True ($newEnvironment -match '(?m)^M2_COMPOSE_PROJECT_NAME=legacy$') 'new compose must resolve to the legacy project'
    Assert-True ($singleResult.Calls -notmatch '(?im)\b(?:rm|down)\b|volume\s+rm|\s-v(?:\s|$)') 'adoption must not remove containers or volumes'

    # More than one old stack: fail closed and do not choose or write identity.
    $multiCase = Join-Path $testRoot 'multiple'
    $multiPackage = Join-Path $multiCase 'package'
    $legacyA = Join-Path $multiCase 'legacy-a'
    $legacyB = Join-Path $multiCase 'legacy-b'
    New-TestPackage -Directory $multiPackage
    New-LegacyDirectory -Directory $legacyA -Secret 'secret-a'
    New-LegacyDirectory -Directory $legacyB -Secret 'secret-b'
    $containers = @(
        (New-DbInspection -Id 'db-a' -Project 'servera' -Prefix 'servera' -WorkingDirectory $legacyA),
        (New-DbInspection -Id 'db-b' -Project 'serverb' -Prefix 'serverb' -WorkingDirectory $legacyB)
    )
    $multiResult = Invoke-IdentityProbe -PackageRoot $multiPackage -Containers $containers -FakeDockerDirectory $fakeDocker -CaseDirectory $multiCase
    Assert-True ($multiResult.ExitCode -ne 0) "multiple installations must stop as ambiguous. Output: $($multiResult.Output)"
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $multiPackage '.m2install.json'))) 'ambiguous detection must not write identity'
    $untouchedEnvironment = [IO.File]::ReadAllText((Join-Path $multiPackage 'linux-port\docker\.env'))
    Assert-True ($untouchedEnvironment -match '(?m)^M2_COMPOSE_PROJECT_NAME=\r?$') 'ambiguous detection must not select a project'
    Assert-True ($multiResult.Calls -notmatch '(?im)\b(?:rm|down)\b|volume\s+rm|\s-v(?:\s|$)') 'ambiguous detection must not remove containers or volumes'

    [pscustomobject]@{
        SingleInstallAdopted = $true
        ExistingDatabaseVolume = [string]$identity.databaseVolume
        LegacySecretsPreserved = $true
        MultipleInstallationsRejected = $true
        DestructiveDockerCommands = $false
    } | ConvertTo-Json
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
