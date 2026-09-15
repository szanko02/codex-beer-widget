param([Parameter(Mandatory)][string]$Executable,[Parameter(Mandatory)][string]$Tag,
      [int]$Seconds=20,[int]$Repeats=2,[ValidateSet(2,5,10,30,60)][int]$RefreshSeconds=10,
      [string[]]$Modes=@('hidden','static','normal','clickthrough','hover-legacy','hover300','hover800','hover1200'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
if ($Tag -notmatch '^[a-z0-9-]+$') { throw 'Use a simple report tag' }
if (Get-Process CodexBeerWidget -ErrorAction SilentlyContinue) { throw 'Close the widget first' }
$output=Join-Path $root ".local/performance-$Tag"
New-Item -ItemType Directory -Force -Path $output | Out-Null
$reports=@()
for ($repeat=1;$repeat -le $Repeats;$repeat++) {
 foreach ($mode in $Modes) {
  if ($mode -notin @('hidden','static','normal','smooth','clickthrough','live','hover-legacy','hover300','hover800','hover1200')) { throw 'Unknown mode' }
  $started=Get-Date
  $process=Start-Process -FilePath $executablePath -ArgumentList "--benchmark=$mode --seconds=$Seconds --refresh=$RefreshSeconds" -WorkingDirectory $root -WindowStyle Hidden -PassThru
  if (!$process.WaitForExit(($Seconds+40)*1000)) { Stop-Process -Id $process.Id; throw "Timeout: $mode" }
  $file=Join-Path $root ".local/benchmark-$mode.json"
  if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTime -lt $started) { throw "No successful new report: $mode" }
  $report=Get-Content -Raw -LiteralPath $file | ConvertFrom-Json
  $report | Add-Member -NotePropertyName repeat -NotePropertyValue $repeat
  $reports += $report
  Copy-Item -LiteralPath $file -Destination (Join-Path $output "$mode-$repeat.json")
  Write-Output "$Tag $repeat/$Repeats $mode CPU=$([math]::Round($report.averageCpuOneCorePercent,3)) working=$([math]::Round($report.after.workingSetMiB,2)) updates=$($report.tooltipUpdates)"
 }
}
@{tag=$Tag;version=(Get-Item -LiteralPath $executablePath).VersionInfo.FileVersion;seconds=$Seconds;reports=$reports} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'summary.json')
