param([ValidateRange(1,10)][int]$Rounds=3,[ValidateRange(100,3000)][int]$Cycles=200)
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if(Get-Process oneui-performance-lab,gpui-performance-lab -ErrorAction SilentlyContinue){throw 'Close performance labs before measuring.'}
$output=Join-Path $PSScriptRoot ('results/entries-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
Get-FileHash -Algorithm SHA256 -LiteralPath $exe,(Join-Path (Split-Path $exe) 'oneui.dll') | Format-List | Out-File (Join-Path $output 'binaries.txt')
"Same Release exe/DLL, 1320x900, 1000 seeded records, dev watcher OFF. First 50 cycles warm up; then measure filtering, editing, unsaved confirmation and discard. Paint is CPU callback work, not GPU time. Entry order alternates. Other desktop apps can affect timings." | Set-Content (Join-Path $output 'method.txt')
function Run-Lab([string]$Name,[string[]]$Options){
    $dir=Join-Path $output $Name;New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $labArgs=@('--view','connections','--output',('"'+$dir+'"'))+$Options
    $p=Start-Process -FilePath $exe -ArgumentList $labArgs -PassThru -WindowStyle Hidden -RedirectStandardOutput "$dir/runtime.log" -RedirectStandardError "$dir/runtime.err"
    if(!$p.WaitForExit(300000)){Stop-Process -Id $p.Id;throw "$Name timed out"}
    if($p.ExitCode){throw "$Name failed: $($p.ExitCode)"}
    return $dir
}
foreach($entry in @('code','template')){Run-Lab "idle-$entry" @('--entry',$entry,'--editor-idle-seconds','5') | Out-Null}
$results=@()
for($round=1;$round -le $Rounds;++$round){
    $order=if($round%2){@('code','template')}else{@('template','code')}
    foreach($entry in $order){
        $dir=Run-Lab "$round-$entry" @('--entry',$entry,'--connections-stress',"$Cycles")
        $row=[ordered]@{round=$round}
        foreach($line in Get-Content "$dir/entry-performance.txt"){if($line -match '^([^=]+)=(.*)$'){$row[$Matches[1]]=$Matches[2]}}
        $results += [pscustomobject]$row
        Write-Host "Finished $round-$entry"
    }
}
$results | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $output 'summary.csv')
$results | Format-Table round,entry,cpu_percent,paint_mean_ms,paint_p95_ms,working_set_mib
Write-Host "Results: $output"
