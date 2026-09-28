param([string]$Output = (Join-Path $PSScriptRoot 'results/effects-gallery'))
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'build-current/bin/oneui-performance-lab.exe'
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
foreach($theme in 'light','dark') {
    foreach($size in @(@(1320,1000,1),@(640,1000,1),@(640,1000,1.25),@(640,1000,1.5))) {
        $name="effects-$theme-$($size[0])-$($size[2]).png"
        $out=Join-Path $Output $name
        $arguments="--view components --component-scene 3 --renderer cpu --width $($size[0]) --height $($size[1]) --scale $($size[2]) --editor-$theme --snapshot `"$out`" --snapshot-exit"
        $p=Start-Process -FilePath $exe -ArgumentList $arguments -PassThru -WindowStyle Hidden
        if(!$p.WaitForExit(15000)){Stop-Process -Id $p.Id;throw "Capture timed out: $name"}
        if($p.ExitCode -ne 0 -or !(Test-Path $out)){throw "Capture failed: $name"}
    }
}
Write-Host "Native raster client captures (content scale, not monitor DPI): $Output"
