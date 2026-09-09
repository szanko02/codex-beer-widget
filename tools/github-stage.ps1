param(
    [Parameter(Mandatory)][ValidateSet('Open','Merge','Status')][string]$Action,
    [string]$Title,
    [string]$BodyFile,
    [int]$Number
)
$ErrorActionPreference = 'Stop'
$origin = git remote get-url origin
if ($origin -notmatch '^https://github.com/([^/]+/[^/]+?)(?:\.git)?$') { throw 'Expected GitHub HTTPS origin' }
$repository = $Matches[1]
$credentialLines = "protocol=https`nhost=github.com`n`n" | git credential fill
$credential = @{}
foreach ($line in $credentialLines) {
    $pair = $line -split '=', 2
    if ($pair.Count -eq 2) { $credential[$pair[0]] = $pair[1] }
}
if (!$credential['password']) { throw 'GitHub credential unavailable' }
$headers = @{ Authorization = 'Bearer ' + $credential['password']; Accept = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28' }
$api = "https://api.github.com/repos/$repository"
switch ($Action) {
    'Open' {
        if (!$Title -or !$BodyFile) { throw 'Title and BodyFile required' }
        $branch = git branch --show-current
        if ($branch -notlike 'codex/*') { throw 'Expected stage branch' }
        $body = @{ title = $Title; body = Get-Content -Raw -LiteralPath $BodyFile; head = $branch; base = 'develop' } | ConvertTo-Json
        Invoke-RestMethod -Method Post -Uri "$api/pulls" -Headers $headers -Body $body -ContentType 'application/json' | Select-Object number,html_url
    }
    'Merge' {
        if (!$Number) { throw 'Number required' }
        $pr = Invoke-RestMethod -Uri "$api/pulls/$Number" -Headers $headers
        if ($pr.base.ref -ne 'develop' -or $pr.head.ref -notlike 'codex/*') { throw 'Unexpected branch pair' }
        $checks = Invoke-RestMethod -Uri "$api/commits/$($pr.head.sha)/check-runs" -Headers $headers
        $bad = @($checks.check_runs | Where-Object { $_.status -ne 'completed' -or $_.conclusion -notin @('success','skipped','neutral') })
        if ($bad.Count) { throw 'Checks pending or failed' }
        $body = @{ merge_method = 'merge'; sha = $pr.head.sha } | ConvertTo-Json
        Invoke-RestMethod -Method Put -Uri "$api/pulls/$Number/merge" -Headers $headers -Body $body -ContentType 'application/json' | Select-Object merged,sha,message
    }
    'Status' {
        $branch = git rev-parse HEAD
        $checks = Invoke-RestMethod -Uri "$api/commits/$branch/check-runs" -Headers $headers
        $checks.check_runs | Select-Object name,status,conclusion,html_url
    }
}
