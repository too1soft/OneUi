param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Bootstrap,
    [Parameter(Mandatory=$true)][string]$CacheDirectory
)
$ErrorActionPreference = 'Stop'
$foundation = Split-Path -Parent $PSScriptRoot
$recipe = Get-Content -LiteralPath (Join-Path $foundation 'go-source.lock.json') -Raw | ConvertFrom-Json
$sourceRoot = (Resolve-Path -LiteralPath $Source).Path
$bootstrapRoot = (Resolve-Path -LiteralPath $Bootstrap).Path
$bootstrapExe = Join-Path $bootstrapRoot 'bin/go.exe'
if ((Get-FileHash -LiteralPath $bootstrapExe -Algorithm SHA256).Hash.ToLowerInvariant() -ne $recipe.bootstrapExeSha256) {
    throw 'Bootstrap executable does not match the reviewed lock.'
}
if (($sourceRoot -eq $bootstrapRoot) -or (Test-Path -LiteralPath (Join-Path $sourceRoot 'bin/go.exe'))) {
    throw 'Use a fresh source checkout, never an installed Go distribution.'
}
& python (Join-Path $PSScriptRoot 'prepare-go.py') --source $sourceRoot
if ($LASTEXITCODE -ne 0) { throw 'Source/patch validation failed.' }
$previous = @{}
foreach ($key in @('GOROOT','GOROOT_BOOTSTRAP','GOTOOLCHAIN','GOCACHE','GOOS','GOARCH','GOAMD64','GO386','CGO_ENABLED')) {
    $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
}
try {
    $env:GOROOT = $sourceRoot
    $env:GOROOT_BOOTSTRAP = $bootstrapRoot
    $env:GOTOOLCHAIN = 'local'
    $env:GOCACHE = [IO.Path]::GetFullPath($CacheDirectory)
    $env:GOOS = 'windows'
    $env:GOARCH = 'amd64'
    $env:GOAMD64 = 'v1'
    $env:GO386 = 'sse2'
    $env:CGO_ENABLED = '0'
    Push-Location (Join-Path $sourceRoot 'src')
    try {
        & cmd.exe /d /c make.bat
        if ($LASTEXITCODE -ne 0) { throw 'Patched Go bootstrap failed.' }
    } finally { Pop-Location }
    & (Join-Path $sourceRoot 'bin/go.exe') version
    if ($LASTEXITCODE -ne 0) { throw 'Built toolchain cannot start on build host.' }
} finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
}
Write-Host 'Built a candidate toolchain, NOT a Win7-certified product. Run native acceptance before publishing.'
