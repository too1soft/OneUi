param([ValidateRange(1, 10)][int]$Rounds = 3, [ValidateRange(3, 60)][int]$Seconds = 5)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Run examples/performance_lab/build.ps1 first.' }
if (Get-Process oneui-performance-lab,gpui-performance-lab -ErrorAction SilentlyContinue) {
    throw 'Close performance lab windows before measuring.'
}
$output = Join-Path $PSScriptRoot ('results/editor-benchmark-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
Get-FileHash -Algorithm SHA256 -LiteralPath $exe,(Join-Path (Split-Path $exe) 'oneui.dll') |
    Format-List | Out-File (Join-Path $output 'binaries.txt')
"Same Release executable, medium overview, 1320x900, alternating order, 3s warmup, ${Seconds}s capture. CPU is percent of total machine; working set in MiB. warm opens/layouts/hides the editor before warmup. No screenshots during timing." |
    Set-Content (Join-Path $output 'method.txt')
function Run-Lab([string]$Name, [string[]]$Options) {
    $dir = Join-Path $output $Name
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $labArgs = $Options + @('--output', ('"' + $dir + '"'))
    $p = Start-Process -FilePath $exe -ArgumentList $labArgs -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $dir 'runtime.log') -RedirectStandardError (Join-Path $dir 'runtime.err')
    if (!$p.WaitForExit(90000)) { Stop-Process -Id $p.Id; throw "$Name timed out." }
    if ($p.ExitCode -ne 0) { throw "$Name failed: $($p.ExitCode)" }
    return $dir
}
Run-Lab 'idle' @('--view','editor','--editor-idle-seconds','5') | Out-Null
$rows = @()
for ($round = 1; $round -le $Rounds; ++$round) {
    $order = if ($round % 2) { @('cold','warm') } else { @('warm','cold') }
    foreach ($mode in $order) {
        $labArgs = @('--load','medium','--benchmark-seconds',"$Seconds",'--exit-after-benchmark')
        if ($mode -eq 'warm') { $labArgs += '--warm-editor' }
        $dir = Run-Lab "$round-$mode" $labArgs
        $summary = Get-ChildItem -LiteralPath $dir -Filter 'oneui-*.txt' | Select-Object -First 1
        if (!$summary) { throw "Missing result: $dir" }
        $values = @{}
        foreach ($line in Get-Content -LiteralPath $summary.FullName) {
            if ($line -match '^([^=]+)=(.*)$') { $values[$Matches[1]] = $Matches[2] }
        }
        $rows += [pscustomobject]@{ round=$round; mode=$mode; mean_ms=$values.mean_ms; p95_ms=$values.p95_ms; cpu_percent=$values.mean_process_cpu_percent_total_machine; peak_working_set_mb=$values.peak_working_set_mb }
        Write-Host "Finished $round-$mode"
    }
}
$rows | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $output 'summary.csv')
$rows | Format-Table
Write-Host "Results: $output"
