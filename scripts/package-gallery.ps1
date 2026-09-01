param(
    [ValidateSet("mingw64", "ucrt64")]
    [string]$Toolchain = "mingw64"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "runtime-dependencies.ps1")

$root = Split-Path -Parent $PSScriptRoot
$toolchainRoot = "C:\msys64\$Toolchain"
$toolchainBin = Join-Path $toolchainRoot "bin"
$objdump = Join-Path $toolchainBin "objdump.exe"
$exe = Join-Path $root "build\$Toolchain\examples\gallery\oneui_gallery.exe"
$oneuiDll = Join-Path $root "build\$Toolchain\oneui.dll"
$distRoot = Join-Path $root "dist"
$dist = Join-Path $distRoot "oneui-gallery-$Toolchain"

if (!(Test-Path $exe)) {
    throw "Gallery executable not found: $exe"
}

if (!(Test-Path $oneuiDll)) {
    throw "OneUI DLL not found: $oneuiDll"
}

if (!(Test-Path $objdump)) {
    throw "objdump not found: $objdump"
}

if (Test-Path $dist) {
    Remove-Item -LiteralPath $dist -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath $exe -Destination $dist
Copy-Item -LiteralPath $oneuiDll -Destination $dist

$runtime = Copy-OneUIRuntimeDependencies `
    -SeedBinaries @((Join-Path $dist "oneui_gallery.exe"), (Join-Path $dist "oneui.dll")) `
    -DestinationDirectory $dist `
    -SearchDirectory $toolchainBin `
    -Objdump $objdump

if ($runtime.MissingDlls.Count -ne 0) {
    $detail = $runtime.MissingDetails -join "; "
    throw "Missing runtime DLLs for Gallery package: $($runtime.MissingDlls -join ', '). Imports: $detail"
}

$readme = @"
OneUI Gallery
=============

Run:
  oneui_gallery.exe

This package includes the MSYS2 UCRT64 DLLs imported by the executable and Skia.

Windows 7 note:
  Windows 7 requires the Microsoft Universal CRT runtime to be installed on the system.
  The package intentionally does not bundle the Windows system CRT api-ms-win/ucrtbase DLLs.
"@

Set-Content -LiteralPath (Join-Path $dist "README.txt") -Value $readme -Encoding ASCII

$zip = Join-Path $distRoot "oneui-gallery-$Toolchain.zip"
if (Test-Path $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip

$distSize = (Get-ChildItem -LiteralPath $dist -Recurse -File | Measure-Object -Property Length -Sum).Sum
$zipSize = (Get-Item -LiteralPath $zip).Length

[PSCustomObject]@{
    Dist = $dist
    Zip = $zip
    Files = (Get-ChildItem -LiteralPath $dist -File).Count
    UncompressedBytes = $distSize
    ZipBytes = $zipSize
    MissingDlls = ($runtime.MissingDlls -join ", ")
}
