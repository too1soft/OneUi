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

. (Join-Path $PSScriptRoot "runtime-dependencies.ps1")

$toolchainBin = "C:\msys64\$Toolchain\bin"
$objdump = Join-Path $toolchainBin "objdump.exe"

if (!(Test-Path $Binary)) {
    throw "Binary not found: $Binary"
}

if (!(Test-Path $objdump)) {
    throw "objdump not found: $objdump"
}

$allowedDevelopmentDlls = @(
    "oneui.dll",
    "libskia.dll",
    "libwinpthread-1.dll"
)

$imports = Get-OneUIImportedDlls -Binary $Binary -Objdump $objdump | Sort-Object -Unique
$external = @()

foreach ($dll in $imports) {
    $lower = $dll.ToLowerInvariant()
    if (Test-OneUISystemDll -Name $dll) {
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
