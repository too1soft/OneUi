param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'windows-build-profile.ps1')
$root = Split-Path -Parent $PSScriptRoot
$build = Resolve-OneUIBuildPath $BuildDir
$output = Resolve-OneUIBuildPath $OutputDirectory
if (Test-Path -LiteralPath $output) { throw 'Acceptance bundle already exists; choose a new output directory' }
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'oneui.dll'),(Join-Path $build 'examples/gallery/oneui_gallery.exe') -Destination $output
Get-ChildItem -LiteralPath $build -File | Where-Object { $_.Name -match '^oneui_.*tests\.exe$' } | Copy-Item -Destination $output
Copy-Item -LiteralPath (Join-Path $root 'out/text-assets') -Destination (Join-Path $output 'assets') -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test-windows-native-bundle.ps1') -Destination $output
Get-ChildItem -LiteralPath $output -Recurse -File | Sort-Object FullName | ForEach-Object {
    $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($_.FullName.Substring($output.Length + 1))"
} | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS') -Encoding ASCII
Write-Host "Prepared test-only bundle: $output"
