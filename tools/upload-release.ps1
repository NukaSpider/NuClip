param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Notes = "Update.",
    [string]$Zip = "",
    [string]$Setup = "",
    [switch]$UpdateSetup
)

$ErrorActionPreference = "Stop"
$keys = Join-Path $PSScriptRoot "r2.local.ps1"
if (-not (Test-Path $keys)) { throw "Create tools\r2.local.ps1 and paste the R2 keys there." }
. $keys

if (-not $R2AccountId -or -not $R2AccessKeyId -or -not $R2SecretAccessKey) {
    throw "Paste R2AccountId, R2AccessKeyId, and R2SecretAccessKey into tools\r2.local.ps1."
}
if (-not $PublishToken -or $PublishToken.Length -lt 16) {
    throw "Paste PublishToken into tools\r2.local.ps1. It must match NUCLIP_PUBLISH_TOKEN on the website."
}
if (-not $VirusTotalApiKey) {
    throw "Paste VirusTotalApiKey into tools\r2.local.ps1. Create one at https://www.virustotal.com/gui/my-apikey"
}
if (-not $R2Bucket) { $R2Bucket = "nuclip" }
if (-not $Site) { $Site = "https://nuclip.xyz" }
$Site = $Site.TrimEnd("/")

$explicitZip = $PSBoundParameters.ContainsKey("Zip") -and $Zip
if (-not $Zip) {
    $Zip = Join-Path $PSScriptRoot "..\dist\NuClip-$Version.zip"
}
if (-not $explicitZip) {
    $packArgs = @{ Version = $Version; Notes = $Notes }
    if ($UpdateSetup) { $packArgs.UpdateSetup = $true }
    Write-Output "Packing NuClip-$Version.zip"
    & (Join-Path $PSScriptRoot "pack-release.ps1") @packArgs
}
if (-not (Test-Path $Zip)) { throw "Zip not found: $Zip" }
$Zip = (Resolve-Path $Zip).Path
$setupPath = ""
$setupHash = ""
if ($UpdateSetup) {
    if (-not $Setup) {
        $Setup = Join-Path $PSScriptRoot "..\build\NuClipSetup.exe"
    }
    if (-not (Test-Path $Setup)) { throw "Setup not found: $Setup" }
    $setupPath = (Resolve-Path $Setup).Path
    $setupHash = (Get-FileHash -Algorithm SHA256 $setupPath).Hash.ToLower()
}
$appExe = Join-Path $PSScriptRoot "..\build\NuClip.exe"
if (-not (Test-Path $appExe)) { throw "NuClip.exe not found: $appExe" }
$appExe = (Resolve-Path $appExe).Path
$appHash = (Get-FileHash -Algorithm SHA256 $appExe).Hash.ToLower()

function Get-HmacBytes([byte[]]$Key, [string]$Text) {
    $hmac = [System.Security.Cryptography.HMACSHA256]::new($Key)
    try { return $hmac.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)) }
    finally { $hmac.Dispose() }
}

function Send-R2Object([string]$FilePath, [string]$ObjectKey, [string]$ContentType) {
    $hash = "UNSIGNED-PAYLOAD"
    $now = [DateTime]::UtcNow
    $amzDate = $now.ToString("yyyyMMddTHHmmssZ")
    $date = $now.ToString("yyyyMMdd")
    $hostName = "$R2AccountId.r2.cloudflarestorage.com".ToLower()
    $encodedKey = ($ObjectKey -split "/" | ForEach-Object { [uri]::EscapeDataString($_) }) -join "/"
    $canonicalUri = "/" + $R2Bucket + "/" + $encodedKey
    $canonicalHeaders = "content-type:$ContentType`nhost:$hostName`nx-amz-content-sha256:$hash`nx-amz-date:$amzDate`n"
    $signedHeaders = "content-type;host;x-amz-content-sha256;x-amz-date"
    $canonical = "PUT`n$canonicalUri`n`n$canonicalHeaders`n$signedHeaders`n$hash"
    $canonicalHash = [System.BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))).Replace("-", "").ToLower()
    $scope = "$date/auto/s3/aws4_request"
    $stringToSign = "AWS4-HMAC-SHA256`n$amzDate`n$scope`n$canonicalHash"
    $secret = [Text.Encoding]::UTF8.GetBytes("AWS4$R2SecretAccessKey")
    $signing = Get-HmacBytes (Get-HmacBytes (Get-HmacBytes (Get-HmacBytes $secret $date) "auto") "s3") "aws4_request"
    $signature = [System.BitConverter]::ToString((Get-HmacBytes $signing $stringToSign)).Replace("-", "").ToLower()
    $authorization = "AWS4-HMAC-SHA256 Credential=$R2AccessKeyId/$scope, SignedHeaders=$signedHeaders, Signature=$signature"
    $url = "https://$hostName$canonicalUri"
    Write-Output "Uploading $ObjectKey"
    & curl.exe -fS --http1.1 -X PUT `
        -H "Content-Type: $ContentType" `
        -H "Host: $hostName" `
        -H "x-amz-content-sha256: $hash" `
        -H "x-amz-date: $amzDate" `
        -H "Authorization: $authorization" `
        --upload-file $FilePath `
        $url
    if ($LASTEXITCODE -ne 0) { throw "R2 rejected $ObjectKey." }
}

function Submit-VirusTotal([string]$FilePath, [string]$Name, [string]$Hash) {
    Write-Output "Scanning $Name"
    $uploadUrl = "https://www.virustotal.com/api/v3/files"
    if ((Get-Item $FilePath).Length -gt 32MB) {
        $urlFile = Join-Path $env:TEMP ("vt-url-" + [guid]::NewGuid().ToString("N") + ".json")
        & curl.exe -sS -f -H "x-apikey: $VirusTotalApiKey" -o $urlFile "https://www.virustotal.com/api/v3/files/upload_url"
        if ($LASTEXITCODE -ne 0) { throw "VirusTotal did not provide an upload URL for $Name." }
        $uploadUrl = (Get-Content -Raw -Encoding UTF8 $urlFile | ConvertFrom-Json).data
        Remove-Item $urlFile -Force
    }
    $out = Join-Path $env:TEMP ("vt-" + [guid]::NewGuid().ToString("N") + ".json")
    $code = (& curl.exe -sS -o $out -w "%{http_code}" -X POST -H "x-apikey: $VirusTotalApiKey" -F "file=@$FilePath" $uploadUrl | Out-String).Trim()
    $detail = ""
    if (Test-Path $out) {
        $detail = (Get-Content -Raw -Encoding UTF8 $out).Trim()
        Remove-Item $out -Force
    }
    if ($code -lt 200 -or $code -ge 300) { throw "VirusTotal did not accept ${Name}: $detail" }
    Write-Output ("https://www.virustotal.com/gui/file/" + $Hash + "?nocache=1")
}

Submit-VirusTotal $appExe "NuClip.exe" $appHash
if ($setupPath) { Submit-VirusTotal $setupPath "NuClipSetup.exe" $setupHash }

$sha256 = (Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLower()
Send-R2Object $Zip "NuClip-$Version.zip" "application/zip"
if ($setupPath) {
    Send-R2Object $setupPath "NuClipSetup.exe" "application/octet-stream"
} else {
    Write-Output "Left NuClipSetup.exe unchanged. The installer reads latest.json."
}

$bodyObject = [ordered]@{
    version = $Version
    notes = $Notes
    sha256 = $sha256
    published = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd")
}
if ($appHash) { $bodyObject.app = $appHash }
if ($setupHash) { $bodyObject.setup = $setupHash }
$body = $bodyObject | ConvertTo-Json -Compress
$manifestPath = Join-Path $env:TEMP ("nuclip-latest-" + $Version + ".json")
Set-Content -Path $manifestPath -Value $body -Encoding ascii
try {
    $response = & curl.exe -fS --http1.1 -X POST `
        -H "Authorization: Bearer $PublishToken" `
        -H "Content-Type: application/json" `
        --data-binary "@$manifestPath" `
        "$Site/api/latest"
    if ($LASTEXITCODE -ne 0) { throw "The website did not update latest.json." }
    $localFeed = Join-Path $PSScriptRoot "..\..\NuClipWeb\releases\latest.json"
    if (Test-Path (Split-Path $localFeed)) {
        [System.IO.File]::WriteAllText((Resolve-Path (Split-Path $localFeed)).Path + "\latest.json", $response.Trim() + "`n", (New-Object System.Text.UTF8Encoding $false))
    }
    Write-Output "latest.json now offers $Version"
    Write-Output "Scan links now follow this release."
    Write-Output $sha256
} finally {
    Remove-Item $manifestPath -Force -ErrorAction SilentlyContinue
}
