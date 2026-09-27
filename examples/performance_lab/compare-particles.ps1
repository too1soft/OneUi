param(
    [ValidateRange(1,10)][int]$Rounds=5,
    [ValidateRange(1,60)][int]$Seconds=5,
    [ValidateSet('light','medium','heavy')][string]$Load='heavy',
    [ValidateNotNullOrEmpty()][ValidateSet('reference','precomputed','batch','combined','mesh')][string[]]$Modes=@('reference','precomputed','batch','combined'),
    [string]$Output=''
)
$ErrorActionPreference='Stop'
if(@($Modes | Select-Object -Unique).Count -ne $Modes.Count){throw 'Modes must not contain duplicates.'}
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build the performance lab first.'}
if(Get-Process oneui-performance-lab,gpui-performance-lab,oneui-particle-tests -ErrorAction SilentlyContinue){throw 'Close performance lab/test windows before measuring.'}
if(!$Output){$Output=Join-Path $PSScriptRoot ('results/particles-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
[ordered]@{
    date=(Get-Date).ToString('o'); rounds=$Rounds; sampleSeconds=$Seconds; warmupSeconds=3
    load=$Load; width=1320; height=900; entry='code'; dev=$false; trace=$false
    particleModes=$Modes
    cpu=(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfLogicalProcessors)
    os=(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version)
    adapters=(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    binaries=(Get-FileHash -Algorithm SHA256 -LiteralPath $exe,(Join-Path (Split-Path $exe) 'oneui.dll') | Select-Object @{n='name';e={Split-Path $_.Path -Leaf}},Hash)
    method='Same binaries/data/animation. Rotate selected particle modes each round and alternate renderer order. CPU wall times, not GPU execution/display FPS. Paint includes content/submit. Cache and process memory are different accounting domains. Each run is a fresh process. Mesh sample counters exclude warmup; total counters include it.'
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Output 'environment.json') -Encoding utf8
$results=@()
$traceBefore=$env:ONEUI_RENDER_TRACE
try {
    $env:ONEUI_RENDER_TRACE='0'
    for($round=1;$round -le $Rounds;++$round){
        $renderers=if($round%2){@('cpu','gpu')}else{@('gpu','cpu')}
        for($offset=0;$offset -lt $Modes.Count;++$offset){
            $mode=$Modes[($offset+$round-1)%$Modes.Count]
            foreach($renderer in $renderers){
                $dir=Join-Path $Output "$round-$mode-$renderer"
                New-Item -ItemType Directory -Force -Path $dir | Out-Null
                $arguments="--renderer $renderer --particle-mode $mode --renderer-benchmark particles --sample-seconds $Seconds --load $Load --width 1320 --height 900 --entry code --output `"$dir`""
                $p=Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden -RedirectStandardOutput "$dir/runtime.log" -RedirectStandardError "$dir/runtime.err"
                if(!$p.WaitForExit(($Seconds+20)*1000)){Stop-Process -Id $p.Id;throw "Timed out: $dir"}
                if($p.ExitCode){throw "Run failed: $dir ($($p.ExitCode))"}
                $row=[ordered]@{round=$round;load=$Load}
                foreach($line in Get-Content -LiteralPath "$dir/renderer-performance.txt"){
                    if($line -match '^([^=]+)=(.*)$'){$row[$Matches[1]]=$Matches[2]}
                }
                $expected=if($renderer -eq 'gpu'){'opengl'}else{'raster'}
                if($row.backend -ne $expected -or $row.backend_changes -ne '0' -or $row.particle_mode -ne $mode){throw "Backend or mode mismatch: $dir"}
                if($mode -eq 'mesh'){
                    if($renderer -eq 'gpu' -and ([long]$row.mesh_draws_sample -le 0 -or [long]$row.mesh_fallbacks_sample -ne 0)){throw "Mesh GPU path fell back during sampling: $dir"}
                    if($renderer -eq 'cpu' -and ([long]$row.mesh_draws_sample -ne 0 -or [long]$row.mesh_fallbacks_sample -le 0)){throw "Mesh CPU fallback missing: $dir"}
                }
                if($row.width -ne '1320' -or $row.height -ne '900'){throw "Geometry mismatch: $dir"}
                $results += [pscustomobject]$row
                $results | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $Output 'summary.csv')
                Write-Host "Finished $round-$mode-$renderer"
            }
        }
    }
    if(@($results.dpi_scale | Select-Object -Unique).Count -ne 1){throw 'DPI changed during measurement'}
    $results | Format-Table round,particle_mode,requested,paint_mean_ms,content_mean_ms,submit_mean_ms,working_set_mib
    Write-Host "Results: $Output"
} finally {
    if($null -eq $traceBefore){Remove-Item Env:ONEUI_RENDER_TRACE -ErrorAction SilentlyContinue}else{$env:ONEUI_RENDER_TRACE=$traceBefore}
}
