param(
    [string]$SkiaSource = "third_party/skia",
    [string]$DepotTools = "third_party/depot_tools",
    [string]$DepotToolsUrl = "https://chromium.googlesource.com/chromium/tools/depot_tools.git",
    [string]$SkiaUrl = "https://skia.googlesource.com/skia.git",
    [string]$OutDir = "",
    [ValidateSet("x64", "x86")]
    [string]$TargetCpu = "x64",
    [string]$WinSdk = "D:/Windows Kits/10",
    [string]$WinVc = "D:/Program Files/Microsoft Visual Studio/18/Community/VC",
    [ValidateSet("win7", "win10")]
    [string]$MinimumWindows = "win10",
    [string]$Revision = "1f26101197bff9fcd939a791beb3094297436d59",
    [int]$Depth = 1,
    [string]$Proxy = "",
    [switch]$Fetch,
    [switch]$SyncDeps,
    [switch]$Generate,
    [switch]$Build,
    [ValidateRange(1, 32)]
    [int]$Jobs = 6
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "windows-build-profile.ps1")
$windowsBuildProfile = Get-OneUIWindowsBuildProfile -MinimumWindows $MinimumWindows

$root = Split-Path -Parent $PSScriptRoot
$skiaPath = Resolve-OneUIBuildPath $SkiaSource
$depotToolsPath = Resolve-OneUIBuildPath $DepotTools
if (!$OutDir) {
    $OutDir = "third_party/skia/out/oneui-win-$TargetCpu-release"
    if ($MinimumWindows -eq 'win7') { $OutDir += '-win7' }
}
$outPath = Resolve-OneUIBuildPath $OutDir

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
if (($Generate -or $Build) -and (& $git -C $skiaPath rev-parse HEAD) -ne $Revision) {
    throw "Skia revision does not match the reviewed text-engine dependency pin: $Revision"
}

if ($SyncDeps) {
    if (!(Test-Path $skiaPath)) {
        throw "Skia source not found: $skiaPath. Run with -Fetch first."
    }
    Run $python "tools/git-sync-deps" $skiaPath
}

if ($SyncDeps -or $Generate -or $Build) {
    $cmake = Require-Command "cmake"
    Run $cmake "`"-DONEUI_ICU_ROOT=$skiaPath/third_party/externals/icu`" -P `"$root/scripts/apply-skia-patches.cmake`""
    if ($MinimumWindows -eq 'win7') {
        Run $cmake "`"-DONEUI_SKIA_ROOT=$skiaPath`" -P `"$root/scripts/apply-skia-win7-patches.cmake`""
    }
}

$winver = if ($MinimumWindows -eq 'win7') { '0x0601' } else { '0x0A00' }
$ntddi = if ($MinimumWindows -eq 'win7') { '0x06010000' } else { '0x0A000000' }
$portableFonts = if ($MinimumWindows -eq 'win7') { 'skia_use_freetype=true' + "`nskia_use_system_freetype2=false`nskia_enable_fontmgr_custom_empty=true`nskia_enable_fontmgr_custom_directory=false`nskia_enable_fontmgr_custom_embedded=false`nskia_enable_fontmgr_android=false" } else { '' }
$wrapperArgs = ''
if ($Generate -or $Build) {
    $env:VSLANG = "1033"
    New-Item -ItemType Directory -Force -Path $outPath | Out-Null
    $toolset = $windowsBuildProfile.toolsetVersion
    if (!$toolset) {
        $toolset = (Get-ChildItem -LiteralPath (Join-Path $WinVc 'Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
    }
    $compiler = Join-Path $WinVc "Tools/MSVC/$toolset/bin/HostX64/$TargetCpu/cl.exe"
    $prefixFile = Join-Path $outPath 'oneui-msvc-includes.json'
    $wrapper = Join-Path $PSScriptRoot 'msvc-includes-wrapper.py'
    & $python $wrapper --probe $compiler --prefix-file $prefixFile
    if ($LASTEXITCODE -ne 0) { throw 'MSVC header dependency probe failed' }
    # Use the actual Python executable, not a shell shim, in Ninja commands.
    $pythonExe = (& $python -c 'import sys; print(sys.executable)').Trim().Replace('\', '/')
    $wrapperArgs = 'cc_wrapper="\"' + $pythonExe + '\" \"' + $wrapper.Replace('\', '/') + '\" --prefix-file \"' + $prefixFile.Replace('\', '/') + '\" --"'
}
$gnArgs = @"
is_debug=false
is_official_build=true
target_cpu="$TargetCpu"
win_sdk="$WinSdk"
win_vc="$WinVc"
win_toolchain_version="$($windowsBuildProfile.toolsetVersion)"
win_sdk_version="$($windowsBuildProfile.windowsSdkVersion)"
$wrapperArgs
$portableFonts
extra_cflags=["/DNTDDI_VERSION=$ntddi", "/DWINVER=$winver", "/D_WIN32_WINNT=$winver"]
extra_cflags_cc=["/DNTDDI_VERSION=$ntddi", "/DWINVER=$winver", "/D_WIN32_WINNT=$winver"]
skia_use_system_expat=false
skia_use_system_harfbuzz=false
skia_use_system_icu=false
skia_use_icu=true
skia_use_harfbuzz=true
skia_enable_skparagraph=true
skia_enable_skshaper=true
skia_enable_skunicode=true
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

    $env:VSLANG = "1033"
    if (!(Select-String -LiteralPath (Join-Path $outPath 'args.gn') -Pattern 'msvc-includes-wrapper.py' -Quiet)) {
        throw 'Regenerate this Skia output with -Generate before building; header tracking wrapper is missing.'
    }
    # Native Start-Process stdout otherwise bypasses the caller's redirection.
    # Keep full diagnostics on disk without flooding the terminal with MSVC
    # /showIncludes output on localized compiler installations.
    $stdout = Join-Path $outPath "oneui-build.stdout.log"
    $stderr = Join-Path $outPath "oneui-build.stderr.log"
    $process = Start-Process -FilePath $ninja.Source -ArgumentList "-C `"$outPath`" -j $Jobs skia skparagraph skunicode_icu" -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Get-Content -LiteralPath $stdout -Tail 8
    if ($process.ExitCode -ne 0) {
        Get-Content -LiteralPath $stderr -Tail 20
        throw "Skia build failed; see $stdout and $stderr"
    }
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
    Write-Host "  .\scripts\build-oneui-msvc-bundled.ps1 -Arch $TargetCpu -MinimumWindows $MinimumWindows -SkiaOut `"$outPath`""
} else {
    Write-Host "Static Skia library was not found yet."
    Write-Host "Expected one of:"
    foreach ($path in $candidateLibs) {
        Write-Host "  $path"
    }
}
