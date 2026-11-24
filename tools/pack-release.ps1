param(
    [string]$Version = "0.1.0",
    [string]$Notes = "Update.",
    [string]$BuildDir = "$PSScriptRoot\..\build",
    [string]$SiteOrigin = "https://nuclip.xyz",
    [string]$PackageOrigin = "https://cdn.nuclip.xyz"
)

$ErrorActionPreference = "Stop"
$BuildDir = (Resolve-Path $BuildDir).Path
$stage = Join-Path $env:TEMP ("NuClip-stage-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $stage | Out-Null

$skipDirs = @("CMakeFiles", "NuClip_autogen", ".qt", "Testing", "x64", "Debug", "Release")
$skipExt = @(".pdb", ".obj", ".ilk", ".exp", ".cmake", ".ninja", ".log")
Get-ChildItem -Path $BuildDir -Force | ForEach-Object {
    if ($skipDirs -contains $_.Name) { return }
    if (-not $_.PSIsContainer -and $skipExt -contains $_.Extension) { return }
    if ($_.Name -in @("CMakeCache.txt", "build.ninja", ".ninja_deps", ".ninja_log", "self-test.txt")) { return }
    Copy-Item $_.FullName -Destination (Join-Path $stage $_.Name) -Recurse -Force
}

$dist = Join-Path $PSScriptRoot "..\dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$zip = Join-Path $dist "NuClip-$Version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Remove-Item $stage -Recurse -Force

$hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
$site = Join-Path $PSScriptRoot "..\..\NuClipWeb"
$releases = Join-Path $site "releases"
New-Item -ItemType Directory -Force -Path $releases | Out-Null
$setup = Join-Path $BuildDir "NuClipSetup.exe"
if (Test-Path $setup) {
    $download = Join-Path $site "download"
    New-Item -ItemType Directory -Force -Path $download | Out-Null
    Copy-Item $setup (Join-Path $download "NuClipSetup.exe") -Force
}

$manifestPath = Join-Path $releases "latest.json"
$entry = [pscustomobject]@{
    version = $Version
    notes = $Notes
    package = "$PackageOrigin/NuClip-$Version.zip"
    sha256 = $hash
    published = (Get-Date -Format "yyyy-MM-dd")
}
$existing = @()
if (Test-Path $manifestPath) {
    $old = Get-Content -Raw -Encoding UTF8 $manifestPath | ConvertFrom-Json
    if ($null -ne $old.versions) {
        $existing = @($old.versions)
    } elseif ($old.version -and $old.version -ne $Version) {
        $existing = @([pscustomobject]@{
            version = $old.version
            notes = $old.notes
            package = $old.package
            sha256 = $old.sha256
            published = $old.published
        })
    }
}
$versions = @($entry) + @($existing | Where-Object { $_.version -ne $Version })
$manifest = [ordered]@{
    version = $entry.version
    notes = $entry.notes
    package = $entry.package
    sha256 = $entry.sha256
    published = $entry.published
    versions = $versions
}
$json = ($manifest | ConvertTo-Json -Depth 5).Trim() + "`n"
[System.IO.File]::WriteAllText($manifestPath, $json, (New-Object System.Text.UTF8Encoding $false))
Write-Output "packed $zip"
Write-Output "sha256 $hash"
