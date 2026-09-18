param([Parameter(Mandatory)][string]$ConfigurationFile)
$ErrorActionPreference = 'Stop'
if (Get-Process CodexBeerWidget -ErrorAction SilentlyContinue) { throw 'Close the widget before changing sync configuration' }
$config = Get-Content -Raw -LiteralPath $ConfigurationFile | ConvertFrom-Json
$origin = [Uri]$config.origin
if ($origin.Scheme -ne 'https' -or $origin.UserInfo -or $origin.Query -or $origin.Fragment -or $origin.AbsolutePath -ne '/') {
    throw 'An HTTPS origin without credentials, query or path is required'
}
if ($config.deviceId -cnotmatch '^[A-Za-z0-9_-]{16,128}$' -or $config.publisherSecret -cnotmatch '^[A-Za-z0-9_-]{43,128}$') {
    throw 'Invalid device credentials'
}
$directory = Join-Path $env:LOCALAPPDATA 'CodexBeerWidget'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$destination = Join-Path $directory 'sync.dpapi'
if (Test-Path -LiteralPath $destination) { throw 'Existing pairing must be revoked before replacing sync.dpapi; never reset its revision' }
Add-Type -AssemblyName System.Security
$data = @{origin=$origin.GetLeftPart([UriPartial]::Authority);deviceId=$config.deviceId;publisherSecret=$config.publisherSecret;revision=0} | ConvertTo-Json -Compress
$bytes = [Text.Encoding]::UTF8.GetBytes($data)
try {
    $encrypted = [Security.Cryptography.ProtectedData]::Protect($bytes, $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
    $file = [IO.File]::Open($destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $file.Write($encrypted, 0, $encrypted.Length); $file.Flush($true) } finally { $file.Dispose() }
} finally { [Array]::Clear($bytes, 0, $bytes.Length) }
Write-Output 'Sync configured for this Windows user. Restart the widget. Keep the source configuration out of version control.'
