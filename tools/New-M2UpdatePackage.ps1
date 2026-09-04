[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateSet('server', 'client')][string]$Type,
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$FileList,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$DownloadUrl = ''
)

$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath($SourceRoot).TrimEnd('\')
$listPath = [IO.Path]::GetFullPath($FileList)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Source root does not exist: $source" }
if (-not (Test-Path -LiteralPath $listPath -PathType Leaf)) { throw "File list does not exist: $listPath" }
New-Item -ItemType Directory -Path $output -Force | Out-Null

$temp = Join-Path ([IO.Path]::GetTempPath()) ('m2-package-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    $entries = @(Get-Content -LiteralPath $listPath | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') })
    if ($entries.Count -eq 0) { throw 'The file list is empty.' }

    # A line may name a directory with a wildcard - "…/src/playerbot_*" - and it
    # expands to whatever is there. Splitting a source file used to mean editing
    # this list too, and forgetting to is what shipped a manager without its own
    # headers twice. A pattern that matches nothing is still an error: it means
    # the tree moved and the package would be silently short.
    $expanded = @()
    foreach ($entry in $entries) {
        if ($entry -notmatch '[\*\?]') { $expanded += $entry; continue }
        $relativePattern = $entry.Replace('/', '\').TrimStart('\')
        if ([IO.Path]::IsPathRooted($relativePattern) -or
            $relativePattern.Split('\') -contains '..') {
            throw "Unsafe pattern: $entry"
        }
        $directory = Split-Path -Parent $relativePattern
        $leaf = Split-Path -Leaf $relativePattern
        $searchRoot = Join-Path $source $directory
        if (-not (Test-Path -LiteralPath $searchRoot -PathType Container)) {
            throw "Pattern directory does not exist: $directory"
        }
        $matched = @(Get-ChildItem -LiteralPath $searchRoot -File -Filter $leaf |
            Sort-Object Name | ForEach-Object { (Join-Path $directory $_.Name) })
        if ($matched.Count -eq 0) { throw "Pattern matched no files: $entry" }
        $expanded += $matched
    }
    $entries = @($expanded | Select-Object -Unique)


    # The overlay sources and the staged build context are two copies of the
    # same files, and a package that carries one without the other is what took
    # every player's server down in 1.22.4 and again in 1.23.2: the compiler saw
    # a manager whose header was still the previous release's, or missing
    # outright. Refuse to build such a package at all.
    $overlayPrefix = 'linux-port\overlays\playerbot\src\game\src\'
    $stagedPrefix = 'linux-port\docker\game\src\server\game\src\'
    $overlayNames = @()
    $stagedNames = @()
    foreach ($relativeInput in $entries) {
        $relative = $relativeInput.Replace('/', '\').TrimStart('\')
        if ($relative.StartsWith($overlayPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            $overlayNames += $relative.Substring($overlayPrefix.Length)
        }
        elseif ($relative.StartsWith($stagedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            $stagedNames += $relative.Substring($stagedPrefix.Length)
        }
    }
    foreach ($name in $stagedNames) {
        if ($overlayNames -notcontains $name) {
            # The launcher syncs overlay -> build context before every build, so
            # an overlay copy left behind by an older release would overwrite the
            # good staged one on the player's machine. Both halves ship together
            # or neither does.
            throw "Build-context copy shipped without its Playerbot source: $name. Add $overlayPrefix$name to $listPath."
        }
    }
    foreach ($name in $overlayNames) {
        if ($stagedNames -notcontains $name) {
            throw "Playerbot source shipped without its build-context copy: $name. Add $stagedPrefix$name to $listPath."
        }
        $a = Join-Path $source ($overlayPrefix + $name)
        $b = Join-Path $source ($stagedPrefix + $name)
        if ((Get-FileHash -LiteralPath $a -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $b -Algorithm SHA256).Hash) {
            throw "Playerbot source and its build-context copy differ: $name. Run prepare-context.sh or copy it across before packaging."
        }
    }

    foreach ($relativeInput in $entries) {
        $relative = $relativeInput.Replace('/', '\').TrimStart('\')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Split('\') -contains '..') {
            throw "Unsafe relative path: $relativeInput"
        }
        if ($relative -ieq 'linux-port\docker\.env' -or $relative.StartsWith('.git\')) {
            throw "Protected file cannot be published in an update: $relative"
        }
        # PowerShell's automatic pipeline-enumerator variable used to be
        # reused here as an ordinary name. Handing it to a cmdlet made
        # packaging hang at random, with the process sitting fully idle.
        $sourceFile = [IO.Path]::GetFullPath((Join-Path $source $relative))
        if (-not $sourceFile.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Path leaves source root: $relative"
        }
        if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
            throw "Listed file does not exist: $relative"
        }
        $destination = Join-Path $temp $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $sourceFile -Destination $destination -Force
    }

    $safeVersion = $Version -replace '[^A-Za-z0-9._-]', '-'
    $zipName = "metin2-$Type-update-$safeVersion.zip"
    $zipPath = Join-Path $output $zipName
    Compress-Archive -Path (Join-Path $temp '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force
    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
    $component = [ordered]@{
        version = $Version
        url = $DownloadUrl
        sha256 = $hash
        size = (Get-Item -LiteralPath $zipPath).Length
    }
    $fragmentPath = Join-Path $output ("$Type-manifest-fragment-$safeVersion.json")
    $component | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $fragmentPath -Encoding UTF8

    Write-Host "Created: $zipPath"
    Write-Host "SHA-256: $hash"
    Write-Host "Manifest fragment: $fragmentPath"
}
finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
