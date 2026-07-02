$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"

& "$root\build\mingw64\examples\gallery\oneui_gallery.exe"
