$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-profile.ps1")
$env:PATH = "$(Resolve-OneUIMsysBin mingw64);$env:PATH"

& "$root\build\mingw64\examples\gallery\oneui_gallery.exe"
