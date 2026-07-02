param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,

    [ValidateSet("development", "product")]
    [string]$Mode = "development",

    [ValidateSet("mingw64", "ucrt64")]
    [string]$Toolchain = "mingw64",

    [switch]$AllowOneUI
)

$ErrorActionPreference = "Stop"

$toolchainBin = "C:\msys64\$Toolchain\bin"
$objdump = Join-Path $toolchainBin "objdump.exe"

if (!(Test-Path $Binary)) {
    throw "Binary not found: $Binary"
}

if (!(Test-Path $objdump)) {
    throw "objdump not found: $objdump"
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

$allowedDevelopmentDlls = @(
    "oneui.dll",
    "libskia.dll",
    "libwinpthread-1.dll"
)

function Get-ImportedDlls($path) {
    & $objdump -p $path |
        Select-String "DLL Name:\s*(.+)$" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Is-SystemDll($name) {
    $lower = $name.ToLowerInvariant()
    return $systemDlls -contains $lower -or $lower.StartsWith("api-ms-win-") -or $lower.StartsWith("ext-ms-win-")
}

$imports = Get-ImportedDlls $Binary | Sort-Object -Unique
$external = @()

foreach ($dll in $imports) {
    $lower = $dll.ToLowerInvariant()
    if (Is-SystemDll $dll) {
        continue
    }

    if ($AllowOneUI -and $lower -eq "oneui.dll") {
        continue
    }

    if ($Mode -eq "development" -and ($allowedDevelopmentDlls -contains $lower)) {
        continue
    }

    $external += $dll
}

$result = [PSCustomObject]@{
    Binary = (Resolve-Path $Binary).Path
    Mode = $Mode
    Imports = ($imports -join ", ")
    ExternalDependencies = (($external | Sort-Object -Unique) -join ", ")
    Pass = $external.Count -eq 0
}

$result

if ($external.Count -ne 0) {
    exit 1
}
