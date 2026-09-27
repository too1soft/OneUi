param([switch]$Run, [switch]$Test, [string]$OneUiRoot = (Join-Path $PSScriptRoot '../..'), [string]$SkiaRoot = '')
$ErrorActionPreference = 'Stop'
$vswhere = 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio with Desktop development with C++ and CMake tools.' }
$vs = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools were not found.' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$buildDir = Join-Path $PSScriptRoot 'build-current'
if (!$SkiaRoot) { $SkiaRoot = Join-Path $OneUiRoot 'third_party/skia' }
$skiaOut = Join-Path $skiaRoot 'out/oneui-win-x64-release'
if (!(Test-Path (Join-Path $skiaOut 'skia.lib'))) { throw 'Build Skia first: see the repository README / examples README.' }
$configure = "call `"$vcvars`" x64 && `"$cmake`" -S `"$PSScriptRoot`" -B `"$buildDir`" -G Ninja -DONEUI_SOURCE_ROOT=`"$OneUiRoot`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DONEUI_SKIA_MODE=bundled-static -DONEUI_BUNDLED_SKIA_ROOT=`"$skiaRoot`" -DONEUI_BUNDLED_SKIA_OUT=`"$skiaOut`""
cmd.exe /c $configure
if ($LASTEXITCODE -ne 0) { throw 'OneUI configure failed' }
cmd.exe /c "call `"$vcvars`" x64 && `"$cmake`" --build `"$buildDir`" --target oneui-performance-lab oneui-editor-tests oneui-connections-tests oneui-authoring-tests oneui-gallery-tests oneui-renderer-tests oneui-particle-tests --parallel 8"
if ($LASTEXITCODE -ne 0) { throw 'OneUI build failed' }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-editor-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Editor tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-connections-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Connections tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-connections-tests.exe') --template; if ($LASTEXITCODE -ne 0) { throw 'Template workflow tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-authoring-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Authoring tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-gallery-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Gallery tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-renderer-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Renderer tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-particle-tests.exe') --cpu (Join-Path $buildDir 'particle-tests'); if ($LASTEXITCODE -ne 0) { throw 'Particle tests failed' } }
if ($Run) { Start-Process -FilePath (Join-Path $buildDir 'bin/oneui-performance-lab.exe') -WorkingDirectory $PSScriptRoot -WindowStyle Hidden }
