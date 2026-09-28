param([switch]$Test, [string]$OneUiRoot = (Join-Path $PSScriptRoot '../..'), [string]$SkiaRoot = '')
$ErrorActionPreference = 'Stop'
$OneUiRoot = (Resolve-Path -LiteralPath $OneUiRoot).Path
$vswhere = 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio with Desktop development with C++ and CMake tools.' }
$vs = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools were not found.' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$buildDir = Join-Path $PSScriptRoot 'build'
if (!$SkiaRoot) { $SkiaRoot = Join-Path $OneUiRoot 'third_party/skia' }
$skiaOut = Join-Path $skiaRoot 'out/oneui-win-x64-release'
if (!(Test-Path (Join-Path $skiaOut 'skia.lib'))) { throw 'Build Skia first: see the repository README / examples README.' }
$sdkTests = if ($Test) { 'ON' } else { 'OFF' }
if ($Test) {
    & $cmake -P (Join-Path $OneUiRoot 'scripts/fetch-text-test-assets.cmake')
    if ($LASTEXITCODE -ne 0) { throw 'Test asset download or checksum verification failed' }
}
cmd.exe /c "call `"$vcvars`" x64 && `"$cmake`" -S `"$PSScriptRoot`" -B `"$buildDir`" -G Ninja -DONEUI_DEMO_SDK_TESTS=$sdkTests -DONEUI_SOURCE_ROOT=`"$OneUiRoot`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DONEUI_SKIA_MODE=bundled-static -DONEUI_BUNDLED_SKIA_ROOT=`"$skiaRoot`" -DONEUI_BUNDLED_SKIA_OUT=`"$skiaOut`""
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
$targets = 'oneui-hello oneui-details oneui-declarative-demo oneui-declarative-tests'
if ($Test) { $targets += ' oneui_reactive_lifetime_tests oneui_interaction_tests oneui_component_behavior_tests oneui_yoga_behavior_tests oneui_authoring_behavior_tests oneui_frame_profile_tests oneui_win32_accessibility_tests oneui_win32_window_loop_tests oneui_control_behavior_tests oneui_stack_behavior_tests oneui_scroll_view_behavior_tests' }
cmd.exe /c "call `"$vcvars`" x64 && `"$cmake`" --build `"$buildDir`" --target $targets --parallel 8"
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if ($Test) {
    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
    cmd.exe /c "call `"$vcvars`" x64 && `"$ctest`" --test-dir `"$buildDir`" --output-on-failure -R `"declarative_|oneui_(abi_sync|reactive_lifetime_tests|interaction_tests|component_behavior_tests|yoga_behavior_tests|authoring_behavior_tests|frame_profile_tests|win32_accessibility_tests|win32_window_loop_tests|control_behavior_tests|stack_behavior_tests|scroll_view_behavior_tests)`""
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
}
