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
$targets = 'oneui-performance-lab'
if ($Test) { $targets += ' oneui-editor-tests oneui-connections-tests oneui-authoring-tests oneui-gallery-tests oneui-renderer-tests oneui-particle-tests oneui-lifecycle-tests oneui-effects-tests oneui-motion-tests oneui-devtools-tests oneui-lab-process-tests oneui-lab-terminal-tests oneui-lab-gesture-tests oneui-lab-workspace-tests oneui-lab-remote-tests oneui-lab-product-tests oneui-lab-media-tests oneui-lab-popup-tests' }
cmd.exe /c "call `"$vcvars`" x64 && `"$cmake`" --build `"$buildDir`" --target $targets --parallel 8"
if ($LASTEXITCODE -ne 0) { throw 'OneUI build failed' }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-editor-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Editor tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-connections-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Connections tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-connections-tests.exe') --template; if ($LASTEXITCODE -ne 0) { throw 'Template workflow tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-authoring-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Authoring tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-gallery-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Gallery tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-terminal-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Terminal workspace tests failed' } }
if ($Test) {
    & (Join-Path $buildDir 'bin/oneui-lab-process-tests.exe') (Join-Path $buildDir 'process-tests') (Join-Path $buildDir 'bin/oneui-performance-lab.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark subprocess tests failed' }
}
if ($Test) { & (Join-Path $buildDir 'bin/oneui-renderer-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Renderer tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-particle-tests.exe') --cpu (Join-Path $buildDir 'particle-tests'); if ($LASTEXITCODE -ne 0) { throw 'Particle tests failed' } }


if ($Test) { & (Join-Path $buildDir 'bin/oneui-lifecycle-tests.exe') cpu; if ($LASTEXITCODE -ne 0) { throw 'Software lifecycle tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-lifecycle-tests.exe') failed-gpu; if ($LASTEXITCODE -ne 0) { throw 'GPU initialization fallback tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-effects-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Effects tests failed' } }
if ($Test) { & (Join-Path $buildDir 'bin/oneui-motion-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Motion continuity tests failed' } }

if ($Run) { Start-Process -FilePath (Join-Path $buildDir 'bin/oneui-performance-lab.exe') -WorkingDirectory $PSScriptRoot -WindowStyle Hidden }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-devtools-tests.exe') cpu (Join-Path $buildDir 'devtools-report.json'); if ($LASTEXITCODE -ne 0) { throw 'Built-in developer tools tests failed' } }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-gesture-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Shared dock gesture tests failed' } }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-workspace-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Workspace lifecycle tests failed' } }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-remote-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Remote frame and input tests failed' } }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-product-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Product composition tests failed' } }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-media-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Media tests failed' } }

if ($Test) { & (Join-Path $buildDir 'bin/oneui-lab-popup-tests.exe'); if ($LASTEXITCODE -ne 0) { throw 'Popup tests failed' } }
