param(
    [ValidateSet("mingw64", "ucrt64", "msvc-bundled-static", "msvc-bundled-static-x86")]
    [string]$Toolchain = "mingw64"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$toolchainBin = if ($Toolchain.StartsWith("msvc-bundled-static")) { "C:\msys64\mingw64\bin" } else { "C:\msys64\$Toolchain\bin" }
$objdump = Join-Path $toolchainBin "objdump.exe"
$build = Join-Path $root "build\$Toolchain"
$sdkRoot = Join-Path $root "dist\OneUI-SDK-$Toolchain"
$zip = Join-Path $root "dist\OneUI-SDK-$Toolchain.zip"

$oneuiDll = Join-Path $build "oneui.dll"
$oneuiImportLib = if ($Toolchain.StartsWith("msvc-bundled-static")) { Join-Path $build "oneui.lib" } else { Join-Path $build "liboneui.dll.a" }
$galleryExe = Join-Path $build "examples\gallery\oneui_gallery.exe"

foreach ($path in @($oneuiDll, $oneuiImportLib, $galleryExe)) {
    if (!(Test-Path $path)) {
        throw "Required build artifact not found: $path"
    }
}

if (!(Test-Path $objdump)) {
    throw "objdump not found: $objdump"
}

if (Test-Path $sdkRoot) {
    Remove-Item -LiteralPath $sdkRoot -Recurse -Force
}
if (Test-Path $zip) {
    Remove-Item -LiteralPath $zip -Force
}

New-Item -ItemType Directory -Force -Path `
    (Join-Path $sdkRoot "bin"), `
    (Join-Path $sdkRoot "lib"), `
    (Join-Path $sdkRoot "include"), `
    (Join-Path $sdkRoot "cmake"), `
    (Join-Path $sdkRoot "examples\gallery"), `
    (Join-Path $sdkRoot "docs"), `
    (Join-Path $sdkRoot "website") | Out-Null

Copy-Item -LiteralPath $oneuiDll -Destination (Join-Path $sdkRoot "bin")
Copy-Item -LiteralPath $oneuiImportLib -Destination (Join-Path $sdkRoot "lib")
Copy-Item -LiteralPath $galleryExe -Destination (Join-Path $sdkRoot "examples\gallery")
Copy-Item -LiteralPath $oneuiDll -Destination (Join-Path $sdkRoot "examples\gallery")
Copy-Item -Path (Join-Path $root "include\*") -Destination (Join-Path $sdkRoot "include") -Recurse
Copy-Item -Path (Join-Path $root "cmake\*") -Destination (Join-Path $sdkRoot "cmake") -Recurse
Copy-Item -Path (Join-Path $root "docs\*.md") -Destination (Join-Path $sdkRoot "docs")
if (Test-Path (Join-Path $root "website")) {
    Copy-Item -Path (Join-Path $root "website\*") -Destination (Join-Path $sdkRoot "website") -Recurse
}

$systemDlls = @(
    "advapi32.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "dwrite.dll",
    "gdi32.dll",
    "kernel32.dll",
    "msvcrt.dll",
    "ole32.dll",
    "oleaut32.dll",
    "opengl32.dll",
    "rpcrt4.dll",
    "shell32.dll",
    "shlwapi.dll",
    "user32.dll",
    "usp10.dll",
    "uuid.dll",
    "winmm.dll",
    "winspool.drv",
    "ws2_32.dll"
)

function Get-ImportedDlls($binary) {
    & $objdump -p $binary |
        Select-String "DLL Name:\s*(.+)$" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Is-SystemDll($name) {
    $lower = $name.ToLowerInvariant()
    return $systemDlls -contains $lower -or $lower.StartsWith("api-ms-win-") -or $lower.StartsWith("ext-ms-win-")
}

$seen = @{}
$missing = New-Object System.Collections.Generic.List[string]
$missingDetails = New-Object System.Collections.Generic.List[string]
$queue = New-Object System.Collections.Generic.Queue[string]
$queue.Enqueue((Join-Path $sdkRoot "bin\oneui.dll"))
$queue.Enqueue((Join-Path $sdkRoot "examples\gallery\oneui_gallery.exe"))

while ($queue.Count -gt 0) {
    $current = $queue.Dequeue()
    foreach ($dll in Get-ImportedDlls $current) {
        $key = $dll.ToLowerInvariant()
        if ($seen.ContainsKey($key) -or (Is-SystemDll $dll) -or $key -eq "oneui.dll") {
            continue
        }

        $seen[$key] = $true
        $target = Join-Path $sdkRoot "bin\$dll"
        if (Test-Path $target) {
            continue
        }

        $source = Join-Path $toolchainBin $dll
        if (!(Test-Path $source)) {
            $missing.Add($dll)
            $missingDetails.Add("$dll imported by $current")
            continue
        }

        Copy-Item -LiteralPath $source -Destination $target
        $queue.Enqueue($target)
    }
}

$missingDlls = $missing | Sort-Object -Unique
if ($missingDlls.Count -ne 0) {
    $detail = ($missingDetails | Sort-Object -Unique) -join "; "
    throw "Missing runtime DLLs for SDK package: $($missingDlls -join ', '). Imports: $detail"
}

$runGallery = @'
$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sdkRoot = Resolve-Path "$scriptDir\..\.."
$env:PATH = "$sdkRoot\bin;$env:PATH"
& "$scriptDir\oneui_gallery.exe"
'@

Set-Content -LiteralPath (Join-Path $sdkRoot "examples\gallery\run-gallery.ps1") -Value $runGallery -Encoding ASCII

$importLibName = Split-Path -Leaf $oneuiImportLib
$sdkKind = if ($Toolchain.StartsWith("msvc-bundled-static")) { "product SDK package" } else { "development SDK package" }
$dependencyNote = if ($Toolchain.StartsWith("msvc-bundled-static")) {
    "This package is built with vendored/static Skia and the static MSVC runtime. End users should not need to install MSYS2, Skia, or the Visual C++ runtime."
} else {
    "This package includes the current dynamic runtime DLLs in bin/ so SDK users do not need to configure Skia manually. It is not the final product package."
}

$readme = @"
OneUI SDK
=========

This is a $sdkKind.

Contents:
  bin/oneui.dll
  lib/$importLibName
  include/oneui/oneui.h
  include/oneui/**/*.h
  cmake/OneUIConfig.cmake
  examples/gallery/oneui_gallery.exe
  examples/gallery/oneui.dll
  docs/*.md
  website/index.html

CMake usage:
  set(CMAKE_PREFIX_PATH <path-to-this-sdk>)
  find_package(OneUI REQUIRED)
  target_link_libraries(my_app PRIVATE OneUI::oneui)

Note:
  $dependencyNote
"@

Set-Content -LiteralPath (Join-Path $sdkRoot "README.txt") -Value $readme -Encoding ASCII

Compress-Archive -Path (Join-Path $sdkRoot "*") -DestinationPath $zip

$sdkBytes = (Get-ChildItem -LiteralPath $sdkRoot -Recurse -File | Measure-Object -Property Length -Sum).Sum
$zipBytes = (Get-Item -LiteralPath $zip).Length

[PSCustomObject]@{
    Sdk = $sdkRoot
    Zip = $zip
    UncompressedBytes = $sdkBytes
    ZipBytes = $zipBytes
    MissingDlls = ($missingDlls -join ", ")
}
