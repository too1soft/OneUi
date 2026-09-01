param(
    [string]$Header = "include/oneui/oneui_c_api.h",
    [string]$RustBindings = "bindings/rust/oneui-sys/src"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root $Header
$rustPath = Join-Path $root $RustBindings

$headerText = Get-Content -LiteralPath $headerPath -Raw
$rustFiles = if (Test-Path -LiteralPath $rustPath -PathType Container) {
    Get-ChildItem -LiteralPath $rustPath -Filter *.rs -File -Recurse | Sort-Object FullName
} else {
    Get-Item -LiteralPath $rustPath
}
$rustText = ($rustFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
$headerVersionMatch = [regex]::Match($headerText, '#define\s+ONEUI_UTF8_ABI_VERSION\s+(\d+)u')
$rustVersionMatch = [regex]::Match($rustText, 'pub const UTF8_ABI_VERSION:\s*c_uint\s*=\s*(\d+);')
if (!$headerVersionMatch.Success -or !$rustVersionMatch.Success) {
    throw "Unable to read ABI versions from the C header and Rust bindings"
}
if ($headerVersionMatch.Groups[1].Value -ne $rustVersionMatch.Groups[1].Value) {
    throw "ABI version mismatch: C=$($headerVersionMatch.Groups[1].Value), Rust=$($rustVersionMatch.Groups[1].Value)"
}

$supportedSymbols = [regex]::Matches($rustText, 'pub fn\s+(oneui_[a-z0-9_]+)\s*\(') |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique
$missing = @($supportedSymbols | Where-Object { $headerText -notmatch "\b$([regex]::Escape($_))\s*\(" })
if ($missing.Count -ne 0) {
    throw "Rust declares C ABI symbols missing from the public header: $($missing -join ', ')"
}

[PSCustomObject]@{
    AbiVersion = [int]$headerVersionMatch.Groups[1].Value
    PortableRustSymbols = $supportedSymbols.Count
    Pass = $true
}
