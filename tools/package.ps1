param([string]$BuildDirectory='build',[string]$OutputDirectory='dist',[string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=(Resolve-Path -LiteralPath (Join-Path $root $BuildDirectory)).Path
$output=[IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
if ($output -eq $root -or !$output.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Package output must be a subdirectory of the repository' }
$cmake=(Get-Command cmake -ErrorAction SilentlyContinue).Source
if (!$cmake) { $cmake='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
if (!(Test-Path -LiteralPath $cmake)) { throw 'CMake is required to package the build' }
$cpack=Join-Path (Split-Path $cmake -Parent) 'cpack.exe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& $cpack --config (Join-Path $build 'CPackConfig.cmake') -C $Configuration -B $output
if ($LASTEXITCODE -ne 0) { throw 'CPack failed' }
Copy-Item -LiteralPath (Join-Path $build "$Configuration/CodexBeerWidget.exe") -Destination (Join-Path $output 'CodexBeerWidget.exe')
$zip=Get-ChildItem -LiteralPath $output -Filter 'CodexBeerWidget-*-windows-x64.zip' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$zip) { throw 'Package ZIP not found' }
$lines=@($zip.FullName,(Join-Path $output 'CodexBeerWidget.exe')) | ForEach-Object { $hash=Get-FileHash -LiteralPath $_ -Algorithm SHA256; "$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_))" }
$lines | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
Get-Item -LiteralPath @($zip.FullName,(Join-Path $output 'CodexBeerWidget.exe'),(Join-Path $output 'SHA256SUMS.txt')) | Select-Object Name,Length
