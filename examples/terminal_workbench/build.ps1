param([switch]$Run,[switch]$Test,[string]$SkiaRoot='')
$ErrorActionPreference='Stop'
$oneRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vswhere='C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C++ tools are required.'}
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$cmake=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$build=Join-Path $PSScriptRoot 'build-current'
if(!$SkiaRoot){$SkiaRoot=Join-Path $oneRoot 'third_party/skia'}
$skiaOut=Join-Path $SkiaRoot 'out/oneui-win-x64-release'
cmd.exe /c "call `"$vcvars`" x64 && `"$cmake`" -S `"$PSScriptRoot`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DONEUI_SKIA_MODE=bundled-static -DONEUI_BUNDLED_SKIA_ROOT=`"$SkiaRoot`" -DONEUI_BUNDLED_SKIA_OUT=`"$skiaOut`""
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
cmd.exe /c "call `"$vcvars`" x64 && `"$cmake`" --build `"$build`" --parallel 8"
if($LASTEXITCODE -ne 0){throw 'Build failed'}
if($Test){& (Join-Path $PSScriptRoot 'check.ps1') -BuildDir $build}
if($Run){Start-Process -FilePath (Join-Path $build 'bin/oneui-terminal-workbench.exe') -WorkingDirectory $PSScriptRoot -WindowStyle Hidden}
