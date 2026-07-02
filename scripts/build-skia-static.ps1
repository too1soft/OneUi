param(
    [string]$SkiaSource = "third_party/skia",
    [string]$DepotTools = "third_party/depot_tools",
    [string]$DepotToolsUrl = "https://chromium.googlesource.com/chromium/tools/depot_tools.git",
    [string]$SkiaUrl = "https://skia.googlesource.com/skia.git",
    [string]$OutDir = "third_party/skia/out/oneui-win-x64-release",
    [ValidateSet("x64", "x86")]
    [string]$TargetCpu = "x64",
    [string]$WinSdk = "D:/Windows Kits/10",
    [string]$WinVc = "D:/Program Files/Microsoft Visual Studio/18/Community/VC",
    [string]$Revision = "",
    [int]$Depth = 1,
    [string]$Proxy = "",
    [switch]$Fetch,
    [switch]$SyncDeps,
    [switch]$Generate,
    [switch]$Build
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$skiaPath = Join-Path $root $SkiaSource
$depotToolsPath = Join-Path $root $DepotTools
$outPath = Join-Path $root $OutDir

function Run($File, $Arguments, $WorkingDirectory = $root) {
    Write-Host ">> $File $Arguments"
    $p = Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $WorkingDirectory -Wait -PassThru -NoNewWindow
    if ($p.ExitCode -ne 0) {
        throw "Command failed with exit code $($p.ExitCode): $File $Arguments"
    }
}

function Require-Command($Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (!$cmd) {
        throw "Required command not found: $Name"
    }
    return $cmd.Source
}

$git = Require-Command "git"
$python = Require-Command "python"
$gitBaseArgs = "-c http.version=HTTP/1.1 "
$gitProxyArgs = if ($Proxy -ne "") { "-c http.proxy=$Proxy -c https.proxy=$Proxy " } else { "" }

if ($Proxy -ne "") {
    $env:http_proxy = $Proxy
    $env:https_proxy = $Proxy
    $env:HTTP_PROXY = $Proxy
    $env:HTTPS_PROXY = $Proxy
}
$env:GIT_HTTP_VERSION = "HTTP/1.1"

if (!$Fetch -and !$SyncDeps -and !$Generate -and !$Build) {
    Write-Host "No phase was selected. Use one or more of: -Fetch -SyncDeps -Generate -Build."
    Write-Host "Typical first run:"
    Write-Host "  .\scripts\build-skia-static.ps1 -Fetch -SyncDeps -Generate -Build"
    exit 0
}

if ($Fetch) {
    if (!(Test-Path $depotToolsPath)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $depotToolsPath) | Out-Null
        $depthArg = if ($Depth -gt 0) { "--depth=$Depth " } else { "" }
        Run $git "${gitBaseArgs}${gitProxyArgs}clone ${depthArg}$DepotToolsUrl `"$depotToolsPath`""
    }

    if (!(Test-Path $skiaPath)) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $skiaPath) | Out-Null
        $depthArg = if ($Depth -gt 0) { "--depth=$Depth " } else { "" }
        Run $git "${gitBaseArgs}${gitProxyArgs}clone ${depthArg}$SkiaUrl `"$skiaPath`""
    }

    if ($Revision -ne "") {
        Run $git "${gitBaseArgs}${gitProxyArgs}fetch --all --tags" $skiaPath
        Run $git "checkout $Revision" $skiaPath
    }
}

$env:PATH = "$depotToolsPath;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

if ($SyncDeps) {
    if (!(Test-Path $skiaPath)) {
        throw "Skia source not found: $skiaPath. Run with -Fetch first."
    }
    Run $python "tools/git-sync-deps" $skiaPath
}

$gnArgs = @"
is_debug=false
is_official_build=true
target_cpu="$TargetCpu"
win_sdk="$WinSdk"
win_vc="$WinVc"
extra_cflags=["/DNTDDI_VERSION=0x06010000", "/DWINVER=0x0601", "/D_WIN32_WINNT=0x0601"]
extra_cflags_cc=["/DNTDDI_VERSION=0x06010000", "/DWINVER=0x0601", "/D_WIN32_WINNT=0x0601"]
skia_use_system_expat=false
skia_use_system_harfbuzz=false
skia_use_system_icu=false
skia_use_system_libjpeg_turbo=false
skia_use_system_libpng=false
skia_use_system_libwebp=false
skia_use_system_zlib=false
skia_enable_pdf=false
skia_enable_skottie=false
skia_enable_svg=false
skia_enable_tools=false
skia_use_dng_sdk=false
skia_use_wuffs=false
"@

if ($Generate) {
    $gnExe = Join-Path $skiaPath "bin/gn.exe"
    if (!(Test-Path $gnExe)) {
        $gn = Get-Command "gn" -ErrorAction SilentlyContinue
        if (!$gn) {
            throw "gn was not found. Expected $gnExe or a gn command on PATH."
        }
        $gnExe = $gn.Source
    }

    New-Item -ItemType Directory -Force -Path $outPath | Out-Null
    Set-Content -LiteralPath (Join-Path $outPath "args.gn") -Value $gnArgs -Encoding ASCII
    Run $gnExe "gen `"$outPath`" --root=`"$skiaPath`""
}

if ($Build) {
    $ninja = Get-Command "ninja" -ErrorAction SilentlyContinue
    if (!$ninja) {
        throw "ninja was not found. Install it or put it on PATH."
    }

    Run "ninja" "-C `"$outPath`" skia"
}

$candidateLibs = @(
    (Join-Path $outPath "libskia.a"),
    (Join-Path $outPath "skia.lib"),
    (Join-Path $outPath "libskia.lib")
)

$found = $candidateLibs | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($found) {
    Write-Host "Static Skia candidate found: $found"
    Write-Host "Configure OneUI with:"
    Write-Host "  C:\msys64\mingw64\bin\cmake.exe --preset mingw64-bundled-static"
} else {
    Write-Host "Static Skia library was not found yet."
    Write-Host "Expected one of:"
    foreach ($path in $candidateLibs) {
        Write-Host "  $path"
    }
}
