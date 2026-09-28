param(
    [ValidateRange(5,3600)][int]$Seconds=1800,
    [ValidateSet('gpu','cpu')][string]$Renderer='gpu',
    [switch]$Exercise,
    [switch]$FailGpuInit,
    [string]$Output=''
)
$ErrorActionPreference='Stop'
if($Exercise -and $Seconds -lt 20){throw 'Exercise needs at least 20 seconds.'}
if($FailGpuInit -and $Renderer -ne 'gpu'){throw 'FailGpuInit requires the GPU request.'}
if(Get-Process oneui-performance-lab,gpui-performance-lab,oneui-particle-tests,oneui-lifecycle-tests -ErrorAction SilentlyContinue){throw 'Close lab and test windows first.'}
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build the performance lab first.'}
if(!$Output){$Output=Join-Path $PSScriptRoot ('results/stability-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath (Join-Path $Output 'stability.csv')){throw 'Choose a fresh output directory.'}
New-Item -ItemType Directory -Force -Path $Output | Out-Null
[ordered]@{
    date=(Get-Date).ToString('o');seconds=$Seconds;renderer=$Renderer;exercise=[bool]$Exercise;injectedGpuFailure=[bool]$FailGpuInit
    width=1320;height=900;load='heavy';particleMode='omitted (verify executable default)'
    cpu=(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfLogicalProcessors)
    os=(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version)
    adapters=(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    binaries=(Get-FileHash -Algorithm SHA256 -LiteralPath $exe,(Join-Path (Split-Path $exe) 'oneui.dll') | Select-Object @{n='name';e={Split-Path $_.Path -Leaf}},Hash)
    method='3-second warmup; stream 5-second interval aggregates; no per-frame sample vector. CPU wall times, not GPU execution or display FPS. Fault injection is compiled only in the lab renderer.'
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$Output/environment.json" -Encoding utf8
$savedTrace=$env:ONEUI_RENDER_TRACE;$savedFault=$env:ONEUI_LAB_FAIL_GPU_INIT
$p=$null
try {
    $env:ONEUI_RENDER_TRACE='0'
    if($FailGpuInit){$env:ONEUI_LAB_FAIL_GPU_INIT='1'}else{Remove-Item Env:ONEUI_LAB_FAIL_GPU_INIT -ErrorAction SilentlyContinue}
    $exerciseArg=if($Exercise){' --stability-exercise'}else{''}
    $p=Start-Process -FilePath $exe -ArgumentList "--renderer $Renderer --stability-seconds $Seconds --load heavy --output `"$Output`"$exerciseArg" -PassThru -WindowStyle Hidden -RedirectStandardOutput "$Output/runtime.log" -RedirectStandardError "$Output/runtime.err"
    $deadline=(Get-Date).AddSeconds($Seconds+30)
    while(!$p.WaitForExit(1000)) {
        if((Get-Date) -gt $deadline){Stop-Process -Id $p.Id;throw 'Stability run timed out.'}
    }
    if($p.ExitCode -ne 0){throw "Stability process failed: $($p.ExitCode)"}
    if(!(Test-Path -LiteralPath "$Output/stability-complete.txt")){throw 'Window closed before the stability run completed.'}
    $rows=@(Import-Csv -LiteralPath "$Output/stability.csv")
    if($rows.Count -eq 0 -or [double]$rows[-1].seconds -lt $Seconds){throw 'Stability sampling incomplete.'}
    $info=@{};foreach($line in Get-Content -LiteralPath "$Output/stability-info.txt") {
        if($line -match '^([^=]+)=(.*)$'){$info[$Matches[1]]=$Matches[2]}
    }
    if($info.particle_mode -ne 'mesh'){throw 'Executable did not default to mesh.'}
    $expected=if($Renderer -eq 'cpu' -or $FailGpuInit){'raster'}else{'opengl'}
    if($info.backend -ne $expected){throw 'Startup backend mismatch.'}
    if($FailGpuInit -and $info.reason -ne 'opengl-context-failed'){throw 'GPU fault injection did not reach initialization failure.'}
    foreach($row in $rows) {
        if($row.backend -ne $expected -or [long]$row.backend_changes -ne 0){throw 'Backend changed during sampling.'}
        if($expected -eq 'opengl' -and [long]$row.mesh_fallbacks -ne 0){throw 'GPU mesh unexpectedly fell back.'}
        if($expected -eq 'raster' -and [long]$row.mesh_draws -ne 0){throw 'Raster reported a GPU mesh submission.'}
        if(!$Exercise -and ([long]$row.paint_count -eq 0 -or $row.width -ne '1320' -or $row.height -ne '900')){throw 'Fixed-load window stopped drawing or changed size.'}
    }
    $totalDraws=($rows | Measure-Object mesh_draws -Sum).Sum
    $totalFallbacks=($rows | Measure-Object mesh_fallbacks -Sum).Sum
    if($expected -eq 'opengl' -and $totalDraws -le 0){throw 'No actual mesh submissions.'}
    if($expected -eq 'raster' -and $totalFallbacks -le 0){throw 'No software fallback draws.'}
    if($Exercise) {
        $steps=@(Import-Csv -LiteralPath "$Output/interactions.csv")
        if($steps.Count -lt 10 -or @($steps | Where-Object editing -eq '1').Count -eq 0 -or @($steps | Where-Object running -eq '0').Count -eq 0){throw 'Interaction sequence incomplete.'}
    }
    Write-Host "Completed $Seconds seconds; backend=$expected, mesh submissions=$totalDraws, software fallbacks=$totalFallbacks"
    Write-Host "Results: $Output (memory/performance trends require reviewing stability.csv)"
} finally {
    if($null -ne $p -and !$p.HasExited){Stop-Process -Id $p.Id -ErrorAction SilentlyContinue}
    if($null -eq $savedTrace){Remove-Item Env:ONEUI_RENDER_TRACE -ErrorAction SilentlyContinue}else{$env:ONEUI_RENDER_TRACE=$savedTrace}
    if($null -eq $savedFault){Remove-Item Env:ONEUI_LAB_FAIL_GPU_INIT -ErrorAction SilentlyContinue}else{$env:ONEUI_LAB_FAIL_GPU_INIT=$savedFault}
}
