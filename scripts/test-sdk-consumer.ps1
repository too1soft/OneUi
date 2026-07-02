param(
    [string]$SdkRoot = "dist/OneUI-SDK-msvc-bundled-static",
    [string]$VsInstall = "D:\Program Files\Microsoft Visual Studio\18\Community",
    [string]$BuildDir = "build/sdk-consumer",
    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$sdkPath = Join-Path $root $SdkRoot
$buildPath = Join-Path $root $BuildDir
$sourcePath = Join-Path $buildPath "src"
$vcvars = Join-Path $VsInstall "VC\Auxiliary\Build\vcvarsall.bat"
$cmake = "C:\msys64\mingw64\bin\cmake.exe"
$ninja = "C:\msys64\mingw64\bin\ninja.exe"

foreach ($path in @($sdkPath, $vcvars, $cmake, $ninja)) {
    if (!(Test-Path $path)) {
        throw "Required path not found: $path"
    }
}

if (Test-Path $buildPath) {
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $sourcePath | Out-Null

$sdkForCMake = (Resolve-Path $sdkPath).Path.Replace("\", "/")

$cmakeLists = @"
cmake_minimum_required(VERSION 3.16)
project(oneui_sdk_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OneUI REQUIRED CONFIG PATHS "$sdkForCMake/cmake" NO_DEFAULT_PATH)

add_executable(oneui_sdk_consumer main.cpp)
target_link_libraries(oneui_sdk_consumer PRIVATE OneUI::oneui)
target_compile_definitions(oneui_sdk_consumer PRIVATE UNICODE _UNICODE)
"@

$main = @'
#include "oneui/oneui.h"

#include <memory>

int main() {
    auto root = std::make_shared<oneui::View>();

    auto button = std::make_shared<oneui::Button>(L"SDK OK");
    button->setFrame(oneui::Rect{24.0f, 24.0f, 120.0f, 36.0f});
    root->add(button);

    auto input = std::make_shared<oneui::TextField>(L"External app");
    input->setFrame(oneui::Rect{24.0f, 76.0f, 180.0f, 36.0f});
    root->add(input);

    auto checkbox = std::make_shared<oneui::Checkbox>(L"Linked through OneUI::oneui");
    checkbox->setFrame(oneui::Rect{24.0f, 126.0f, 260.0f, 28.0f});
    root->add(checkbox);

    auto window = oneui::Window::create(L"OneUI SDK Consumer", 360, 220);
    window->setContent(root);
    return 0;
}
'@

Set-Content -LiteralPath (Join-Path $sourcePath "CMakeLists.txt") -Value $cmakeLists -Encoding ASCII
Set-Content -LiteralPath (Join-Path $sourcePath "main.cpp") -Value $main -Encoding ASCII

$configure = "`"$vcvars`" $Arch && `"$cmake`" -S `"$sourcePath`" -B `"$buildPath`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_MAKE_PROGRAM=`"$ninja`""
$build = "`"$vcvars`" $Arch && `"$cmake`" --build `"$buildPath`""

cmd.exe /c $configure
if ($LASTEXITCODE -ne 0) {
    throw "SDK consumer configure failed"
}

cmd.exe /c $build
if ($LASTEXITCODE -ne 0) {
    throw "SDK consumer build failed"
}

$consumerExe = Join-Path $buildPath "oneui_sdk_consumer.exe"

& (Join-Path $PSScriptRoot "audit-runtime.ps1") `
    -Binary $consumerExe `
    -Mode product `
    -Toolchain mingw64 `
    -AllowOneUI

if ($LASTEXITCODE -ne 0) {
    throw "SDK consumer runtime audit failed"
}

Get-Item $consumerExe | Select-Object FullName, Length
