param(
    [ValidateSet('cpu','gpu','both')][string]$Renderer='both',
    [ValidateRange(30,360)][int]$TargetHz=60,
    [string]$Output=(Join-Path $PSScriptRoot 'results/motion-check')
)
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-motion-tests.exe'
if(!(Test-Path $exe)){throw 'Run build.ps1 first.'}
if(Get-Process oneui-performance-lab -ErrorAction SilentlyContinue){throw 'Close the performance demo before the timing check.'}
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
@{
    captured_at=(Get-Date -Format o); target_hz=$TargetHz
    exe_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash
    sdk_sha256=(Get-FileHash (Join-Path (Split-Path $exe) 'oneui.dll') -Algorithm SHA256).Hash
    os=[Environment]::OSVersion.VersionString
    cpu=(Get-CimInstance Win32_Processor | Select-Object -ExpandProperty Name)
    gpu=(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name)
    note='Content scale exercises raster/layout scaling; this is not a physical monitor DPI or display-present measurement.'
} | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Output 'environment.json')
$backends=if($Renderer -eq 'both'){@('gpu','cpu')}else{@($Renderer)}
$results=@()
foreach($backend in $backends){
    foreach($case in @(@(1320,1000,120,1),@(1320,1000,220,1),@(640,1000,400,1),@(960,1200,220,1.25),@(960,1200,220,1.5))){
        $name="$backend-$($case[0])-$($case[2])-$($case[3])"
        $dir=Join-Path $Output $name
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        & $exe --check $backend $dir $case[0] $case[1] $case[2] $TargetHz $case[3] *> (Join-Path $dir 'runtime.log')
        $status=$LASTEXITCODE
        $row=[ordered]@{case=$name;exit=$status}
        $report=Join-Path $dir 'motion-report.txt'
        if(Test-Path $report){foreach($line in Get-Content $report){if($line -match '^([^=]+)=(.*)$'){$row[$Matches[1]]=$Matches[2]}}}
        $results += [pscustomobject]$row
        $results | Export-Csv -NoTypeInformation -LiteralPath (Join-Path $Output 'summary.csv')
        Write-Host "$name : exit=$status"
    }
}
$results | Format-Table case,exit,geometry_errors,reverse_steps,late_frames,stalls,max_interval_ms,max_geometry_error
if(@($results | Where-Object exit -ne 0).Count){throw "Motion regression detected. Inspect $Output/summary.csv and each motion-frames.csv; rejected results are retained."}
Write-Host 'Motion checks passed: native paint samples, continuity, cadence and unchanged control/subscription counts.'
