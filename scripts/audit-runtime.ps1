param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,

    [ValidateSet("development", "product")]
    [string]$Mode = "development",

    [ValidateSet("mingw64", "ucrt64")]
    [string]$Toolchain = "mingw64",

    [switch]$AllowOneUI,
    [ValidateSet('win7','win10')][string]$MinimumWindows = 'win10',
    [string]$WindowsReference = "",
    [string[]]$RuntimeDirectory = @()
)

$ErrorActionPreference = "Stop"

if ($WindowsReference) {
    $auditArgs = @((Join-Path $PSScriptRoot 'audit-windows-runtime.py'), '--binary', $Binary, '--reference', $WindowsReference)
    foreach ($directory in $RuntimeDirectory) { $auditArgs += @('--runtime-dir', $directory) }
    & python @auditArgs
    if ($LASTEXITCODE -ne 0) { throw "Windows baseline import audit failed" }
    return
}
if ($Mode -eq 'product' -and $MinimumWindows -eq 'win7') {
    throw "Product audit requires -WindowsReference from the target Windows baseline. DLL names alone cannot prove compatibility. Use -Mode development only for development dependency collection."
}

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
    WindowsCompatibilityVerified = $false
}

$result

if ($external.Count -ne 0) {
    exit 1
}
