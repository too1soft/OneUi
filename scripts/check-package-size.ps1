param(
    [string]$Zip = "dist/OneUI-SDK-msvc-bundled-static.zip",
    [ValidateRange(0, [long]::MaxValue)]
    [long]$MaxBytes = 0,
    [switch]$AuditOnly
)

$ErrorActionPreference = "Stop"

if (-not $AuditOnly -and $MaxBytes -eq 0) {
    throw "Specify -AuditOnly to report measured size, or -MaxBytes with an explicitly reviewed full-text SDK budget. The legacy 5 MiB limit no longer applies."
}
if ($AuditOnly -and $MaxBytes -ne 0) {
    throw "Use either -AuditOnly or -MaxBytes, not both."
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$zipPath = if ([System.IO.Path]::IsPathRooted($Zip)) { $Zip } else { Join-Path $root $Zip }

if (-not (Test-Path -LiteralPath $zipPath)) {
    throw "Package was not found: $zipPath"
}

$item = Get-Item -LiteralPath $zipPath
$pass = if ($AuditOnly) { $null } else { $item.Length -le $MaxBytes }

[pscustomobject]@{
    Package = $item.FullName
    Bytes = $item.Length
    MaxBytes = $MaxBytes
    Mode = if ($AuditOnly) { "AuditOnly (not release approval)" } else { "EnforceReviewedBudget" }
    Pass = $pass
}

if (-not $AuditOnly -and -not $pass) {
    throw "Package size $($item.Length) exceeds limit $MaxBytes"
}
