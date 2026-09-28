param([string]$BuildDir=(Join-Path $PSScriptRoot 'build-current'))
$ErrorActionPreference='Stop'
$exe=Join-Path $BuildDir 'bin/oneui-terminal-workbench.exe'
if(!(Test-Path $exe)){throw 'Build the workbench first: ./build.ps1'}
$out=Join-Path $BuildDir 'acceptance'
New-Item -ItemType Directory -Force $out | Out-Null
$prior=$env:ONEUI_ENABLE_GPU
$results=@()
try {
    foreach($case in @(
        @{Name='gpu-light-wide';Gpu='1';Args=@()},
        @{Name='gpu-dark-narrow';Gpu='1';Args=@('--dark','--narrow')},
        @{Name='cpu-light-wide';Gpu='0';Args=@()},
        @{Name='cpu-dark-narrow';Gpu='0';Args=@('--dark','--narrow')}
    )) {
        $env:ONEUI_ENABLE_GPU=$case.Gpu
        $testArgs=@('--test')+$case.Args
        $lines=& $exe @testArgs 2>&1
        $exitCode=$LASTEXITCODE
        $lines | Set-Content -Encoding utf8 (Join-Path $out ($case.Name+'.txt'))
        $lines | Write-Output
        $results+=@{case=$case.Name;exitCode=$exitCode;passed=($exitCode -eq 0)}
        if($exitCode -ne 0){throw "$($case.Name) failed"}
    }
    $env:ONEUI_ENABLE_GPU='1'
    & $exe --capture (Join-Path $out 'light-wide.png')
    if($LASTEXITCODE -ne 0){throw 'Light capture failed'}
    & $exe --dark --narrow --capture (Join-Path $out 'dark-narrow.png')
    if($LASTEXITCODE -ne 0){throw 'Dark capture failed'}
    foreach($scene in @('wide','split','files','processes','collapsed')) {
        $sceneArgs=@('--dark','--capture',(Join-Path $out ($scene+'.png')))
        if($scene -ne 'wide') { $sceneArgs+=@('--scene',$scene) }
        if($scene -eq 'files') { $sceneArgs+='--narrow' }
        & $exe @sceneArgs
        if($LASTEXITCODE -ne 0){throw "Scene capture failed: $scene"}
    }
    $hashes=@{}
    foreach($name in @('oneui-terminal-workbench.exe','oneui.dll','oneui_terminal_demo_owner.dll','oneui-viewc.exe')) {
        $hashes[$name]=(Get-FileHash -Algorithm SHA256 (Join-Path $BuildDir ('bin/'+$name))).Hash
    }
    @{date=(Get-Date -Format o);platform='Windows native';cases=$results;sha256=$hashes;
      scope='Native WM_CHAR/mouse route, synthetic composition, resize and lifetime; not OS IME candidate-window or cross-monitor DPI acceptance.'} |
      ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 (Join-Path $out 'result.json')
} finally { $env:ONEUI_ENABLE_GPU=$prior }
