$script:OneUIBuildRoot = Split-Path -Parent $PSScriptRoot

function Get-OneUIWindowsBuildProfile {
    param([ValidateSet('win7', 'win10')][string]$MinimumWindows = 'win10')
    $profiles = Get-Content -LiteralPath (Join-Path $script:OneUIBuildRoot 'cmake/windows-build-profiles.json') -Raw | ConvertFrom-Json
    return $profiles.$MinimumWindows
}

function Resolve-OneUIBuildPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $script:OneUIBuildRoot $Path))
}
