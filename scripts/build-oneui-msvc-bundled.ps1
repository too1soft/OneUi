param(
    [string]$VsInstall = "D:\Program Files\Microsoft Visual Studio\18\Community",
    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",
    [string]$BuildDir = "",
    [string]$SkiaOut = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$vcvars = Join-Path $VsInstall "VC\Auxiliary\Build\vcvarsall.bat"
$cmake = Join-Path $VsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = Join-Path $VsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

if ($BuildDir -eq "") {
    $BuildDir = if ($Arch -eq "x86") { "build/msvc-bundled-static-x86" } else { "build/msvc-bundled-static" }
}
if ($SkiaOut -eq "") {
    $SkiaOut = if ($Arch -eq "x86") { "third_party/skia/out/oneui-win-x86-release" } else { "third_party/skia/out/oneui-win-x64-release" }
}

$buildPath = Join-Path $root $BuildDir
$skiaOutPath = Join-Path $root $SkiaOut

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

$configure = "`"$vcvars`" $Arch && `"$cmake`" -S `"$root`" -B `"$buildPath`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DONEUI_SKIA_MODE=bundled-static -DONEUI_BUNDLED_SKIA_ROOT=`"$root/third_party/skia`" -DONEUI_BUNDLED_SKIA_OUT=`"$skiaOutPath`""
$build = "`"$vcvars`" $Arch && `"$cmake`" --build `"$buildPath`""

cmd.exe /c $configure
if ($LASTEXITCODE -ne 0) {
    throw "MSVC bundled configure failed"
}

cmd.exe /c $build
if ($LASTEXITCODE -ne 0) {
    throw "MSVC bundled build failed"
}

Get-Item (Join-Path $buildPath "oneui.dll"), (Join-Path $buildPath "examples/gallery/oneui_gallery.exe") | Select-Object FullName, Length
