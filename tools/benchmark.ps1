param([int]$Seconds=30,[string[]]$Modes=@('hidden','static','normal','smooth','clickthrough','live'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$executable=Join-Path $root 'build/Release/CodexBeerWidget.exe'
if (!(Test-Path -LiteralPath $executable)) { throw 'Build Release first' }
if (Get-Process -Name CodexBeerWidget -ErrorAction SilentlyContinue) { throw 'Close the widget before benchmarking' }
foreach ($mode in $Modes) {
    if ($mode -notin @('hidden','static','normal','smooth','clickthrough','live')) { throw 'Unknown benchmark mode' }
    $started=Get-Date
    $process=Start-Process -FilePath $executable -ArgumentList "--benchmark=$mode --seconds=$Seconds" -WorkingDirectory $root -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(($Seconds+40)*1000)) { Stop-Process -Id $process.Id; throw "Benchmark timed out: $mode" }
    $file=Join-Path $root ".local/benchmark-$mode.json"
    if (!(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTime -lt $started) { throw "Benchmark produced no new report: $mode" }
    $report=Get-Content -Raw -LiteralPath $file | ConvertFrom-Json
    [pscustomobject]@{Mode=$mode;CPU=[math]::Round($report.averageCpuOneCorePercent,3);WorkingMiB=[math]::Round($report.after.workingSetMiB,2);PrivateMiB=[math]::Round($report.after.privateMiB,2);FPS=[math]::Round($report.framesPerSecond,2);FirstFrameMs=[math]::Round($report.firstFrameMs,1)} | Format-Table
}
