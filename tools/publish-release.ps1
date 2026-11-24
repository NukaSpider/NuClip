param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Notes = "Update.",
    [string]$Server = "https://nuclip.xyz",
    [string]$Zip = "",
    [switch]$IncludeInstaller
)

$ErrorActionPreference = "Stop"
if (-not $env:NUCLIP_PUBLISH_TOKEN -or $env:NUCLIP_PUBLISH_TOKEN.Length -lt 16) {
    throw "Set NUCLIP_PUBLISH_TOKEN to a secret of at least 16 characters."
}

if (-not $Zip) {
    & (Join-Path $PSScriptRoot "pack-release.ps1") -Version $Version -Notes $Notes
    $Zip = Join-Path $PSScriptRoot "..\dist\NuClip-$Version.zip"
}
$Zip = (Resolve-Path $Zip).Path
$Server = $Server.TrimEnd("/")
$partBytes = 768KB
if ($env:NUCLIP_PART_BYTES) { $partBytes = [int64]$env:NUCLIP_PART_BYTES }
if ($partBytes -lt 256KB) { $partBytes = 256KB }
$zipInfo = Get-Item $Zip
$count = [math]::Ceiling($zipInfo.Length / $partBytes)
if ($count -gt 1024) { throw "The zip needs more than 1024 parts." }
$notesQuery = [uri]::EscapeDataString($Notes)
$versionQuery = [uri]::EscapeDataString($Version)
$stream = [System.IO.File]::OpenRead($Zip)
try {
    for ($index = 0; $index -lt $count; $index++) {
        $remaining = [math]::Min($partBytes, $zipInfo.Length - ($index * $partBytes))
        $partPath = Join-Path $env:TEMP ("nuclip-part-" + $Version + "-" + $index + ".bin")
        $out = [System.IO.File]::Create($partPath)
        try {
            $buffer = New-Object byte[] (1024 * 1024)
            $left = $remaining
            while ($left -gt 0) {
                $take = [math]::Min($buffer.Length, $left)
                $read = $stream.Read($buffer, 0, $take)
                if ($read -le 0) { throw "The zip ended before the upload finished." }
                $out.Write($buffer, 0, $read)
                $left -= $read
            }
        } finally {
            $out.Dispose()
        }
        Write-Output ("Uploading part {0} of {1}" -f ($index + 1), $count)
        $partUrl = "$Server/api/releases/part?version=$versionQuery&index=$index&count=$count&notes=$notesQuery"
        & curl.exe -fS --http1.1 -X POST `
            -H "Authorization: Bearer $($env:NUCLIP_PUBLISH_TOKEN)" `
            -H "Content-Type: application/octet-stream" `
            --data-binary "@$partPath" `
            $partUrl
        $uploadExit = $LASTEXITCODE
        Remove-Item $partPath -Force -ErrorAction SilentlyContinue
        if ($uploadExit -ne 0) { exit $uploadExit }
        Write-Output ""
    }
} finally {
    $stream.Dispose()
}

if ($IncludeInstaller) {
    $setup = Join-Path $PSScriptRoot "..\build\NuClipSetup.exe"
    if (-not (Test-Path $setup)) { throw "NuClipSetup.exe was not found in the build folder." }
    $setup = (Resolve-Path $setup).Path
    & curl.exe -fS --http1.1 -X POST `
        -H "Authorization: Bearer $($env:NUCLIP_PUBLISH_TOKEN)" `
        -H "Content-Type: application/octet-stream" `
        --data-binary "@$setup" `
        "$Server/api/installer"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Output ""
}
