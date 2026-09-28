param(
    [ValidateRange(1,10)][int]$Rounds=3,
    [ValidateRange(1,60)][int]$Seconds=5,
    [ValidateSet('code','template')][string]$Entry='code',
    [string]$Output=''
)
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build the performance lab first.'}
if(Get-Process oneui-performance-lab,gpui-performance-lab -ErrorAction SilentlyContinue){throw 'Close all performance lab windows before measuring.'}
if(!$Output){$Output=Join-Path $PSScriptRoot ('results/shadows-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$metadata=[ordered]@{
    date=(Get-Date).ToString('o'); rounds=$Rounds; sampleSeconds=$Seconds; warmupSeconds=3
    width=1320; height=1000; entry=$Entry; dev=$false; trace=$false
    cpu=(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfLogicalProcessors)
    os=(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version)
    adapters=(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    binaries=(Get-FileHash -Algorithm SHA256 -LiteralPath $exe,(Join-Path (Split-Path $exe) 'oneui.dll') | Select-Object @{n='name';e={Split-Path $_.Path -Leaf}},Hash)
    method='Same lab binary: ONEUI_SHADOW_REFERENCE=1 selects old full-size masks; unset selects adaptive nine-patch masks. Alternating mode-first rounds. Three-second warmup. Alternating CPU/GPU-first rounds. Effects toggles Reveal every 500ms and alternates expand/fade every four seconds, with native button hover; effects-idle exercises the same page for the first 2.1 seconds, then settles before sampling; only boundary posts occur in its sample. No per-frame tree rebuild or stylesheet parsing. CPU wall times, not GPU timestamps or display FPS; paint includes submission. Idle count must be zero. One failed idle sample may be retried once; all rejected raw runs remain on disk and accepted rows record attempt=1. Repeated failure aborts.'
}
$metadata | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Output 'environment.json') -Encoding utf8
$results=@()
$shadowBefore=$env:ONEUI_SHADOW_REFERENCE
$traceBefore=$env:ONEUI_RENDER_TRACE
try {
    $env:ONEUI_RENDER_TRACE='0'
    for($round=1;$round -le $Rounds;++$round){
        $order=if($round%2){@('cpu','gpu')}else{@('gpu','cpu')}
        $modes=if($round%2){@('reference','optimized')}else{@('optimized','reference')}
        foreach($mode in $modes){
        if($mode -eq 'reference'){$env:ONEUI_SHADOW_REFERENCE='1'}else{Remove-Item Env:ONEUI_SHADOW_REFERENCE -ErrorAction SilentlyContinue}
        foreach($scene in @('effects','effects-idle')){
            foreach($renderer in $order){
                for($attempt=0;$attempt -lt 2;++$attempt){
                $dir=Join-Path $Output "$round-$mode-$scene-$renderer"
                if($attempt){$dir+="-retry"}
                New-Item -ItemType Directory -Force -Path $dir | Out-Null
                $arguments="--renderer $renderer --renderer-benchmark $scene --sample-seconds $Seconds --width 1320 --height 1000 --entry $Entry --output `"$dir`""
                $p=Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput "$dir/runtime.log" -RedirectStandardError "$dir/runtime.err"
                if(!$p.WaitForExit(($Seconds+20)*1000)){Stop-Process -Id $p.Id;throw "Timed out: $round-$mode-$scene-$renderer"}
                if($p.ExitCode){throw "Run failed: $round-$mode-$scene-$renderer ($($p.ExitCode))"}
                $row=[ordered]@{round=$round;entry=$Entry;shadow=$mode}
                foreach($line in Get-Content -LiteralPath "$dir/renderer-performance.txt"){
                    if($line -match '^([^=]+)=(.*)$'){$row[$Matches[1]]=$Matches[2]}
                }
                $expected=if($renderer -eq 'gpu'){'opengl'}else{'raster'}
                if($row.backend -ne $expected -or $row.backend_changes -ne '0'){throw "Backend mismatch/fallback in $dir; comparison rejected. Inspect renderer-performance.txt."}
                if($row.width -ne '1320' -or $row.height -ne '1000'){throw "Window geometry mismatch in $dir"}
                if($row.mesh_draws_sample -ne '0'){throw "Unexpected particle scene in $dir; sample rejected."}
                if($scene -eq 'effects-idle' -and $row.paint_count -ne '0'){
                    if($attempt -eq 0){Write-Host "Rejected idle sample ($($row.paint_count) paints), raw record kept: $dir";continue}
                    throw "Idle repaint gate failed twice: $dir"
                }
                $row['attempt']=$attempt
                $results += [pscustomobject]$row
                $results | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $Output 'summary.csv')
                Write-Host "Finished $round-$mode-$scene-$renderer"
                break
                }
            }
        }
    }
    }
    if(@($results.dpi_scale | Select-Object -Unique).Count -ne 1){throw 'DPI changed during measurement; comparison rejected.'}
    $results | Format-Table round,shadow,scene,requested,backend,cpu_percent,paint_mean_ms,submit_mean_ms,blit_mean_ms,working_set_mib
    Write-Host "Results: $Output"
} finally {
    if($null -eq $shadowBefore){Remove-Item Env:ONEUI_SHADOW_REFERENCE -ErrorAction SilentlyContinue}else{$env:ONEUI_SHADOW_REFERENCE=$shadowBefore}
    if($null -eq $traceBefore){Remove-Item Env:ONEUI_RENDER_TRACE -ErrorAction SilentlyContinue}else{$env:ONEUI_RENDER_TRACE=$traceBefore}
}
