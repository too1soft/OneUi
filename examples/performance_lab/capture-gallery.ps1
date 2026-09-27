param([string]$Output=(Join-Path $PSScriptRoot 'results/gallery'))
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$cases=@(
    @{Name='form-light-wide'; Args='--view components --width 1320 --height 900'},
    @{Name='form-dark-narrow'; Args='--view components --editor-dark --compact --width 640 --height 800'},
    @{Name='table-dark-wide'; Args='--view components --editor-dark --compact --component-scene 1 --width 1320 --height 900'},
    @{Name='states-light-narrow'; Args='--view components --component-scene 2 --width 640 --height 800'},
    @{Name='editor-wide'; Args='--view editor --entry code --editor-light --width 1320 --height 900'},
    @{Name='editor-narrow'; Args='--view editor --entry template --editor-invalid --compact --width 640 --height 800'}
)
foreach($case in $cases){
    $image=Join-Path $Output ($case.Name+'.png')
    $process=Start-Process -FilePath $exe -ArgumentList ($case.Args+' --snapshot "'+$image+'" --snapshot-exit') -PassThru -WindowStyle Hidden -RedirectStandardOutput ($image+'.log') -RedirectStandardError ($image+'.err')
    if(!$process.WaitForExit(30000)){Stop-Process -Id $process.Id;throw ('Timed out: '+$case.Name)}
    if($process.ExitCode -ne 0){throw ('Capture failed: '+$case.Name)}
    if(!(Test-Path -LiteralPath $image)){throw ('Missing capture: '+$image)}
    Write-Host $image
}
