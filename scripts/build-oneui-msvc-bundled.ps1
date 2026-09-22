param(
    [string]$VsInstall = "D:\Program Files\Microsoft Visual Studio\18\Community",
    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "RelWithDebInfo",
    [string]$BuildDir = "",
    [string]$SkiaOut = "",
    [ValidateSet("win7", "win10")]
    [string]$MinimumWindows = "win10",
    [switch]$TestFrameCapture
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "windows-build-profile.ps1")
$windowsBuildProfile = Get-OneUIWindowsBuildProfile -MinimumWindows $MinimumWindows

$root = Split-Path -Parent $PSScriptRoot
$vcvars = Join-Path $VsInstall "VC\Auxiliary\Build\vcvarsall.bat"
$cmake = Join-Path $VsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = Join-Path $VsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

if ($BuildDir -eq "") {
    $BuildDir = if ($Arch -eq "x86") { "build/msvc-bundled-static-x86" } else { "build/msvc-bundled-static" }
    if ($MinimumWindows -eq 'win7') { $BuildDir += '-win7' }
}
if ($SkiaOut -eq "") {
    $SkiaOut = if ($Arch -eq "x86") { "third_party/skia/out/oneui-win-x86-release" } else { "third_party/skia/out/oneui-win-x64-release" }
    if ($MinimumWindows -eq 'win7') { $SkiaOut += '-win7' }
}

$buildPath = Resolve-OneUIBuildPath $BuildDir
$skiaOutPath = Resolve-OneUIBuildPath $SkiaOut

if (!(Test-Path $vcvars)) {
    throw "vcvarsall.bat not found: $vcvars"
}
if (!(Test-Path $cmake)) {
    throw "cmake not found: $cmake"
}
if (!(Test-Path $ninja)) {
    throw "ninja not found: $ninja"
}
if (!(Test-Path $skiaOutPath)) {
    throw "Skia build output not found: $skiaOutPath"
}
if ($MinimumWindows -eq 'win7') {
    $recipe = Get-Content -LiteralPath (Join-Path $skiaOutPath 'args.gn') -Raw
    $expectedCpu = if ($Arch -eq 'x86') { 'x86' } else { 'x64' }
    foreach ($entry in @(@('target_cpu', $expectedCpu), @('win_toolchain_version', $windowsBuildProfile.toolsetVersion), @('win_sdk_version', $windowsBuildProfile.windowsSdkVersion))) {
        $pattern = '(?m)^\s*' + [regex]::Escape($entry[0]) + '\s*=\s*"' + [regex]::Escape($entry[1]) + '"\s*$'
        if ($recipe -notmatch $pattern) { throw "Skia recipe does not match the Win7 profile ($($entry[0])=$($entry[1])). Rebuild Skia with -MinimumWindows win7 in a separate output directory." }
    }
    foreach ($setting in @('skia_use_freetype', 'skia_enable_fontmgr_custom_empty')) {
        if ($recipe -notmatch ('(?m)^\s*' + $setting + '\s*=\s*true\s*$')) {
            throw "Win7 Skia recipe is missing $setting; rebuild the complete Win7 text dependency closure."
        }
    }
    if ($recipe -notmatch '(?m)^\s*cc_wrapper\s*=\s*"[^\r\n]*msvc-includes-wrapper\.py[^\r\n]*"\s*$') {
        throw 'Win7 Skia requires the localized MSVC dependency-tracking wrapper; rebuild with build-skia-static.ps1.'
    }
}

$vcArguments = $Arch
if ($windowsBuildProfile.toolsetVersion) {
    $compiler = Join-Path $VsInstall "VC/Tools/MSVC/$($windowsBuildProfile.toolsetVersion)/bin/Hostx64/$Arch/cl.exe"
    if (!(Test-Path -LiteralPath $compiler)) { throw "Required Win7 toolset is missing: $compiler. No automatic fallback to a newer toolset is allowed." }
    $vcArguments += " $($windowsBuildProfile.windowsSdkVersion) -vcvars_ver=$($windowsBuildProfile.toolsetVersion)"
}
$configure = "call `"$vcvars`" $vcArguments && `"$cmake`" -S `"$root`" -B `"$buildPath`" -G Ninja -DCMAKE_BUILD_TYPE=$Configuration -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DONEUI_WINDOWS_BASELINE=$MinimumWindows -DONEUI_SKIA_MODE=bundled-static -DONEUI_BUNDLED_SKIA_ROOT=`"$root/third_party/skia`" -DONEUI_BUNDLED_SKIA_OUT=`"$skiaOutPath`""
$build = "call `"$vcvars`" $vcArguments && `"$cmake`" --build `"$buildPath`" --parallel 6"
$captureOption = if ($TestFrameCapture) { 'ON' } else { 'OFF' }
$configure += " -DONEUI_ENABLE_TEST_FRAME_CAPTURE=$captureOption"

cmd.exe /c $configure
if ($LASTEXITCODE -ne 0) {
    throw "MSVC bundled configure failed"
}

cmd.exe /c $build
if ($LASTEXITCODE -ne 0) {
    throw "MSVC bundled build failed"
}

Get-Item (Join-Path $buildPath "oneui.dll"), (Join-Path $buildPath "examples/gallery/oneui_gallery.exe") | Select-Object FullName, Length
