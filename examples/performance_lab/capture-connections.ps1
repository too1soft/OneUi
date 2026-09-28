param([string]$Output=(Join-Path $PSScriptRoot 'results/connections'))
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Run build.ps1 first.'}
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$cases=@(
    @{Name='detail-light-wide'; Args='--view details --entry code --width 1320 --height 900'},
    @{Name='detail-dark-narrow'; Args='--view details --entry template --editor-dark --compact --width 640 --height 800'},
    @{Name='list-light-narrow'; Args='--view connections --entry code --width 640 --height 800'},
    @{Name='editor-error-narrow'; Args='--view editor --entry template --editor-light --editor-invalid --width 640 --height 800'}
)
foreach($case in $cases){
    $image=Join-Path $Output ($case.Name+'.png')
    $process=Start-Process -FilePath $exe -ArgumentList ($case.Args+' --snapshot "'+$image+'" --snapshot-exit') -PassThru -WindowStyle Hidden -RedirectStandardOutput ($image+'.log') -RedirectStandardError ($image+'.err')
    try {
        if(!$process.WaitForExit(30000)){throw ('Timed out: '+$case.Name)}
        if($process.ExitCode -ne 0){throw ('Capture failed: '+$case.Name)}
        if(!(Test-Path -LiteralPath $image)){throw ('Missing capture: '+$image)}
        if((Get-Content -LiteralPath ($image+'.layout.txt') -Raw).Trim() -ne 'No layout issues.'){throw ('Layout failed: '+$case.Name)}
        Write-Host $image
    } finally {if(!$process.HasExited){Stop-Process -Id $process.Id}}
}
