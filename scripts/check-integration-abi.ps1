<#
Run only after the root grants the build slot and rebuilds capture.
This is a DLL consumer check, not a CMake configure/build wrapper.
Temporary files use capture/integration-abi; no DLL is copied.
#>
param([Parameter(Mandatory)][string]$ExpectedDllSha256)
$ErrorActionPreference = 'Stop'
$sdk = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$capture = Join-Path $sdk 'build/capture'
$output = Join-Path $capture 'integration-abi'
$baseline = '64a5b89'
$cache = Get-Content (Join-Path $capture 'CMakeCache.txt')
function CacheValue([string]$name) {
    $line = @($cache | Where-Object { $_ -match ('^' + [regex]::Escape($name) + ':[^=]+=(.*)$') })
    if ($line.Count -ne 1) { throw "Missing or ambiguous cache value: $name" }
    return $line[0].Substring($line[0].IndexOf('=') + 1)
}
function AssertSafePath([string]$path) {
    $absolute = [IO.Path]::GetFullPath($path)
    if (-not $absolute.StartsWith([IO.Path]::GetFullPath($output) + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw "Outside ABI output: $path" }
    for ($p = $absolute; $p; $p = [IO.Path]::GetDirectoryName($p)) {
        if ((Test-Path -LiteralPath $p) -and
            ((Get-Item -LiteralPath $p).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "ABI path is a reparse point: $p"
        }
    }
}
if ((CacheValue 'CMAKE_GENERATOR') -ne 'Ninja' -or
    (CacheValue 'CMAKE_BUILD_TYPE') -ne 'RelWithDebInfo' -or
    (CacheValue 'ONEUI_SKIA_MODE') -ne 'bundled-static' -or
    [IO.Path]::GetFullPath((CacheValue 'CMAKE_HOME_DIRECTORY')) -ne $sdk) {
    throw 'Capture configuration differs from the reviewed integration configuration.'
}
$compiler = CacheValue 'CMAKE_CXX_COMPILER'
$ninja = Get-Content (Join-Path $capture 'build.ninja')
$widget = @($ninja | Select-String '^build CMakeFiles[\\/]oneui_core.dir[\\/]src[\\/]core[\\/]widget.cpp.obj:')
if ($widget.Count -ne 1) { throw 'Cannot identify the actual Widget compiler flags.' }
$block = $ninja[($widget[0].LineNumber)..($widget[0].LineNumber + 8)]
$flags = @((($block | Where-Object { $_ -match '^  FLAGS = ' }) -replace '^  FLAGS = ', '') -split ' ' | Where-Object { $_ })
$defines = @((($block | Where-Object { $_ -match '^  DEFINES = ' }) -replace '^  DEFINES = ', '') -split ' ' |
    Where-Object { $_ -and $_ -ne '-DONEUI_BUILDING_DLL' -and $_ -ne '-DONEUI_STATIC' })
if ($flags -notcontains '-MT' -or $flags -contains '-MD' -or $defines -match 'ONEUI_BUILDING_DLL|ONEUI_STATIC') {
    throw 'Unexpected CRT or DLL consumer definitions.'
}
if ($env:CL -or $env:_CL_ -or $env:LINK) { throw 'Unreviewed CL/_CL_/LINK environment overrides.' }
$dll = Join-Path $capture 'oneui.dll'
if ((Get-FileHash -Algorithm SHA256 $dll).Hash -ne $ExpectedDllSha256) { throw 'Capture DLL hash differs from the rebuilt candidate.' }
$previousPath = $env:PATH
. (Join-Path $sdk '../ishell_oneui/scripts/native_build_environment.ps1')
Initialize-MsvcEnvironment
if ([IO.Path]::GetFullPath((Get-Command cl.exe).Source) -ne [IO.Path]::GetFullPath($compiler)) {
    throw 'MSVC helper selected a different compiler from CMakeCache.'
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
$transient = @('old-tree.zip', 'old', 'old-probe.exe', 'old-probe.obj', 'old-probe.pdb', 'old-compile.pdb',
    'current-probe.exe', 'current-probe.obj', 'current-probe.pdb', 'current-compile.pdb')
foreach ($name in $transient) {
    $path = Join-Path $output $name; AssertSafePath $path
    if (Test-Path -LiteralPath $path) { throw "Review previous failed ABI artifacts before reusing: $path" }
}
$success = $false
Push-Location $capture
try {
    $env:PATH = "$capture;$env:PATH"
    $archive = Join-Path $output 'old-tree.zip'
    & git -C $sdk archive --format=zip "--output=$archive" $baseline include
    if ($LASTEXITCODE) { throw 'Cannot export the real D12 include tree.' }
    Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $output 'old')
    $identity = @(& git -C $sdk rev-parse $baseline "${baseline}:include")
    if ($LASTEXITCODE) { throw 'Cannot record the baseline Git identity.' }
    @("baseline=$($identity[0])", "includeTree=$($identity[1])", "compiler=$compiler", "flags=$flags",
      "defines=$defines", "dllSha256=$ExpectedDllSha256") | Set-Content (Join-Path $output 'provenance.txt')
    foreach ($variant in @('old', 'current')) {
        $include = if ($variant -eq 'old') { Join-Path $output 'old/include' } else { Join-Path $sdk 'include' }
        $exe = Join-Path $output "$variant-probe.exe"
        & $compiler /nologo @flags @defines "/I$include" (Join-Path $sdk 'tests/abi/presentation_layout_probe.cpp') `
            (Join-Path $capture 'oneui.lib') "/Fo$(Join-Path $output "$variant-probe.obj")" `
            "/Fd$(Join-Path $output "$variant-compile.pdb")" "/Fe$exe" /link /DEBUG /INCREMENTAL:NO `
            "/PDB:$(Join-Path $output "$variant-probe.pdb")" *> (Join-Path $output "$variant-compile.log")
        if ($LASTEXITCODE) { throw "$variant header probe did not compile; retain failure evidence." }
        & $exe 1> (Join-Path $output "$variant-stdout.txt") 2> (Join-Path $output "$variant-stderr.txt")
        if ($LASTEXITCODE) { throw "$variant header probe failed at runtime; retain failure evidence." }
    }
    $old = [IO.File]::ReadAllText((Join-Path $output 'old-stdout.txt'))
    $current = [IO.File]::ReadAllText((Join-Path $output 'current-stdout.txt'))
    if ($old -cne $current) { throw 'Old/current full ABI stdout differs; retain both outputs.' }
    if ((Get-FileHash -Algorithm SHA256 $dll).Hash -ne $ExpectedDllSha256) { throw 'DLL changed during ABI verification.' }
    $success = $true
    Write-Output 'ABI consumers completed; complete old/current stdout is identical.'
} finally {
    Pop-Location
    $env:PATH = $previousPath
    if ($success) {
        foreach ($name in $transient) {
            $path = Join-Path $output $name; AssertSafePath $path
            if (Test-Path -LiteralPath $path) {
                if ((Get-Item -LiteralPath $path).PSIsContainer) {
                    if (Get-ChildItem -LiteralPath $path -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
                        throw "Unexpected link in ABI temporary tree: $path"
                    }
                    Remove-Item -LiteralPath $path -Recurse
                } else { Remove-Item -LiteralPath $path }
            }
        }
    }
}
