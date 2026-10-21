param(
    [Parameter(Mandatory)][string]$Version,
    [string]$Source = 'D:/oneui-sdk/src',
    [string]$Build = 'D:/oneui-sdk/build',
    [string]$Packages = 'D:/oneui-sdk/dist',
    [string]$Licenses = 'D:/oneui-sdk/stage/licenses',
    [string]$Config = '',
    [string]$Python = 'python',
    [string]$CMake = 'cmake',
    [string]$ArtifactAudit = '',
    [double]$ApprovedBudgetGiB = 20,
    [switch]$ExistingPackages,
    [switch]$Publish
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Config) { $Config = Join-Path $repo 'website/deploy/oneui-production.json' }
if ($Version -notmatch '^\d+\.\d+\.\d+-preview\.[0-9]+$') { throw 'This pipeline publishes preview versions only.' }
$Source = (Resolve-Path -LiteralPath $Source).Path
$Build = (Resolve-Path -LiteralPath $Build).Path
$Packages = [IO.Path]::GetFullPath($Packages)
$work = Join-Path (Split-Path -Parent $Build) 'release-current'
$report = Join-Path $repo 'out/native-release-review/publish-result.json'
function Run-Python([string[]]$Arguments) {
    & $Python -B @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Release step failed; publication stopped.' }
}
function Remove-OwnedFixture([string]$Path, [string]$Root) {
    $resolved = [IO.Path]::GetFullPath($Path)
    $boundary = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup escaped owned root.' }
    if (-not (Test-Path -LiteralPath $resolved)) { return }
    if (-not (Test-Path -LiteralPath (Join-Path $Root '.oneui-release-owned'))) { throw 'Missing ownership marker.' }
    $links = @(Get-Item -LiteralPath $resolved; Get-ChildItem -LiteralPath $resolved -Recurse -Force) |
        Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }
    if ($links) { throw 'Refusing cleanup containing links.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
if (-not $ExistingPackages) {
    # A designated SDK maintainer must run this; applications use sdk fetch instead.
    if (-not $ArtifactAudit) { throw 'Provide the current cross-disk artifact audit JSON via -ArtifactAudit before building.' }
    $audit = Get-Content -LiteralPath $ArtifactAudit -Raw | ConvertFrom-Json
    $roots = @($audit.roots | ForEach-Object { $_.path }) + @($Build, $work)
    $roots = @($roots | ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\', '/') } | Sort-Object -Unique)
    $roots = @($roots | Where-Object {
        $candidate = $_
        -not @($roots | Where-Object { $candidate -ne $_ -and $candidate.StartsWith($_ + '\', [StringComparison]::OrdinalIgnoreCase) }).Count
    })
    $auditTool = Join-Path $env:USERPROFILE '.codex/skills/build-artifact-hygiene/scripts/audit_artifacts.py'
    if (-not (Test-Path -LiteralPath $auditTool)) { throw 'Install the build-artifact-hygiene audit helper first.' }
    $auditArgs = @($auditTool, '--kind', 'build', '--max-gib', "$ApprovedBudgetGiB")
    foreach ($root in $roots) {
        if (Test-Path -LiteralPath $root) { $auditArgs += @('--root', $root) }
    }
    Run-Python $auditArgs
    Run-Python @((Join-Path $PSScriptRoot 'check-build-space.py'), '--minimum-gib', '22', $Build, $Packages)
    $dirty = & git -C $Source status --porcelain
    if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Build source must be an identified clean Git revision.' }
    $cache = Get-Content -LiteralPath (Join-Path $Build 'CMakeCache.txt') -Raw
    $home = [regex]::Match($cache, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$').Groups[1].Value.Trim()
    if ([IO.Path]::GetFullPath($home) -ne $Source) { throw 'Build cache belongs to another source tree.' }
    foreach ($required in @('CMAKE_BUILD_TYPE:STRING=Release', 'ONEUI_WINDOWS_BASELINE:STRING=win10',
        'ONEUI_ENABLE_DEVELOPER_TOOLS:BOOL=OFF', 'ONEUI_ENABLE_TEST_FRAME_CAPTURE:BOOL=OFF')) {
        if (-not $cache.Contains($required)) { throw "Production configuration missing: $required" }
    }
    $recipe = Get-Content -LiteralPath (Join-Path $Build 'sdk-inputs-Release.json') -Raw | ConvertFrom-Json
    if ($recipe.architecture -ne 'x86_64' -or $recipe.profile -ne 'modern') { throw 'Expected Windows modern x64 build.' }
    foreach ($mode in @('shared', 'static')) {
        if (Test-Path -LiteralPath (Join-Path $Packages "oneui-$Version-windows-modern-x86_64-$mode.zip")) {
            throw 'Versioned packages already exist. Use -ExistingPackages to publish them or choose a new version.'
        }
    }
    # Import the exact compiler environment from this build, never silently upgrade it.
    $compiler = [IO.Path]::GetFullPath($recipe.compiler)
    $vcRoot = [IO.DirectoryInfo](Split-Path -Parent $compiler)
    while ($vcRoot -and $vcRoot.Name -ne 'VC') { $vcRoot = $vcRoot.Parent }
    if (-not $vcRoot) { throw 'Cannot locate the configured MSVC toolchain.' }
    $toolset = [regex]::Match($compiler, 'MSVC[\\/]([^\\/]+)').Groups[1].Value
    $vcvars = Join-Path $vcRoot.FullName 'Auxiliary/Build/vcvars64.bat'
    foreach ($value in @($vcvars, $toolset)) { if ($value -match '["&|<>^%\r\n]') { throw 'Invalid toolchain path.' } }
    $savedEnvironment = @{}
    Get-ChildItem Env: | ForEach-Object { $savedEnvironment[$_.Name] = $_.Value }
    try {
        $lines = & cmd.exe /d /s /c "call `"$vcvars`" -vcvars_ver=$toolset >nul && set"
        if ($LASTEXITCODE -ne 0) { throw 'Compiler environment failed.' }
        foreach ($line in $lines) { if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') } }
        & $CMake --build $Build --config Release --target oneui oneui_static oneui-viewc --parallel 4
        if ($LASTEXITCODE -ne 0) { throw 'SDK compilation failed.' }
        if ((Test-Path -LiteralPath $work) -and -not (Test-Path -LiteralPath (Join-Path $work '.oneui-release-owned'))) {
            throw 'Release work directory is not owned by this pipeline.'
        }
        if ((Test-Path -LiteralPath $work) -and ((Get-Item -LiteralPath $work).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked work directory.' }
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        Set-Content -LiteralPath (Join-Path $work '.oneui-release-owned') -Value 'OneUI release pipeline' -Encoding utf8
        $frozen = Join-Path $work 'frozen'
        Remove-OwnedFixture $frozen $work
        Run-Python @((Join-Path $PSScriptRoot 'package-windows-sdk.py'), '--source', $Source, '--build', $Build,
            '--output', $frozen, '--licenses', $Licenses)
        Run-Python @((Join-Path $PSScriptRoot 'package-versioned-sdk.py'), '--source', $Source, '--input', $frozen,
            '--output', $Packages, '--licenses', (Join-Path $frozen 'licenses'), '--platform', 'windows',
            '--architecture', 'x86_64', '--profile', 'modern', '--toolchain', 'MSVC 19.50 / static CRT',
            '--minimum-os', 'Windows 10 x64', '--version', $Version)
        Run-Python @((Join-Path $PSScriptRoot 'test-versioned-sdk.py'), '--packages', $Packages,
            '--version', $Version, '--work', (Join-Path $work 'sdk-consumer-current'), '--target', 'windows-modern-x86_64',
            '--cmake', $CMake, '--report', (Join-Path $work 'consumer-report.json'))
    } finally {
        foreach ($name in @((Get-ChildItem Env:).Name)) {
            if (-not $savedEnvironment.ContainsKey($name)) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
        }
        foreach ($name in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
        if (Test-Path -LiteralPath (Join-Path $work '.oneui-release-owned')) {
            Remove-OwnedFixture (Join-Path $work 'frozen') $work
        }
    }
    # Include the fixed work directory even if it did not exist before this run.
    $postAuditArgs = @($auditTool, '--kind', 'build', '--max-gib', "$ApprovedBudgetGiB")
    foreach ($root in $roots) { if (Test-Path -LiteralPath $root) { $postAuditArgs += @('--root', $root) } }
    Run-Python $postAuditArgs
}
$publishArgs = @((Join-Path $PSScriptRoot 'publish-sdk.py'), '--config', $Config, '--packages', $Packages,
    '--version', $Version, '--targets', 'windows-modern-x86_64', '--report', $report)
if ($Publish) { $publishArgs += '--apply' }
Run-Python $publishArgs
