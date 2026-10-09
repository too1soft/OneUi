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

function Resolve-OneUIVisualStudio {
    param([string]$InstallPath = '')
    if (-not $InstallPath) { $InstallPath = $env:VSINSTALLDIR }
    if (-not $InstallPath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $InstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
        }
    }
    if (-not $InstallPath -or -not (Test-Path -LiteralPath (Join-Path $InstallPath 'VC/Auxiliary/Build/vcvarsall.bat'))) {
        throw 'Visual Studio C++ tools were not found. Pass -VsInstall or set VSINSTALLDIR.'
    }
    return [IO.Path]::GetFullPath($InstallPath)
}

function Resolve-OneUIBuildTool {
    param([ValidateSet('cmake', 'ninja')][string]$Name, [string]$VsInstall)
    $relative = if ($Name -eq 'cmake') { 'CMake/bin/cmake.exe' } else { 'Ninja/ninja.exe' }
    $bundled = Join-Path $VsInstall "Common7/IDE/CommonExtensions/Microsoft/CMake/$relative"
    if (Test-Path -LiteralPath $bundled) { return $bundled }
    $command = Get-Command "$Name.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw "$Name was not found in Visual Studio or PATH."
}

function Resolve-OneUIWindowsSdk {
    param([string]$SdkPath = '')
    if (-not $SdkPath) { $SdkPath = $env:WindowsSdkDir }
    if (-not $SdkPath) {
        $SdkPath = (Get-ItemProperty 'HKLM:/SOFTWARE/Microsoft/Windows Kits/Installed Roots' -ErrorAction SilentlyContinue).KitsRoot10
    }
    if (-not $SdkPath -or -not (Test-Path -LiteralPath (Join-Path $SdkPath 'Include'))) {
        throw 'Windows SDK was not found. Pass -WinSdk or set WindowsSdkDir.'
    }
    return [IO.Path]::GetFullPath($SdkPath)
}

function Resolve-OneUIMsysBin {
    param([ValidateSet('mingw64', 'ucrt64')][string]$Toolchain = 'mingw64')
    if ($env:MSYS2_ROOT) {
        $bin = Join-Path $env:MSYS2_ROOT "$Toolchain/bin"
    } else {
        $compiler = Get-Command g++.exe -ErrorAction SilentlyContinue
        if ($compiler) { $bin = Split-Path -Parent $compiler.Source }
    }
    if (-not $bin -or (Split-Path -Leaf (Split-Path -Parent $bin)) -ne $Toolchain -or
        -not (Test-Path -LiteralPath (Join-Path $bin 'objdump.exe'))) {
        throw "Set MSYS2_ROOT to your MSYS2 installation, or put $Toolchain/bin on PATH."
    }
    return $bin
}
