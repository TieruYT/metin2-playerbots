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
