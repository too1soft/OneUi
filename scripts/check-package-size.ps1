param(
    [string]$Zip = "dist/OneUI-SDK-msvc-bundled-static.zip",
    [long]$MaxBytes = 5242880
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$zipPath = if ([System.IO.Path]::IsPathRooted($Zip)) { $Zip } else { Join-Path $root $Zip }

if (-not (Test-Path -LiteralPath $zipPath)) {
    throw "Package was not found: $zipPath"
}

$item = Get-Item -LiteralPath $zipPath
$pass = $item.Length -le $MaxBytes

[pscustomobject]@{
    Package = $item.FullName
    Bytes = $item.Length
    MaxBytes = $MaxBytes
    Pass = $pass
}

if (-not $pass) {
    throw "Package size $($item.Length) exceeds limit $MaxBytes"
}
