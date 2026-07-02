param(
    [ValidateSet("mingw64", "ucrt64")]
    [string]$Toolchain = "mingw64"
)

$ErrorActionPreference = "Stop"

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

$seen = @{}
$missing = New-Object System.Collections.Generic.List[string]
$queue = New-Object System.Collections.Generic.Queue[string]
$queue.Enqueue((Join-Path $dist "oneui_gallery.exe"))
$queue.Enqueue((Join-Path $dist "oneui.dll"))

function Get-ImportedDlls($binary) {
    & $objdump -p $binary |
        Select-String "DLL Name:\s*(.+)$" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Is-SystemDll($name) {
    $lower = $name.ToLowerInvariant()
    return $systemDlls -contains $lower -or $lower.StartsWith("api-ms-win-") -or $lower.StartsWith("ext-ms-win-")
}

while ($queue.Count -gt 0) {
    $current = $queue.Dequeue()
    foreach ($dll in Get-ImportedDlls $current) {
        $key = $dll.ToLowerInvariant()
        if ($seen.ContainsKey($key) -or (Is-SystemDll $dll)) {
            continue
        }

        $seen[$key] = $true
        $target = Join-Path $dist $dll
        if (Test-Path $target) {
            continue
        }

        $source = Join-Path $toolchainBin $dll
        if (!(Test-Path $source)) {
            $missing.Add($dll)
            continue
        }

        Copy-Item -LiteralPath $source -Destination $target
        $queue.Enqueue($target)
    }
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
    MissingDlls = ($missing | Sort-Object -Unique) -join ", "
}
